const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  const errors = [];
  page.on('pageerror', err => errors.push(err.message));
  page.on('console', msg => {
    if (msg.type() === 'error') errors.push(msg.text());
  });
  
  await page.goto('http://192.168.100.100/', { waitUntil: 'domcontentloaded', timeout: 60000 });
  await page.waitForTimeout(3000);
  
  if (errors.length > 0) {
    console.log('=== ERRORS ===');
    errors.forEach(e => console.log(e));
  } else {
    console.log('No JavaScript errors - page loaded successfully');
  }
  
  await browser.close();
})();
