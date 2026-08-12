const { chromium } = require('playwright');

(async () => {
  console.log('Opening browser...');
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  // Capture console messages
  const consoleLogs = [];
  page.on('console', msg => {
    consoleLogs.push({ type: msg.type(), text: msg.text() });
  });
  
  // Go to page with different wait strategy
  console.log('Navigating to dashboard...');
  try {
    await page.goto('http://192.168.100.100/', { 
      timeout: 60000,
      waitUntil: 'domcontentloaded'
    });
    console.log('Page loaded (domcontentloaded)');
  } catch(e) {
    console.log('Timeout with domcontentloaded, trying networkidle...');
    await page.goto('http://192.168.100.100/', { 
      timeout: 60000,
      waitUntil: 'networkidle'
    });
    console.log('Page loaded (networkidle)');
  }
  
  // Wait a bit for JS to execute
  await page.waitForTimeout(5000);
  
  console.log('\n=== Console Logs (last 20) ===');
  consoleLogs.slice(-20).forEach(log => {
    console.log(`[${log.type}] ${log.text}`);
  });
  
  console.log('\n=== Page Title ===');
  console.log(await page.title());
  
  await browser.close();
  console.log('\n=== Done ===');
})();