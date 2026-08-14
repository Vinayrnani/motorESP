#!/usr/bin/env node
/*
 * agent_explore.js — LLM-driven exploratory browser agent for motorESP web UI
 *
 * Usage:
 *   node agent_explore.js <BASE_URL>
 *
 * LLM backend (default: local opencode CLI, free model):
 *   AGENT_OPENCODE  (default ~/.opencode/bin/opencode)
 *   AGENT_LLM_MODEL (optional, passed as -m to opencode)
 *   AGENT_MAX_STEPS (default 30)
 *
 * Fallback HTTP backend (only if opencode CLI missing), via env or agent_config.json:
 *   AGENT_LLM_BASE_URL (default https://api.openai.com/v1)
 *   AGENT_LLM_MODEL    (default gpt-4o-mini)
 *   AGENT_LLM_API_KEY  (required for fallback)
 */
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
const { execFile } = require('child_process');
const util = require('util');
const execFileP = util.promisify(execFile);

const BASE = process.argv[2] || 'https://tmp-orders-things-library.trycloudflare.com';
const MAX_STEPS = parseInt(process.env.AGENT_MAX_STEPS || '30', 10);
const OPENCODE = process.env.AGENT_OPENCODE || (process.env.HOME || '/home/ubuntu') + '/.opencode/bin/opencode';

if (process.env.AGENT_LOG) {
  const w = fs.createWriteStream(process.env.AGENT_LOG, { flags: 'a' });
  const orig = console.log;
  console.log = (...a) => { w.write(a.join(' ') + '\n'); orig(...a); };
  console.error = (...a) => { w.write('ERR ' + a.join(' ') + '\n'); orig(...a); };
}

function loadCfg() {
  const cfg = { baseUrl: process.env.AGENT_LLM_BASE_URL || 'https://api.openai.com/v1',
                model: process.env.AGENT_LLM_MODEL || 'opencode/big-pickle',
                apiKey: process.env.AGENT_LLM_API_KEY || '' };
  try {
    const f = path.join(__dirname, 'agent_config.json');
    if (fs.existsSync(f)) Object.assign(cfg, JSON.parse(fs.readFileSync(f, 'utf8')));
  } catch (e) {}
  return cfg;
}

const LLM_TIMEOUT = parseInt(process.env.AGENT_LLM_TIMEOUT || '600000', 10);
const LLM_RETRIES = parseInt(process.env.AGENT_LLM_RETRIES || '2', 10);

function runOpencode(args) {
  return new Promise((resolve, reject) => {
    const { spawn } = require('child_process');
    const os = require('os');
    const outFile = path.join(os.tmpdir(), 'oc_' + process.pid + '_' + Date.now() + '.out');
    const fd = fs.openSync(outFile, 'w');
    const child = spawn(OPENCODE, args, { stdio: ['ignore', fd, fd], detached: true });
    const timer = setTimeout(() => {
      try { process.kill(-child.pid, 'SIGTERM'); } catch (e) {}
      fs.closeSync(fd);
      reject(Object.assign(new Error('opencode timeout after ' + LLM_TIMEOUT + 'ms (killed)'), { code: 'TIMEOUT', killed: true }));
    }, LLM_TIMEOUT);
    child.on('error', (e) => { clearTimeout(timer); try { fs.closeSync(fd); } catch (e2) {} reject(e); });
    child.on('exit', (code, sig) => {
      clearTimeout(timer);
      try { fs.closeSync(fd); } catch (e) {}
      const stdout = fs.readFileSync(outFile, 'utf8');
      try { fs.unlinkSync(outFile); } catch (e) {}
      if (code !== 0) {
        const err = new Error('opencode exited ' + code + ' sig ' + sig);
        err.code = code; err.signal = sig; err.stderr = stdout.slice(0, 500);
        reject(err);
      } else resolve({ stdout });
    });
  });
}

async function llmChat(cfg, messages) {
  const prompt = messages.map(m => '[' + m.role.toUpperCase() + ']\n' + m.content).join('\n\n');
  const args = ['run', '--pure', '--format', 'json'];
  if (cfg.model) args.push('-m', cfg.model);
  for (let attempt = 0; attempt <= LLM_RETRIES; attempt++) {
    const t0 = Date.now();
    try {
      const { stdout } = await runOpencode(args.concat([prompt]));
      let reply = '';
      for (const line of stdout.split('\n')) {
        if (!line.trim()) continue;
        try {
          const ev = JSON.parse(line);
          if (ev.type === 'text' && ev.part && ev.part.type === 'text' && ev.part.text) {
            reply = ev.part.text;
          }
        } catch (e) {}
      }
      if (reply.trim()) {
        console.log('[llmChat] ok in', ((Date.now() - t0) / 1000).toFixed(1) + 's');
        return reply.trim();
      }
      throw new Error('opencode returned empty reply');
    } catch (e) {
      console.error('[llmChat] attempt', attempt + 1, 'failed after', ((Date.now() - t0) / 1000).toFixed(1) + 's',
        JSON.stringify({ code: e.code, killed: e.killed, msg: (e.message || '').slice(0, 80) }));
      if (e.code === 'ENOENT') {
        if (!cfg.apiKey) throw new Error('opencode CLI not found at ' + OPENCODE + ' and no AGENT_LLM_API_KEY for fallback');
        const resp = await fetch(cfg.baseUrl + '/chat/completions', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', Authorization: 'Bearer ' + cfg.apiKey },
          body: JSON.stringify({ model: cfg.model || 'gpt-4o-mini', messages, max_tokens: 700, temperature: 0.4 }),
          signal: AbortSignal.timeout(60000)
        });
        if (!resp.ok) {
          const body = await resp.text().catch(() => '');
          throw new Error('LLM HTTP ' + resp.status + ' ' + body.slice(0, 300));
        }
        const data = await resp.json();
        return data.choices[0].message.content;
      }
      if (e.message.includes('empty reply') && attempt < LLM_RETRIES) continue;
      if (attempt >= LLM_RETRIES) throw e;
    }
  }
  throw new Error('llmChat exhausted retries');
}

async function extractSnapshot(page) {
  return await page.evaluate(() => {
    const out = { url: location.pathname, interactive: [], text: [], state: {} };
    const seen = new Set();
    const hidden = (el) => {
      if (el.closest && el.closest('.sheet') && !el.closest('.sheet').classList.contains('open')) return true;
      for (let n = el; n; n = n.parentElement) {
        const cs = getComputedStyle(n);
        if (cs.display === 'none' || cs.visibility === 'hidden') return true;
      }
      return false;
    };
    document.querySelectorAll('button, a[href], input, select, textarea, .seg button').forEach(el => {
      if (hidden(el)) return;
      let label = (el.getAttribute('aria-label') || el.innerText || el.value || el.getAttribute('href') || '').trim().slice(0, 60);
      const lblEl = el.id ? document.querySelector('label[for="' + el.id + '"]') : null;
      if (lblEl && lblEl.innerText.trim()) label = lblEl.innerText.trim().slice(0, 60) + ' [' + (el.value || '') + ']';
      if (!label) return;
      const key = el.tagName + ':' + label;
      if (seen.has(key)) return;
      seen.add(key);
      const disabled = el.disabled || el.classList.contains('disabled');
      const extra = el.tagName === 'SELECT' ? ' options:' + Array.from(el.options).map(o => o.textContent.trim()).join('|').slice(0, 120) : '';
      out.interactive.push((el.id ? '#' + el.id : el.tagName) + ' "' + label + '"' + extra + (disabled ? ' [DISABLED]' : ''));
    });
    document.querySelectorAll('#statusBig, .status-hero, #stateExpl, #summary, #loadErr, #loadOk, #lastUpd, #msg, #toast, #moreHint, #meterHint, #reasonPanel, .num-card, .hint, #stateBadge, #tripBadge, #voltageStatus, #subLine, #statusPlain').forEach(el => {
      if (hidden(el)) return;
      const t = (el.innerText || '').trim().slice(0, 200);
      if (t) out.text.push(el.id ? '#' + el.id + ': ' + t : t);
    });
    return out;
  });
}

const SYSTEM = `You are an exploratory UI/UX tester for "motorESP", an ESP8266 submersible pump controller web app.
Act like a curious human user. Explore all pages, click buttons, check that statuses make sense, look for UX issues:
confusing text, redundant messages, broken states, dead buttons, missing feedback, layout problems.

You receive a page snapshot: current URL, interactive elements (buttons/links/inputs, [DISABLED] markers), and main text.
Reply ONLY with a JSON object, no prose:
{"action":"click|type|select|goto|scroll|done","target":"<css selector or path>","value":"<text for type/select>","finding":"<UX issue found so far, or ''>","report":"<final report when done, or ''>"}

Rules:
- Prefer element IDs or tag+label for target.
- To navigate: {"action":"goto","target":"/dashboard"}.
- The app has these pages: / (control), /dashboard, /settings, /data. Explore ALL of them.
- A status of "SAFETY STOP" or trips>0 is normal (device may be in test mode); report how well it is explained, not as a device bug.
- Page semantics: TEST MODE / #qTest just toggles a banner (device reports anyway) and needs no repeated clicks — clicking it multiple times adds no info. {SAFETY STOP/RESET NEEDED} means a protection tripped; RESET unlocks, START runs the pump (only when meter valid). LOAD MORE appends 100 rows and updates the "shown" counter (turns into "All loaded" at the end). 'last update' under the uptime row is the dashboard's live-refresh heartbeat. The QUICK ACTIONS sheet (#btnMore) must be OPEN first — TEST MODE/ERASE LOG/REBOOT buttons are inert while the sheet is closed (by design).
- Do NOT click toggles/destructives more than once, and do not revisit pages you already audited. One fresh look at each page is enough.
- When you have explored enough (~12-20 actions), reply {"action":"done","report":"<full human-readable findings report>"}.
- If you are at action 15-20, STOP exploring and reply with "done" + your report — a report is better than extra clicks. Stay under 14 actions per run.`;

(async () => {
  const cfg = loadCfg();
  if (!fs.existsSync(OPENCODE) && !cfg.apiKey) {
    console.error('ERROR: opencode CLI not found at ' + OPENCODE + ' and no AGENT_LLM_API_KEY for fallback.');
    console.error('Either set AGENT_OPENCODE to the opencode binary path, or create test/playwright/agent_config.json:');
    console.error('  {"baseUrl":"https://api.openai.com/v1","model":"gpt-4o-mini","apiKey":"sk-..."}');
    process.exit(2);
  }
  console.log('BASE', BASE, '| LLM backend: opencode CLI (' + OPENCODE + ')' + (cfg.model ? ' model ' + cfg.model : ''));

  let browser = await chromium.launch({ headless: true });
  let page = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const consoleErrors = [], pageErrors = [], failedReqs = [];
  const attachListeners = (p) => {
    p.on('console', m => { if (m.type() === 'error') consoleErrors.push(m.text().slice(0, 200)); });
    p.on('pageerror', e => pageErrors.push(e.message.slice(0, 200)));
    p.on('requestfailed', r => failedReqs.push(r.url().slice(0, 120)));
  };
  attachListeners(page);

  async function ensurePage() {
    if (!page.isClosed() && browser.isConnected()) return;
    console.log('[recover] browser/page died, relaunching browser');
    await browser.close().catch(() => {});
    browser = await chromium.launch({ headless: true });
    page = await browser.newPage({ viewport: { width: 390, height: 844 } });
    attachListeners(page);
    await page.goto(BASE + '/', { timeout: 30000, waitUntil: 'domcontentloaded' }).catch(() => {});
    await page.waitForTimeout(2000);
  }

  await page.goto(BASE + '/', { timeout: 30000, waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(2500);

  const history = [];
  for (let step = 1; step <= MAX_STEPS; step++) {
    await ensurePage();
    const snap = await extractSnapshot(page);
    const userMsg = 'Step ' + step + '.\n' + JSON.stringify(snap) + '\n' +
      (history.length ? '\nHistory (last 6):\n' + history.slice(-6).join('\n') : '\nNo history yet.');
    let decision;
    for (let attempt = 0; attempt < 3; attempt++) {
      try {
        const raw = await llmChat(cfg, [{ role: 'system', content: SYSTEM }, { role: 'user', content: userMsg }]);
        decision = JSON.parse(raw.replace(/```json|```/g, '').trim());
        break;
      } catch (e) {
        if (attempt === 2) throw e;
        await new Promise(r => setTimeout(r, 1500));
      }
    }
    const act = decision.action || 'done';
    console.log('[step', step, '] action:', act, JSON.stringify(decision.target || ''), 'value:', JSON.stringify(decision.value || ''));
    try {
      if (act === 'click') await page.click(decision.target, { timeout: 4000 });
      else if (act === 'type') await page.fill(decision.target, decision.value || '', { timeout: 4000 });
      else if (act === 'select') await page.selectOption(decision.target, decision.value || '', { timeout: 4000 });
      else if (act === 'goto') await page.goto(BASE + decision.target, { timeout: 30000 });
      else if (act === 'scroll') await page.mouse.wheel(0, 600);
      else if (act === 'done') {
        console.log('\n================= AGENT REPORT =================\n');
        console.log(decision.report || '(no report)');
        console.log('\n=================================================');
        break;
      }
      await page.waitForTimeout(2000);
      history.push('STEP ' + step + ' [' + act + ' ' + decision.target + '] -> ' + (decision.finding || '') + ' | ' + snap.url);
    } catch (e) {
      history.push('STEP ' + step + ' [' + act + ' ' + decision.target + '] FAILED: ' + e.message.slice(0, 120));
      await page.screenshot({ path: '/tmp/opencode/agent_fail_' + step + '.png' }).catch(() => {});
    }
    if (step === MAX_STEPS) {
      console.log('\n================= AGENT REPORT (ran out of steps) =================\n');
      console.log('(agent did not finish; last state: ' + snap.url + ')');
    }
  }

  console.log('\nConsole errors:', consoleErrors.length ? consoleErrors : 'none');
  console.log('Page errors:', pageErrors.length ? pageErrors : 'none');
  console.log('Failed requests:', failedReqs.length ? failedReqs : 'none');
  await browser.close();
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });