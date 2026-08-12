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
  
  // Capture network requests
  const requests = [];
  page.on('request', req => requests.push(req.url()));
  
  console.log('Navigating to dashboard...');
  await page.goto('http://192.168.100.100/', { timeout: 30000 });
  
  // Wait for loading to finish
  console.log('Waiting for page to load...');
  await page.waitForTimeout(15000); // Wait for data fetching
  
  console.log('\n=== Console Logs (last 30) ===');
  consoleLogs.slice(-30).forEach(log => {
    console.log(`[${log.type}] ${log.text}`);
  });
  
  console.log('\n=== Page Title ===');
  console.log(await page.title());
  
  // Check if loading overlay is visible
  const loadingVisible = await page.isVisible('#loadingOverlay');
  console.log('\nLoading overlay visible:', loadingVisible);
  
  // Check record count if visible
  const progressText = await page.textContent('#loadingProgress').catch(() => 'N/A');
  console.log('Progress text:', progressText);
  
  // Check Dexie database count via console
  const dbCount = await page.evaluate(async () => {
    const db = new Dexie('EggubatorDB');
    return await db.logs.count();
  }).catch(() => 'Error');
  console.log('Dexie log count:', dbCount);
  
  await browser.close();
  console.log('\n=== Done ===');
})();