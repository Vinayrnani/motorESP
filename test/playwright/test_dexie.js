const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  const consoleLogs = [];
  page.on('console', msg => {
    const text = msg.text();
    if (!text.includes('favicon')) {
      consoleLogs.push(`[${msg.type()}] ${text}`);
    }
  });
  
  console.log('Testing: Initial load should fetch all historical data...');
  await page.goto('http://192.168.100.100/', { timeout: 60000, waitUntil: 'domcontentloaded' });
  
  // Wait for initial load to complete (up to 30 seconds)
  await page.waitForTimeout(30000);
  
  // Check Dexie count via page evaluation
  const dexieInfo = await page.evaluate(async () => {
    try {
      const db = new Dexie('EggubatorDB');
      const count = await db.logs.count();
      const allLogs = await db.logs.toArray();
      const bootIds = [...new Set(allLogs.map(l => l.bootId))].sort((a,b) => a-b);
      return { count, bootIds, hasData: count > 0 };
    } catch(e) {
      return { error: e.message };
    }
  });
  
  console.log('\n=== Dexie Results ===');
  console.log('Total records:', dexieInfo.count);
  console.log('Boot IDs present:', dexieInfo.bootIds.join(', '));
  
  console.log('\n=== Console Errors ===');
  consoleLogs.filter(l => l.startsWith('[error]')).forEach(l => console.log(l));
  
  console.log('\n=== Test Result ===');
  if (dexieInfo.count > 0) {
    console.log('✓ PASS: Dexie has', dexieInfo.count, 'records');
  } else {
    console.log('✗ FAIL: Dexie is empty!');
    console.log('\nRecent console logs:');
    consoleLogs.slice(-10).forEach(l => console.log(l));
  }
  
  await browser.close();
})();
