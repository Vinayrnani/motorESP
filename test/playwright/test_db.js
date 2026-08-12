const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  const errors = [];
  page.on('console', msg => {
    if (msg.type() === 'error') {
      errors.push(msg.text());
    }
  });
  page.on('pageerror', err => errors.push('PAGE ERROR: ' + err.message));
  
  console.log('Loading page...');
  await page.goto('http://192.168.100.100/', { waitUntil: 'domcontentloaded', timeout: 60000 });
  
  await page.waitForTimeout(5000);
  
  console.log('\n=== Errors ===');
  errors.forEach(e => console.log(e));
  
  console.log('\n=== Checking db object ===');
  const dbCheck = await page.evaluate(() => {
    try {
      return { 
        hasDexie: typeof Dexie !== 'undefined',
        hasDb: typeof db !== 'undefined',
        dbLogs: typeof db !== 'undefined' ? typeof db.logs : 'N/A'
      };
    } catch(e) {
      return { error: e.message };
    }
  });
  console.log(JSON.stringify(dbCheck, null, 2));
  
  await browser.close();
})();
