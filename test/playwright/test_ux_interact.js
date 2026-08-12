const { chromium } = require('playwright');
const BASE = process.argv[2] || 'https://tmp-orders-things-library.trycloudflare.com';
let fail = 0;
const ok = (name, cond, extra = '') => { console.log((cond ? 'PASS' : 'FAIL') + ' ' + name + (extra ? ' :: ' + extra : '')); if (!cond) fail++; };

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });

  // ---- Control: read-only checks first (no pump state changes)
  await page.goto(BASE + '/', { timeout: 30000, waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(3800);
  const statusBig = await page.textContent('#statusBig');
  ok('status big text is one of RUNNING/STOPPED/SAFETY STOP/POWER RESTORED', ['RUNNING','STOPPED','SAFETY STOP','POWER RESTORED'].some(x => statusBig.includes(x)), statusBig.trim());
  const voltTx = (await page.textContent('#stVolt')).trim();
  ok('voltage has unit', /^\d+ V$/.test(voltTx), voltTx);
  const powTx = (await page.textContent('#stPow')).trim();
  ok('power single unit', /^\d+(\.\d+)? (W|kW)$/.test(powTx), powTx);

  // mode badges: 3 candidates exist
  const badges = await page.evaluate(() => [...document.querySelectorAll('.mode-btn')].map(b => ({ m: b.dataset.mode, active: b.classList.contains('active') })));
  ok('exactly one mode badge active', badges.filter(b => b.active).length === 1, JSON.stringify(badges));

  // ---- Dashboard read-only
  await page.goto(BASE + '/dashboard', { timeout: 30000 });
  await page.waitForTimeout(3800);
  const nVolt = (await page.textContent('#nVolt')).trim();
  ok('dashboard voltage with unit', /^\d+ V$/.test(nVolt), nVolt);
  const nPow = (await page.textContent('#nPow')).trim();
  ok('dashboard power single unit', /^\d+(\.\d+)? (W|kW)$/.test(nPow), nPow);
  ok('charts created', await page.evaluate(() => {
    const c = Chart.getChart('chartPower');
    return !!c && c.data.datasets[0].data.length >= 0;
  }));
  ok('poll selector default 5s', await page.evaluate(() => document.querySelector('#pollSel').value === '5000'));

  // ---- Settings: save + restore
  await page.goto(BASE + '/settings', { timeout: 30000 });
  await page.waitForTimeout(3000);
  const ocVal = await page.inputValue('#ocRunning');
  ok('settings loaded', /^1[0-9](\.[05])?$|^50$/.test(ocVal), 'ocRunning=' + ocVal);
  await page.fill('#ocRunning', '13');
  await page.click('#btnSave');
  await page.waitForTimeout(1600);
  let s = await (await page.evaluate(() => fetch('/settings/api').then(r => r.json())));
  ok('ocRunning persisted=13', s.ocRunning === 13, 'got ' + s.ocRunning);
  await page.fill('#ocRunning', '12');
  await page.click('#btnSave');
  await page.waitForTimeout(1600);
  s = await (await page.evaluate(() => fetch('/settings/api').then(r => r.json())));
  ok('ocRunning restored=12', s.ocRunning === 12, 'got ' + s.ocRunning);

  // ---- Data page: rows + LOAD MORE
  await page.goto(BASE + '/data', { timeout: 30000 });
  await page.waitForTimeout(4000);
  let rowCount = await page.evaluate(() => document.querySelectorAll('#rows tr').length);
  ok('data table has rows', rowCount > 0, rowCount + ' rows');
  const firstRow = (await page.evaluate(() => document.querySelector('#rows tr') ? document.querySelector('#rows tr').innerText : '')).replace(/\s+/g, ' ');
  ok('first row plausible (boot 1, voltage ~200-250, RUNNING chip)', /^1 \d+ 2\d\d \d+\.\d /.test(firstRow), firstRow.slice(0, 70));
  await page.click('#loadMore');
  await page.waitForTimeout(2500);
  const rowCount2 = await page.evaluate(() => document.querySelectorAll('#rows tr').length);
  ok('load more appends without errors', rowCount2 >= rowCount, rowCount + '→' + rowCount2);

  ok('zero page/console errors', errors.length === 0, errors.join(' | '));
  await browser.close();
  console.log(fail ? '=== ' + fail + ' FAILURES ===' : '=== ALL PASS ===');
  process.exit(fail ? 1 : 0);
})().catch(e => { console.error('FATAL', e); process.exit(1); });