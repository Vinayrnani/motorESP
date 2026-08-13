const { chromium } = require('playwright');
const BASE = process.argv[2] || 'https://tmp-orders-things-library.trycloudflare.com';
const pass = [], fail = [], info = [];
function check(name, cond, extra) {
  (cond ? pass : fail).push(name + (extra ? ' :: ' + extra : ''));
}
const api = async (p, path) => {
  for (let i = 0; i < 5; i++) {
    const r = await p.goto(BASE + path, { timeout: 30000 });
    if (r.status() === 200) { await p.waitForTimeout(800); return 200; }
    await p.waitForTimeout(2500);
  }
  return 0;
};
const waitStatus = async (p, pred, tries = 12) => {
  for (let i = 0; i < tries; i++) {
    try { const s = await p.evaluate(async () => await (await fetch('/status')).json()); if (pred(s)) return s; } catch (e) {}
    await p.waitForTimeout(1500);
  }
  return null;
};

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });

  const readUI = () => page.evaluate(() => ({
    startDisabled: document.getElementById('btnStart').disabled,
    startText: document.getElementById('btnStart').firstChild.textContent.trim(),
    startCnt: document.getElementById('btnStart').querySelector('.cnt').textContent,
    stopDisabled: document.getElementById('btnStop').disabled,
    stopText: document.getElementById('btnStop').firstChild.textContent.trim(),
    stopCnt: document.getElementById('btnStop').querySelector('.cnt').textContent,
    resetDisabled: document.getElementById('btnReset').disabled,
    statusBig: document.getElementById('statusBig').textContent,
    banner: document.getElementById('testBanner').style.display,
  }));

  // 1. mock off + pump off -> START disabled w/ NO METER (no pzem), STOP disabled, RESET disabled
  await api(page, '/settings/api?mock=0');
  await api(page, '/control?action=reset');
  await page.goto(BASE + '/', { timeout: 30000 }); await page.waitForTimeout(3500);
  let s = await readUI();
  check('mock-off: banner hidden', s.banner === 'none', s.banner);
  check('mock-off: START NO METER disabled', s.startDisabled && s.startText === 'NO METER', JSON.stringify(s));
  check('mock-off: STOP disabled', s.stopDisabled, JSON.stringify(s));
  check('mock-off: RESET disabled', s.resetDisabled, JSON.stringify(s));

  // 2. mock running -> external manual-start detection should drive pump to RUNNING (current>=2A)
  await api(page, '/settings/api?mock=1&mockProfile=running&pumpMode=1');
  const sRun = await waitStatus(page, s => s.pumpState === 'RUNNING', 15);
  check('mock running: pump reaches RUNNING', !!sRun, JSON.stringify(sRun));
  if (sRun) {
    await page.goto(BASE + '/', { timeout: 30000 }); await page.waitForTimeout(3000);
    s = await readUI();
    check('mock running: START disabled RUNNING', s.startDisabled && s.startText === 'RUNNING', JSON.stringify(s));
    check('min-run: STOP disabled w/ countdown', s.stopDisabled && /^\d+m \d+s$|^\d+s$/.test(s.stopCnt), JSON.stringify(s));
  }

  // 4. wait until min-run (30s) elapsed while RUNNING -> STOP enabled
  const sReady = await waitStatus(page, s => s.pumpState === 'RUNNING' && s.minRunLeft === 0, 25);
  check('min-run elapsed (RUNNING, minRunLeft 0)', !!sReady, JSON.stringify(sReady));
  await page.goto(BASE + '/', { timeout: 30000 }); await page.waitForTimeout(3000);
  s = await readUI();
  check('after min-run: STOP enabled', !s.stopDisabled && s.stopText === 'STOP', JSON.stringify(s));

  // 5. force current to 0A (mockVoltage override) so external-start detection stays quiet, then STOP -> START blocked by min-off countdown (60s)
  await api(page, '/settings/api?mock=1&mockVoltage=240&mockCurrent=0&mockPower=0');
  await api(page, '/control?action=stop');
  const sOff = await waitStatus(page, s => s.pumpState === 'OFF', 10);
  check('stop issued: pump OFF', !!sOff, JSON.stringify(sOff));
  await page.goto(BASE + '/', { timeout: 30000 }); await page.waitForTimeout(4000);
  s = await readUI();
  check('stopped: START blocked w/ countdown (min-off)', s.startDisabled && /^\d+m \d+s$|^\d+s$/.test(s.startCnt) && s.startText !== 'NO METER', JSON.stringify(s));
  check('stopped: STOP shows STOPPED disabled', s.stopDisabled && s.stopText === 'STOPPED', JSON.stringify(s));

  // 6. force trip to check RESET enabled
  await api(page, '/settings/api?mock=1&mockProfile=oc');
  await api(page, '/control?action=start');
  const sTrip = await waitStatus(page, s => s.pumpState === 'TRIPPED', 12);
  check('trip: pump TRIPPED', !!sTrip, JSON.stringify(sTrip));
  if (sTrip) {
    await page.goto(BASE + '/', { timeout: 30000 }); await page.waitForTimeout(3000);
    s = await readUI();
    check('trip: status SAFETY STOP', s.statusBig === 'SAFETY STOP', s.statusBig);
    check('trip: RESET enabled', !s.resetDisabled, JSON.stringify(s));
    check('trip: START shows RESET FIRST', s.startDisabled && s.startText === 'RESET FIRST', JSON.stringify(s));
  }

  // 7. mock off FIRST (avoid external-start re-trip race), then reset -> clean state
  await api(page, '/settings/api?mock=0');
  await api(page, '/control?action=reset');
  await page.goto(BASE + '/', { timeout: 30000 }); await page.waitForTimeout(3500);
  s = await readUI();
  const sFinal = await waitStatus(page, s => s.trips === 0 && !s.mock, 8);
  check('post-reset: clean (trips 0, mock 0)', !!sFinal, JSON.stringify(sFinal));
  check('post-reset: RESET disabled again', s.resetDisabled, JSON.stringify(s));

  check('zero page JS errors', errors.length === 0, errors.join('|'));

  console.log('\nRESULT: ' + pass.length + ' pass, ' + fail.length + ' fail');
  console.log('INFO:', JSON.stringify(info), 'PASS:');
  pass.forEach(p => console.log('  ✓ ' + p));
  if (fail.length) { console.log('FAIL:'); fail.forEach(f => console.log('  ✗ ' + f)); process.exitCode = 1; }
  await browser.close();
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });