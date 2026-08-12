const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  const logs = [];
  page.on('console', msg => logs.push(`[${msg.type()}] ${msg.text()}`));
  
  console.log('Loading page...');
  await page.goto('http://192.168.100.100/', { waitUntil: 'domcontentloaded', timeout: 60000 });
  
  // Wait for potential errors
  await page.waitForTimeout(10000);
  
  // Check if fetchNextBatch was called
  const called = logs.some(l => l.includes('fetchNextBatch') || l.includes('Processing'));
  console.log('fetchNextBatch called:', called);
  
  // Show relevant logs
  console.log('\n=== Console Logs (errors/warnings) ===');
  logs.filter(l => l.includes('error') || l.includes('Processing') || l.includes('fetchNext')).forEach(l => console.log(l));
  
  // Check Dexie after 30s
  await page.waitForTimeout(25000);
  
  const info = await page.evaluate(async () => {
    try {
      const db = new Dexie('EggubatorDB');
      const count = await db.logs.count();
      return { count };
    } catch(e) {
      return { error: e.message };
    }
  });
  
  console.log('\nDexie count after 35s:', info.count);
  if (info.error) console.log('Error:', info.error);
  
  await browser.close();
})();
