const { chromium } = require('playwright');

const BASE = process.argv[2] || 'https://wings-coleman-dylan-lined.trycloudflare.com';
const results = [];
const pages = [
  { path: '/', name: 'Control' },
  { path: '/dashboard', name: 'Dashboard' },
  { path: '/settings', name: 'Settings' },
  { path: '/data', name: 'Data' },
];

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 390, height: 844 } });

  for (const p of pages) {
    const consoleErrors = [];
    const failedRequests = [];
    const pageErrors = [];
    page.on('console', m => { if (m.type() === 'error') consoleErrors.push(m.text()); });
    page.on('pageerror', e => pageErrors.push(e.message));
    page.on('requestfailed', r => failedRequests.push(r.url() + ' :: ' + (r.failure() && r.failure().errorText)));

    const resp = await page.goto(BASE + p.path, { timeout: 30000, waitUntil: 'domcontentloaded' });
    await page.waitForTimeout(4000);
    const title = await page.title();
    const text = await page.evaluate(() => document.body.innerText.replace(/\s+/g, ' ').trim().slice(0, 300));
    results.push({ page: p.name, path: p.path, status: resp && resp.status(), title, consoleErrors, pageErrors, failedRequests, text });
    await page.screenshot({ path: '/tmp/opencode/shot_' + p.path.replace('/', '') + '.png', fullPage: true });
  }

  console.log(JSON.stringify(results, null, 1));
  await browser.close();
})().catch(e => { console.error('FATAL', e.message); process.exit(1); });