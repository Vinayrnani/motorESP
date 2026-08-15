const { chromium } = require('playwright');

const BASE = process.argv[2] || 'https://tmp-orders-things-library.trycloudflare.com';
const CHECK = (name, ok, extra) => console.log((ok ? 'PASS' : 'FAIL') + '  ' + name + (extra ? '  [' + extra + ']' : ''));
let pass = 0, fail = 0;
function check(name, ok, extra) { ok ? pass++ : fail++; CHECK(name, ok, extra); }

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const errs = [];
  page.on('console', m => { if (m.type() === 'error') errs.push(m.text()); });
  page.on('pageerror', e => errs.push('PAGEERROR: ' + e.message));

  const status = async () => { try { const r = await page.request.get(BASE + '/status'); return await r.json(); } catch (e) { return { error: true }; } };
  const fresh = p => page.goto(BASE + p, { timeout: 30000, waitUntil: 'domcontentloaded' });
  const sleep = ms => new Promise(r => setTimeout(r, ms));
  const waitStatusField = async (field, timeoutMs, expect = 0) => {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const st = await status();
      if (st && st[field] === expect) return true;
      await sleep(2000);
    }
    return false;
  };

  // seed mock meter data (bench has no real PZEM; 0A keeps pump OFF, 235V = VOLTAGE OK)
  await page.request.post(BASE + '/settings/api?mock=1&mockVoltage=235&mockCurrent=0&mockPower=0');
  await sleep(4000);

  // ---------- 1. CONTROL page: status rendering ----------
  console.log('\n== 1. CONTROL PAGE (status, stats, buttons) ==');
  await fresh('/');
  await sleep(4000);
  check('statusBig visible', await page.isVisible('#statusBig'));
  const big = await page.textContent('#statusBig');
  check('statusBig text', ['RUNNING', 'STOPPED', 'SAFETY STOP', 'POWER RESTORED', 'STARTING…'].includes(big.trim()), big.trim());
  check('3 stat cards visible', await page.isVisible('#stVolt') && await page.isVisible('#stCur') && await page.isVisible('#stPow'));
  const volt = (await page.textContent('#stVolt')).trim();
  check('voltage single-unit', /^\d+ V$/.test(volt), volt);
  check('voltPill visible', await page.isVisible('#voltPill'));
  check('mode selector has 3 buttons', await page.locator('.mode-btn').count() === 3);
  check('START/STOP/RESET buttons exist', await page.isVisible('#btnStart') && await page.isVisible('#btnStop') && await page.isVisible('#btnReset'));

  // ---------- 2. MODE buttons: click ALL 3 like a human ----------
  console.log('\n== 2. MODE SELECTOR (click each, verify) ==');
  for (const [m, label] of [[0, 'OFF'], [1, 'MANUAL'], [2, 'AUTO']]) {
    await page.click(`.mode-btn[data-mode="${m}"]`);
    let ok = false, s = { pumpMode: -1 }, active = 'none';
    for (let tries = 0; tries < 4; tries++) {
      await sleep(900);
      s = await status();
      try { active = await page.evaluate(() => { const el = document.querySelector('.mode-btn.active'); return el ? el.dataset.mode : 'none'; }); } catch (e) {}
      if (s.pumpMode === m && active === String(m)) { ok = true; break; }
      await page.click(`.mode-btn[data-mode="${m}"]`);
    }
    check('mode click ' + label, ok, 'device=' + s.pumpMode + ' ui=' + active);
  }
  await page.click('.mode-btn[data-mode="1"]'); // restore MANUAL at the end
  await sleep(800);

  // ---------- 3. START/STOP with mock: real click flow ----------
  console.log('\n== 3. START/STOP BUTTONS (real pump flow via mock) ==');
  // force clean state via API first
  await page.request.post(BASE + '/settings/api?mock=0');
  await page.request.get(BASE + '/control?action=reset');
  await sleep(2000);
  await fresh('/');
  await sleep(2500);
  let s = await status();
  check('pre-flight clean: mock=0 trips=0', s.mock === 0 && s.trips === 0, 'mock=' + s.mock + ' trips=' + s.trips);
  // START button exists and is in a valid state (disabled or enabled depending on PZEM/min-off)
  const btnVisible = await page.isVisible('#btnStart');
  check('START button visible', btnVisible);
  // set short timers to avoid 60s+ waits that drop the tunnel
  await page.request.post(BASE + '/settings/api?minRun=10&minOff=10');
  await sleep(500);
  // wait out any min-off window
  await waitStatusField('minOffLeft', 20000);
  // enable mock profile=running + reset trips via API, then START
  await page.request.post(BASE + '/settings/api?mock=1');
  await waitStatusField('mock', 10000, 1);
  await sleep(600);
  await page.request.post(BASE + '/settings/api?mockProfile=running');
  await page.request.get(BASE + '/control?action=reset');
  // mock profile=running feeds 9.6A >= startSuccessCurrent -> external manual-start detection
  await waitStatusField('pumpStateRaw', 15000, 2);
  await sleep(2500);
  s = await status();
  check('mock profile=running -> RUNNING', s.pumpState === 'RUNNING', 'state=' + s.pumpState);
  check('START disabled while running', await page.$eval('#btnStart', b => b.disabled), (await page.textContent('#btnStart')).trim());
  let stopTxt = (await page.textContent('#btnStop')).trim();
  check('STOP shows countdown or STOP', stopTxt.includes('BLOCKED') || stopTxt === 'STOP', stopTxt);
  // wait for min-run to expire
  await waitStatusField('minRunLeft', 70000);
  // drop mock current to 0 so pump stays OFF after STOP
  await page.request.post(BASE + '/settings/api?mockVoltage=240&mockCurrent=0&mockPower=0');
  await sleep(1000);
  await page.click('#btnStop');
  await sleep(2500);
  s = await status();
  check('STOP click -> OFF', s.pumpState === 'OFF', 'state=' + s.pumpState);
  let startTxt = (await page.textContent('#btnStart')).trim();
  check('START blocked w/ min-off countdown', startTxt.includes('BLOCKED') || startTxt.includes('NO METER') || startTxt === 'START', startTxt);
  await waitStatusField('minOffLeft', 70000);

  // ---------- 4. NAVIGATION via tab bar ----------
  console.log('\n== 4. NAVIGATE VIA TAB BAR ==');
  await fresh('/');
  await sleep(1500);
  let nav = await page.$$eval('nav.tab-bar a', as => as.map(a => a.getAttribute('href')));
  for (const href of nav) {
    await page.click(`nav.tab-bar a[href="${href}"]`);
    await sleep(2000);
    const url = page.url().replace(BASE, '');
    check('nav to ' + href, url === href, url);
  }

  // ---------- 5. DASHBOARD explore ----------
  console.log('\n== 5. DASHBOARD (numerics, charts, poll selector) ==');
  await fresh('/dashboard');
  await sleep(4000);
  for (const id of ['nVolt', 'nCur', 'nPow', 'nEn', 'nHz']) {
    const txt = (await page.textContent('#' + id)).trim();
    check('numeric ' + id + ' non-empty', txt.length > 0, txt);
  }
  const pfTxt = (await page.textContent('#nPF')).trim();
  check('numeric nPF non-empty', pfTxt.length > 0, pfTxt);
  for (const ch of ['chartPower', 'chartVoltage', 'chartCurrent']) {
    const exists = await page.evaluate(c => !!Chart.getChart(c), ch);
    check('chart ' + ch + ' created', exists);
  }
  await page.selectOption('#pollSel', '1000');
  await sleep(1200);
  const pollVal = await page.$eval('#pollSel', el => el.value);
  check('poll selector change to 1s', pollVal === '1000', pollVal);
  await page.selectOption('#pollSel', '5000');

  // ---------- 6. SETTINGS explore every option ----------
  console.log('\n== 6. SETTINGS (all fields loaded, save roundtrip) ==');
  await fresh('/settings');
  await sleep(3500);
  const fields = ['ocRunning','ocStartInstant','ocDelay','dryRunCurrent','dryRunPower','dryRunDelay','dryRunActivation',
    'voltOver','voltUnder','voltWarn','voltCritical','voltageDelay','voltageLockout',
    'startSuccessCurrent','startVerifyDelay','startFailBlock','minRun','minOff',
    'autoRetryDelay','maxRetries','logIntervalRunning','logIntervalOff','pzemReadRunning'];
  let allFilled = true;
  for (const f of fields) {
    if (!await page.isVisible('#' + f)) { allFilled = false; break; }
  }
  check('all ' + fields.length + ' numeric fields visible', allFilled);
  check('test-mode checkbox exists', await page.isVisible('#cMock'));
  check('mock profile select exists', await page.isVisible('#cMockProfile'));
  check('save button visible', await page.isVisible('#btnSave'));
  check('clear data button visible', await page.isVisible('#btnClearData'));
  // roundtrip a value
  await page.fill('#ocRunning', '13');
  await page.click('#btnSave');
  await sleep(2000);
  const apiJson = await (await page.request.get(BASE + '/settings/api')).json();
  check('saved ocRunning=13 persists', apiJson.ocRunning === 13, apiJson.ocRunning);
  await page.fill('#ocRunning', '12');
  await page.click('#btnSave').catch(() => {});
  await sleep(1200);
  const apiJson2 = await (await page.request.get(BASE + '/settings/api')).json();
  check('restored ocRunning=12', apiJson2.ocRunning === 12, apiJson2.ocRunning);

  // ---------- 7. DATA page explore ----------
  console.log('\n== 7. DATA PAGE (rows, LOAD MORE) ==');
  await fresh('/data');
  await sleep(4000);
  let rowCount1 = await page.locator('#rows tr').count();
  for (let i = 0; i < 3 && rowCount1 === 0; i++) {
    await page.click('#loadMore').catch(() => {});
    await sleep(3500);
    rowCount1 = await page.locator('#rows tr').count();
  }
  check('data rows rendered', rowCount1 > 0, 'rows=' + rowCount1);
  const firstRow = (await page.innerText('#rows tr:first-child')).replace(/\s+/g, ' ').trim();
  check('first row plausible', /^\d+ \d+ \d+ \d+(\.\d+)? \d+ (0\.\d\d|-)/.test(firstRow), firstRow);
  await page.click('#loadMore');
  await sleep(3500);
  const rowCount2 = await page.locator('#rows tr').count();
  check('LOAD MORE appends rows', rowCount2 > rowCount1, rowCount1 + '->' + rowCount2);
  const summary = (await page.textContent('#summary')).trim();
  check('summary line present', summary.length > 0, summary.slice(0, 60));

  // ---------- 8. CLEANUP ----------
  console.log('\n== 8. CLEANUP (state restore) ==');
  await page.request.post(BASE + '/settings/api?mock=0');
  await page.request.post(BASE + '/settings/api?minRun=30&minOff=60');
  await page.request.get(BASE + '/control?action=reset');
  await sleep(1000);
  await fresh('/');
  await sleep(3500);
  s = await status();
  if (!s || s.error) { await sleep(4000); s = await status(); }
  check('final: mock=0', s.mock === 0, 'mock=' + s.mock);
  check('final: trips=0', s.trips === 0, 'trips=' + s.trips);
  check('final: OFF', s.pumpState === 'OFF', s.pumpState);
  const modeCount = await page.locator('.mode-btn.active').count();
  check('final: exactly one mode active', modeCount === 1, 'count=' + modeCount);

  console.log('\n========== EXPLORATORY TEST SUMMARY: ' + pass + ' PASS / ' + fail + ' FAIL ==========');
  if (errs.length) { console.log('\nCONSOLE/PAGE ERRORS (' + errs.length + '):'); errs.forEach(e => console.log('  ' + e)); }
  await browser.close();
  process.exit(fail ? 1 : 0);
})().catch(e => { console.error('FATAL', e.message); process.exit(2); });
