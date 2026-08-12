const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  const errors = [];
  page.on('pageerror', err => errors.push('PAGE ERROR: ' + err.message));
  page.on('console', msg => {
    if (msg.type() === 'error') errors.push('CONSOLE ERROR: ' + msg.text());
  });
  
  try {
    await page.goto('http://192.168.100.100/', { 
      waitUntil: 'domcontentloaded', 
      timeout: 30000 
    });
    await page.waitForTimeout(5000);
  } catch(e) {
    errors.push('NAVIGATION ERROR: ' + e.message);
  }
  
  if (errors.length > 0) {
    console.log('=== ERRORS ===');
    errors.forEach(e => console.log(e));
  } else {
    console.log('Page loaded without errors!');
    
    // Check if mainLoop is defined
    const hasMainLoop = await page.evaluate(() => typeof mainLoop !== 'undefined');
    console.log('mainLoop defined:', hasMainLoop);
  }
  
  await browser.close();
})();
