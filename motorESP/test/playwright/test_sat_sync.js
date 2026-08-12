/**
 * SAT Sync Test: Verifies that after a new batch, the very first dashboard load
 * correctly sets Unix timestamps on decoded log entries.
 *
 * Bug: syncSAT() updates the ESP's boot startUnix via syncTime, but does NOT
 * update the browser's bootStartCache with the corrected value. So fetchNextBatch()
 * → decodeLogs() uses stale startUnix=0, producing timestamps near epoch 0 (1970).
 *
 * Run:  node test/playwright/test_sat_sync.js
 * CAUTION: This test triggers a new batch, which CLEARS ALL LOGS on the device.
 */

const { chromium } = require('playwright');
const http = require('http');

// Resolve ESP IP via mDNS first, so updates work regardless of DHCP assignment
const ESP_URL = (() => {
  const { execSync } = require('child_process');
  try {
    const ip = execSync('ping -c 1 eggubator.local 2>/dev/null | grep -oE \'[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+\' | head -1')
      .toString().trim();
    if (ip) return `http://${ip}`;
  } catch (_) {}
  return 'http://192.168.100.100';
})();

const YEAR_2020_MS = 1577836800000; // Any real timestamp must be > this

function httpGet(url) {
  return new Promise((resolve, reject) => {
    const req = http.get(url, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve(data));
    });
    req.on('error', reject);
    req.setTimeout(10000, () => { req.destroy(); reject(new Error('timeout')); });
  });
}

async function getStatus() {
  return JSON.parse(await httpGet(`${ESP_URL}/status`));
}

async function main() {
  // ── Phase 1: Check device is alive ──
  console.log('=== SAT Sync Test ===\n');

  let status;
  try {
    status = await getStatus();
    console.log(`Device:     ${status.ip} (v${status.version})`);
    console.log(`Boot ID:    ${status.bootId}`);
    console.log(`Sector:     ${status.currentSector} (start: ${status.startSector})`);
    console.log(`startTS:    ${status.batchStartUnix} (${new Date(status.batchStartUnix * 1000).toISOString()})`);
    console.log(`UptimeSec:  ${status.uptimeSec}`);
    console.log(`ElapsedSec: ${status.elapsedSeconds}`);
    console.log(`Day:        ${status.currentDay}\n`);
  } catch (e) {
    console.error(`✗ Cannot reach ESP at ${ESP_URL}: ${e.message}`);
    process.exit(1);
  }

  // ── Phase 2: Open Playwright browser ──
  console.log('Launching browser...');
  const browser = await chromium.launch({ headless: true });
  const context = await browser.newContext();
  const page = await context.newPage();

  const consoleLogs = [];
  const pageErrors = [];
  page.on('console', msg => {
    const text = msg.text();
    // Filter out noisy logs
    if (!text.includes('favicon')) {
      consoleLogs.push({ type: msg.type(), text });
    }
  });
  page.on('pageerror', err => {
    pageErrors.push(err.message);
  });

  // ── Phase 3: Trigger a new batch ──
  console.log('\n=== Starting new batch (this CLEARS all logs!) ===');

  // Navigate to settings page
  await page.goto(`${ESP_URL}/settings`, { timeout: 30000, waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(3000);

  // The "Start New Batch" button triggers confirm() dialog
  // We need to handle the dialog before clicking
  page.once('dialog', async dialog => {
    console.log(`Dialog: "${dialog.message()}" → accepting`);
    await dialog.accept();
  });

  // Evaluate startNewBatch directly to avoid dialog issue
  console.log('Triggering new batch...');
  const oldStartSector = status.startSector;

  // We call the underlying API directly instead of through the UI (which uses confirm())
  const unixNow = Math.floor(Date.now() / 1000);
  try {
    const resp = await httpGet(`${ESP_URL}/settings/api?action=newBatch&timestamp=${unixNow}`);
    console.log(`newBatch response: ${resp}`);
  } catch (e) {
    // ESP will reboot, so this request will fail - that's expected
    console.log('ESP rebooting after new batch (expected)');
  }

  // Wait for ESP to reboot
  console.log('Waiting for ESP to reboot...');
  let rebooted = false;
  for (let i = 0; i < 30; i++) {
    await new Promise(r => setTimeout(r, 1000));
    try {
      const s = await getStatus();
      if (s.bootId !== undefined) {
        console.log(`ESP back online after ${i + 1}s`);
        console.log(`  New bootId: ${s.bootId}, sector: ${s.currentSector} (was ${oldStartSector})`);
        status = s;
        rebooted = true;
        break;
      }
    } catch (_) {}
  }
  if (!rebooted) {
    console.error('✗ ESP did not come back online after reboot');
    await browser.close();
    process.exit(1);
  }

  // ── Phase 4: First dashboard load after new batch ──
  console.log('\n=== First dashboard load after new batch ===');

  // Fresh page context to simulate a new browser (no cached Dexie data)
  // Actually we can just use the same page, but the startSector has changed
  // so the dashboard will detect it and clear Dexie.
  await page.goto(`${ESP_URL}/`, { timeout: 60000, waitUntil: 'domcontentloaded' });

  console.log('Waiting for SAT sync + initial data fetch...');
  // Wait for Dexie CDN to load and dashboard to initialize
  await page.waitForFunction(() => typeof Dexie !== 'undefined', { timeout: 30000 }).catch(() => {
    console.log('Warning: Dexie not loaded from CDN, continuing...');
  });
  await page.waitForTimeout(15000);

  // Check if loading overlay is gone
  const overlayVisible = await page.evaluate(() => {
    const el = document.getElementById('loadingOverlay');
    if (!el) return 'not-found';
    return el.style.display !== 'none' ? 'visible' : 'hidden';
  }).catch(() => 'error');
  console.log(`Loading overlay: ${overlayVisible}`);

  // ── Phase 5: Inspect Dexie data for correct timestamps ──
  console.log('\n=== Dexie timestamp validation ===');

  const dexieReport = await page.evaluate(async () => {
    const db = new Dexie('EggubatorDB');
    db.version(4).stores({
      logs: 't, timeSec, bootId, temp, hum, h, a, f, s',
      bootTimestamps: 'bootId, startUnix, duration'
    });
    const count = await db.logs.count();
    if (count === 0) {
      return { count: 0, status: 'EMPTY' };
    }

    const all = await db.logs.toArray();
    const timestamps = all.map(l => l.t);
    const minT = Math.min(...timestamps);
    const maxT = Math.max(...timestamps);
    const bootIds = [...new Set(all.map(l => l.bootId))].sort((a,b) => a-b);
    const invalidCount = all.filter(l => l.t < 1577836800000).length; // < year 2020
    const unixNow = Math.floor(Date.now() / 1000);

    // Check bootStartCache (from bootTimestamps table)
    const bootTS = await db.bootTimestamps.toArray();

    // Check a sample decoded entry
    const sample = all.slice(0, 3).map(l => ({
      t_raw: l.t,
      t_date: new Date(l.t).toISOString(),
      timeSec: l.timeSec,
      bootId: l.bootId,
      temp: l.temp,
      hum: l.hum
    }));

    const nowMs = Date.now();
    const oneWeekMs = 7 * 24 * 60 * 60 * 1000;
    const allValid = all.every(l => l.t > 1577836800000 && l.t < nowMs + oneWeekMs);

    return {
      count,
      minT,
      maxT,
      minT_date: new Date(minT).toISOString(),
      maxT_date: new Date(maxT).toISOString(),
      bootIds,
      invalidEntries: invalidCount,
      allValid,
      bootTimestamps: bootTS,
      sample,
      nowMs
    };
  });

  console.log(`Records in Dexie:   ${dexieReport.count}`);
  if (dexieReport.count > 0) {
    console.log(`Boot IDs:           ${dexieReport.bootIds.join(', ')}`);
    console.log(`Timestamp range:    ${dexieReport.minT_date} → ${dexieReport.maxT_date}`);
    console.log(`Invalid entries:    ${dexieReport.invalidEntries} (< 2020)`);
    console.log(`All timestamps valid: ${dexieReport.allValid}`);

    if (dexieReport.sample && dexieReport.sample.length > 0) {
      console.log('\nSample entries:');
      dexieReport.sample.forEach((s, i) => {
        console.log(`  [${i}] t=${s.t_date} timeSec=${s.timeSec} bootId=${s.bootId} temp=${s.temp} hum=${s.hum}`);
      });
    }

    if (dexieReport.bootTimestamps && dexieReport.bootTimestamps.length > 0) {
      console.log('\nBoot timestamps cache:');
      dexieReport.bootTimestamps.forEach(bt => {
        const d = bt.startUnix > 0 ? new Date(bt.startUnix * 1000).toISOString() : 'INVALID';
        console.log(`  bootId=${bt.bootId} startUnix=${bt.startUnix} (${d}) duration=${bt.duration}`);
      });
    }
  }

  // ── Phase 6: Second load (should work after refresh) ──
  console.log('\n=== Second dashboard load (refresh) ===');
  await page.reload({ waitUntil: 'domcontentloaded' });
  await page.waitForTimeout(15000);

  const dexieReport2 = await page.evaluate(async () => {
    const db = new Dexie('EggubatorDB');
    db.version(4).stores({
      logs: 't, timeSec, bootId, temp, hum, h, a, f, s',
      bootTimestamps: 'bootId, startUnix, duration'
    });
    const count = await db.logs.count();
    if (count === 0) return { count: 0 };

    const all = await db.logs.toArray();
    const timestamps = all.map(l => l.t);
    const minT = Math.min(...timestamps);
    const maxT = Math.max(...timestamps);
    const bootTS = await db.bootTimestamps.toArray();

    return {
      count,
      minT,
      maxT,
      minT_date: new Date(minT).toISOString(),
      maxT_date: new Date(maxT).toISOString(),
      bootTS
    };
  });

  console.log(`Records:  ${dexieReport2.count}`);
  console.log(`Timestamps: ${dexieReport2.minT_date} → ${dexieReport2.maxT_date}`);

  if (dexieReport2.bootTS) {
    console.log('Boot timestamps:');
    dexieReport2.bootTS.forEach(bt => {
      const d = bt.startUnix > 0 ? new Date(bt.startUnix * 1000).toISOString() : 'INVALID';
      console.log(`  bootId=${bt.bootId} startUnix=${bt.startUnix} (${d})`);
    });
  }

  // ── RESULTS ──
  console.log('\n\n=== TEST RESULTS ===');

  const firstLoadOk = dexieReport.count > 0 && dexieReport.allValid;
  const secondLoadOk = dexieReport2.count > 0 &&
    dexieReport2.minT > YEAR_2020_MS &&
    dexieReport2.maxT > YEAR_2020_MS;

  if (firstLoadOk) {
    console.log('✓ PASS: First load after new batch has valid timestamps');
  } else {
    console.log('✗ FAIL: First load after new batch has INVALID timestamps:');
    if (dexieReport.count === 0) console.log('  - Dexie is empty');
    if (!dexieReport.allValid) {
      console.log(`  - ${dexieReport.invalidEntries}/${dexieReport.count} entries have timestamps < 2020`);
      console.log(`  - Min timestamp: ${dexieReport.minT_date} (${dexieReport.minT}ms)`);
    }
  }

  if (secondLoadOk) {
    console.log('✓ PASS: Second load has valid timestamps');
  } else {
    console.log(`✗ FAIL: Second load still shows issues`);
    if (dexieReport2.count === 0) console.log('  - Dexie is empty');
  }

  // ── Console errors summary ──
  const errors = consoleLogs.filter(l => l.type === 'error' || l.text.includes('Error') || l.text.includes('error'));
  if (errors.length > 0) {
    console.log('\nConsole errors:');
    errors.slice(0, 10).forEach(e => console.log(`  [${e.type}] ${e.text}`));
  }
  if (pageErrors.length > 0) {
    console.log('\nPage errors:');
    pageErrors.forEach(e => console.log(`  ${e}`));
  }

  await browser.close();
  console.log('\n=== Test Complete ===');

  // Exit with non-zero if first load failed
  if (!firstLoadOk) process.exit(1);
}

main().catch(e => {
  console.error('Fatal:', e);
  process.exit(1);
});
