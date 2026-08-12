const { chromium } = require('playwright');

(async () => {
  console.log('Opening browser...');
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  // Let page load and fetch data
  console.log('Navigating to dashboard...');
  await page.goto('http://192.168.100.100/', { timeout: 60000, waitUntil: 'domcontentloaded' });
  
  // Wait longer for all fetching to complete
  console.log('Waiting for data fetch...');
  await page.waitForTimeout(30000);
  
  // Get progress text
  const progressText = await page.textContent('#loadingProgress').catch(() => 'N/A');
  console.log('Progress text:', progressText);
  
  // Check if loading overlay is gone
  const loadingVisible = await page.isVisible('#loadingOverlay').catch(() => false);
  console.log('Loading overlay visible:', loadingVisible);
  
  // Try to get Dexie count
  const dbInfo = await page.evaluate(() => {
    try {
      const db = new Dexie('EggubatorDB');
      return { dbExists: true };
    } catch(e) {
      return { dbExists: false, error: e.message };
    }
  });
  console.log('Dexie available:', JSON.stringify(dbInfo));
  
  await browser.close();
  console.log('=== Done ===');
})();