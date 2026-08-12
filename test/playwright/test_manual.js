const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  const logs = [];
  page.on('console', msg => logs.push(`[${msg.type()}] ${msg.text()}`));
  page.on('pageerror', err => logs.push(`[PAGE ERROR] ${err.message}`));
  
  console.log('Loading page...');
  await page.goto('http://192.168.100.100/', { waitUntil: 'domcontentloaded', timeout: 60000 });
  
  // Wait for page to initialize
  await page.waitForTimeout(5000);
  
  // Check for JavaScript errors
  const errors = logs.filter(l => l.includes('error') || l.includes('ERROR'));
  if (errors.length > 0) {
    console.log('\n=== ERRORS FOUND ===');
    errors.forEach(e => console.log(e));
  }
  
  // Manually call fetchNextBatch(0,0) and check result
  console.log('\nManually calling fetchNextBatch(0,0)...');
  const result = await page.evaluate(async () => {
    try {
      await window.fetchNextBatch(0, 0);
      const count = await window.db.logs.count();
      return { success: true, count };
    } catch(e) {
      return { success: false, error: e.message };
    }
  });
  
  console.log('Result:', result);
  
  await browser.close();
})();
