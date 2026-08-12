const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const context = await browser.newContext({ ignoreHTTPSErrors: true });
  // Clear cache
  await context.clearCookies();
  const page = await context.newPage();
  
  // Disable cache
  await page.route('**/*', route => {
    route.continue({ headers: { ...route.request().headers(), 'Cache-Control': 'no-cache' } });
  });
  
  console.log('Loading page with cache disabled...');
  await page.goto('http://192.168.100.100/', { waitUntil: 'domcontentloaded', timeout: 60000 });
  
  // Wait for loading to complete
  await page.waitForTimeout(30000);
  
  // Check Dexie
  const info = await page.evaluate(async () => {
    try {
      const db = new Dexie('EggubatorDB');
      const count = await db.logs.count();
      return { count, error: null };
    } catch(e) {
      return { count: 0, error: e.message };
    }
  });
  
  console.log('Dexie count:', info.count);
  if (info.error) console.log('Error:', info.error);
  
  // Get console errors
  const logs = [];
  page.on('console', msg => logs.push(msg.text()));
  
  await browser.close();
})();
