const { chromium } = require('playwright');

(async () => {
  console.log('Opening browser...');
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage();
  
  // Capture all console messages
  const allLogs = [];
  page.on('console', msg => allLogs.push({ type: msg.type(), text: msg.text() }));
  
  console.log('Navigating to dashboard...');
  await page.goto('http://192.168.100.100/', { timeout: 60000, waitUntil: 'domcontentloaded' });
  
  // Wait for data fetching to complete
  await page.waitForTimeout(25000);
  
  // Get overall stats
  console.log('\n=== Summary ===');
  
  // Check loading status
  const progressText = await page.textContent('#loadingProgress').catch(() => 'N/A');
  console.log('Progress:', progressText);
  
  // Get Dexie stats
  const dbStats = await page.evaluate(async () => {
    const db = new Dexie('EggubatorDB');
    const total = await db.logs.count();
    const all = await db.logs.toArray();
    const bootIds = [...new Set(all.map(l => l.bootId))].sort((a,b) => a-b);
    const timeRange = all.length > 0 ? {
      earliest: new Date(Math.min(...all.map(l => l.t))).toISOString(),
      latest: new Date(Math.max(...all.map(l => l.t))).toISOString()
    } : null;
    return { total, bootIds, timeRange };
  });
  
  console.log('Total records in Dexie:', dbStats.total);
  console.log('Boot IDs found:', dbStats.bootIds.join(', '));
  console.log('Time range:', dbStats.timeRange ? `${dbStats.timeRange.earliest} to ${dbStats.timeRange.latest}` : 'N/A');
  
  // Show key console logs
  console.log('\n=== Key Console Logs ===');
  allLogs.forEach(log => {
    if (log.text.includes('fetchNextBatch') || 
        log.text.includes('Backend response') || 
        log.text.includes('Decoded') ||
        log.text.includes('Total in Dexie') ||
        log.text.includes('Saved: bootIds')) {
      console.log(log.text);
    }
  });
  
  await browser.close();
  console.log('\n=== Done ===');
})();