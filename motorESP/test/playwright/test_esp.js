const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  const logs = [];
  page.on('console', msg => logs.push({ type: msg.type(), text: msg.text() }));
  
  console.log('Navigating...');
  await page.goto('http://192.168.100.100/', { timeout: 60000, waitUntil: 'domcontentloaded' });
  
  // Wait a bit
  await page.waitForTimeout(5000);
  
  // Check ESP status via fetch
  const statusRes = await fetch('http://192.168.100.100/status');
  const statusData = await statusRes.json();
  console.log('ESP Status:', JSON.stringify(statusData));
  
  // Check what /data returns
  const dataRes = await fetch('http://192.168.100.100/data?boot=' + statusData.bootId + '&time=4294967295&count=200');
  const dataJson = await dataRes.json();
  console.log('/data response:', JSON.stringify(dataJson).substring(0, 500));
  
  // Show console logs
  console.log('\nConsole logs:');
  logs.forEach(l => console.log(`[${l.type}] ${l.text}`));
  
  await browser.close();
})();