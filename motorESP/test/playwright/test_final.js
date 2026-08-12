const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  // Listen to all events
  const errors = [];
  page.on('pageerror', err => {
    errors.push('PAGE ERROR: ' + err.message);
  });
  page.on('console', msg => {
    if (msg.type() === 'error') {
      errors.push('CONSOLE ERROR: ' + msg.text());
    }
  });
  
  console.log('Loading page (60s timeout)...');
  try {
    await page.goto('http://192.168.100.100/', { 
      waitUntil: 'load', 
      timeout: 60000 
    });
    console.log('Page loaded successfully!');
  } catch(e) {
    errors.push('NAV ERROR: ' + e.message);
  }
  
  // Wait a bit for any async errors
  await page.waitForTimeout(3000);
  
  if (errors.length > 0) {
    console.log('\n=== ALL ERRORS ===');
    errors.forEach(e => console.log(e));
  } else {
    console.log('No errors detected!');
  }
  
  await browser.close();
})();
