const { chromium } = require('playwright');
const BASE = process.argv[2] || 'https://importantly-tune-carnival-dimensional.trycloudflare.com';
(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 390, height: 844 } });
  const errs = [];
  page.on('pageerror', e => errs.push(e.message));
  let ok = 0, fail = 0;
  const check = (name, cond) => { console.log((cond ? 'PASS ' : 'FAIL ') + name); cond ? ok++ : fail++; };

  await page.goto(BASE + '/settings', { timeout: 30000, waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(2500);
  const hints = await page.$$eval('.range-hint', els => els.map(e => e.textContent));
  check('settings: ocRunning range hint', hints.some(h => h.includes('5') && h.includes('50')));
  check('settings: hint count >= 15', hints.length >= 15);
  const firstHint = await page.$eval('#ocRunning', el => el.closest('.field').querySelector('label').textContent);
  check('settings: hint inside label', /range 5–50/.test(firstHint));

  await page.goto(BASE + '/data', { timeout: 30000, waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(4000);
  let sum = await page.$eval('#summary', el => el.textContent);
  check('data: summary has ~pct', /~?\d+% of archive loaded/.test(sum) || /end of archive/.test(sum));
  console.log('   summary:', sum);

  await page.goto(BASE + '/dashboard', { timeout: 30000, waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(5000);
  let lastUpdBefore = await page.$eval('#lastUpd', el => el.textContent);
  if (/retrying/.test(lastUpdBefore)) {
    await page.reload({ timeout: 30000 });
    await page.waitForTimeout(6000);
    lastUpdBefore = await page.$eval('#lastUpd', el => el.textContent);
  }
  check('dash: heartbeat fresh', !/STALE/.test(lastUpdBefore));
  await page.route('**/status', r => r.abort());
  await page.waitForTimeout(8000);
  const lastUpdAfter = await page.$eval('#lastUpd', el => el.textContent);
  check('dash: stale marker shown', /⚠ STALE DATA/.test(lastUpdAfter) && /may be old/.test(lastUpdAfter));
  const staleCls = await page.$eval('.numerics', el => el.className);
  check('dash: .numerics.stale applied', staleCls.includes('stale'));
  console.log('   heartbeat:', lastUpdBefore, '=>', lastUpdAfter);

  console.log('\n' + ok + ' passed, ' + fail + ' failed');
  if (errs.length) console.log('PAGEERRORS:', errs);
  await browser.close();
  process.exit(fail ? 1 : 0);
})();