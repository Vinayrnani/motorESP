const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  // Clear Dexie before test
  await page.goto('http://192.168.100.100/', { waitUntil: 'domcontentloaded' });
  await page.evaluate(async () => {
    const db = new Dexie('EggubatorDB');
    await db.delete();
  });
  
  console.log('Cleared Dexie. Reloading page...');
  
  // Now reload and let it fetch
  await page.reload({ waitUntil: 'domcontentloaded' });
  
  // Wait 30 seconds for data fetch
  await page.waitForTimeout(30000);
  
  // Check Dexie
  const result = await page.evaluate(async () => {
    try {
      const db = new Dexie('EggubatorDB');
      const count = await db.logs.count();
      return { count, error: null };
    } catch(e) {
      return { count: 0, error: e.message };
    }
  });
  
  console.log('Dexie record count:', result.count);
  if (result.error) console.log('Error:', result.error);
  
  await browser.close();
})();
