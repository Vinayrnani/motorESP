#!/usr/bin/env node
/**
 * Comprehensive state × mode combination test.
 * Verifies every UI element for every feasible combination.
 *
 * Simulatable states: OFF, RUNNING, TRIPPED
 * Modes: OFF(0), MANUAL(1), AUTO_RETRY(2)
 * Meter: valid (mock) or invalid (no mock, no PZEM)
 */

const BASE = process.argv[2] || 'http://motorESP.local';
const { chromium } = require('playwright');

async function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

async function fresh(page, path) {
  await page.goto(BASE + (path || '/'), { waitUntil: 'domcontentloaded', timeout: 15000 });
  await page.waitForFunction(() => {
    const el = document.getElementById('statusBig');
    return el && el.textContent.trim() !== '' && el.textContent.trim() !== 'Waiting for status…';
  }, { timeout: 12000 });
}

async function setMode(page, mode) {
  await page.request.post(BASE + '/control?action=mode&mode=' + mode);
  await sleep(400);
}

async function setMock(page, on, profile) {
  if (on) {
    await page.request.post(BASE + '/settings/api?mock=1&mockProfile=' + (profile || 'off'));
  } else {
    await page.request.post(BASE + '/settings/api?mock=0');
  }
  await sleep(800);
}

async function getUI(page) {
  return page.evaluate(() => ({
    statusBig: document.getElementById('statusBig')?.textContent?.trim() || '',
    statusPlain: document.getElementById('statusPlain')?.textContent?.trim() || '',
    heroClass: document.getElementById('heroWrap')?.className || '',
    icon: document.getElementById('statusIcon')?.textContent?.trim() || '',
    voltPill: document.getElementById('voltPill')?.textContent?.trim() || '',
    btnStart: document.getElementById('btnStart')?.disabled || false,
    btnStartText: document.getElementById('btnStart')?.firstChild?.textContent?.trim() || '',
    btnStartSub: document.getElementById('btnStart')?.querySelector('.act-sub')?.textContent?.trim() || '',
    btnStop: document.getElementById('btnStop')?.disabled || false,
    btnStopText: document.getElementById('btnStop')?.firstChild?.textContent?.trim() || '',
    btnStopSub: document.getElementById('btnStop')?.querySelector('.act-sub')?.textContent?.trim() || '',
    btnReset: document.getElementById('btnReset')?.disabled || false,
    btnResetText: document.getElementById('btnReset')?.firstChild?.textContent?.trim() || '',
    reasonPanel: document.getElementById('reasonPanel')?.classList?.contains('on') || false,
    testBanner: document.getElementById('testBanner')?.classList?.contains('on') || false,
    modeActive: document.querySelector('.mode-btn.active')?.dataset?.mode ?? '?',
  }));
}

async function waitForPoll(page) {
  // Wait for 3s poll interval + margin
  await sleep(3500);
}

// Expected values for each combination
// key: `${state}_mode${mode}`
const EXPECT = {
  // OFF state × modes
  'OFF_mode0': {
    statusBig: 'OFF',
    statusPlain: (s) => s.includes('off') || s.includes('Switch to'),
    heroClass: (c) => c.includes('ring-stop'),
    icon: '⏸',
    btnStartDisabled: true, // disabled: no PZEM = NO METER
    btnStartSub: 'NO METER',
    btnStopDisabled: true,
    btnStopSub: 'pump off',
    reasonPanel: false,
    modeActive: '0',
  },
  'OFF_mode1': {
    statusBig: 'STOPPED',
    statusPlain: (s) => s.includes('idle') || s.includes('Press START'),
    heroClass: (c) => c.includes('ring-stop'),
    icon: '⏸',
    btnStartDisabled: true, // disabled: no PZEM = NO METER
    btnStartSub: 'NO METER',
    btnStopDisabled: true,
    btnStopSub: 'pump off',
    reasonPanel: false,
    modeActive: '1',
  },
  'OFF_mode2': {
    statusBig: 'STOPPED',
    statusPlain: (s) => s.includes('idle') || s.includes('Press START'),
    heroClass: (c) => c.includes('ring-stop'),
    icon: '⏸',
    btnStartDisabled: true, // disabled: no PZEM = NO METER
    btnStartSub: 'NO METER',
    btnStopDisabled: true,
    btnStopSub: 'pump off',
    reasonPanel: false,
    modeActive: '2',
  },

  // RUNNING state × modes
  'RUNNING_mode0': {
    statusBig: 'RUNNING',
    statusPlain: (s) => s.includes('running normally') || s.includes('running'),
    heroClass: (c) => c.includes('ring-ok'),
    icon: '⚡',
    btnStartDisabled: true,
    btnStartText: 'RUNNING',
    reasonPanel: false,
  },
  'RUNNING_mode1': {
    statusBig: 'RUNNING',
    statusPlain: (s) => s.includes('running normally') || s.includes('running'),
    heroClass: (c) => c.includes('ring-ok'),
    icon: '⚡',
    btnStartDisabled: true,
    btnStartText: 'RUNNING',
    reasonPanel: false,
  },
  'RUNNING_mode2': {
    statusBig: 'RUNNING',
    statusPlain: (s) => s.includes('running normally') || s.includes('running'),
    heroClass: (c) => c.includes('ring-ok'),
    icon: '⚡',
    btnStartDisabled: true,
    btnStartText: 'RUNNING',
    reasonPanel: false,
  },

  // TRIPPED state × modes
  'TRIPPED_mode0': {
    statusBig: 'SAFETY STOP',
    statusPlain: (s) => s.includes('stopped for safety') || s.includes('LOCKOUT'),
    heroClass: (c) => c.includes('ring-alarm'),
    icon: '⚠',
    btnStartDisabled: true,
    btnStartSub: 'RESET NEEDED',
    btnStopDisabled: true,
    btnStopSub: 'safety stop',
    btnResetDisabled: false,
    reasonPanel: true,
  },
  'TRIPPED_mode1': {
    statusBig: 'SAFETY STOP',
    statusPlain: (s) => s.includes('stopped for safety') || s.includes('LOCKOUT'),
    heroClass: (c) => c.includes('ring-alarm'),
    icon: '⚠',
    btnStartDisabled: true,
    btnStartSub: 'RESET NEEDED',
    btnStopDisabled: true,
    btnStopSub: 'safety stop',
    btnResetDisabled: false,
    reasonPanel: true,
  },
  'TRIPPED_mode2': {
    statusBig: 'SAFETY STOP',
    statusPlain: (s) => s.includes('stopped for safety') || s.includes('LOCKOUT'),
    heroClass: (c) => c.includes('ring-alarm'),
    icon: '⚠',
    btnStartDisabled: true,
    btnStartSub: 'RESET NEEDED',
    btnStopDisabled: true,
    btnStopSub: 'safety stop',
    btnResetDisabled: false,
    reasonPanel: true,
  },
};

let pass = 0, fail = 0, total = 0;

function check(label, actual, expected) {
  total++;
  if (typeof expected === 'function') {
    if (expected(actual)) { pass++; }
    else { fail++; console.log(`  ✗ ${label}: "${actual}" (fn check failed)`); }
  } else if (actual === expected) {
    pass++;
  } else {
    fail++;
    console.log(`  ✗ ${label}: got "${expected}" expected "${actual}"`);
  }
}

(async () => {
  const browser = await chromium.launch({ headless: true });
  const ctx = await browser.newContext({ viewport: { width: 420, height: 800 } });
  const page = await ctx.newPage();

  page.on('console', msg => {
    if (msg.type() === 'error') console.log(`  [console.error] ${msg.text()}`);
  });

  const combos = [
    // [mode, stateSetup, stateLabel]
    [0, null, 'OFF'],           // mode OFF → pump off by default
    [1, null, 'OFF'],           // mode MANUAL → pump off
    [2, null, 'OFF'],           // mode AUTO → pump off
    [0, 'running', 'RUNNING'],  // mode OFF + mock running (externally started)
    [1, 'running', 'RUNNING'],  // mode MANUAL + running
    [2, 'running', 'RUNNING'],  // mode AUTO + running
    [1, 'oc', 'TRIPPED'],       // mode MANUAL + trip
    [2, 'oc', 'TRIPPED'],       // mode AUTO + trip
  ];

  // Process each combo
  for (const [mode, mockProfile, stateLabel] of combos) {
    const modeName = ['OFF', 'MANUAL', 'AUTO'][mode];
    const combo = `${stateLabel}_mode${mode}`;
    console.log(`\n--- ${stateLabel} × ${modeName} mode ---`);

    // Clean state: reset trips, turn off mock, set mode
    await page.request.post(BASE + '/control?action=reset').catch(() => {});
    await sleep(500);
    await setMock(page, false);
    await sleep(1000);
    await setMode(page, 1); // set to MANUAL first to ensure pump can be controlled
    await sleep(500);

    // Now set up the desired state
    if (mockProfile) {
      await setMock(page, true, mockProfile);
      await sleep(2000);
      // For TRIPPED, wait for trip to fire
      if (stateLabel === 'TRIPPED') await sleep(6000);
    }

    // Set target mode AFTER state setup
    await setMode(page, mode);
    await fresh(page, '/');
    await sleep(1500); // let first poll settle

    const ui = await getUI(page);
    const exp = EXPECT[combo];
    if (!exp) { console.log('  (no expectations defined)'); continue; }

    // Check each field
    if (exp.statusBig) check('statusBig', ui.statusBig, exp.statusBig);
    if (exp.statusPlain) check('statusPlain', ui.statusPlain, exp.statusPlain);
    if (exp.heroClass) check('heroClass', ui.heroClass, exp.heroClass);
    if (exp.icon) check('icon', ui.icon, exp.icon);
    if (exp.btnStartDisabled !== undefined) check('btnStart.disabled', ui.btnStart, exp.btnStartDisabled);
    if (exp.btnStartText) check('btnStart.text', ui.btnStartText, exp.btnStartText);
    if (exp.btnStartSub) check('btnStart.sub', ui.btnStartSub, exp.btnStartSub);
    if (exp.btnStopDisabled !== undefined) check('btnStop.disabled', ui.btnStop, exp.btnStopDisabled);
    if (exp.btnStopText) check('btnStop.text', ui.btnStopText, exp.btnStopText);
    if (exp.btnStopSub) check('btnStop.sub', ui.btnStopSub, exp.btnStopSub);
    if (exp.btnResetDisabled !== undefined) check('btnReset.disabled', ui.btnReset, exp.btnResetDisabled);
    if (exp.reasonPanel !== undefined) check('reasonPanel', ui.reasonPanel, exp.reasonPanel);
    if (exp.modeActive) check('modeActive', ui.modeActive, exp.modeActive);

    // Extra: always log what's shown for manual review
    console.log(`  [actual] big="${ui.statusBig}" plain="${ui.statusPlain.substring(0, 60)}" icon="${ui.icon}" hero="${ui.heroClass}"`);
    console.log(`  [actual] start=${ui.btnStart ? 'disabled' : 'enabled'} "${ui.btnStartText}" "${ui.btnStartSub}" | stop=${ui.btnStop ? 'disabled' : 'enabled'} "${ui.btnStopText}" "${ui.btnStopSub}" | reset=${ui.btnReset ? 'disabled' : 'enabled'}`);
    console.log(`  [actual] reasonPanel=${ui.reasonPanel} mode=${ui.modeActive} test=${ui.testBanner}`);
  }

  // Cleanup
  await setMock(page, false);
  await setMode(page, 1);

  console.log(`\n=== Results: ${pass}/${total} passed, ${fail} failed ===`);
  await browser.close();
  process.exit(fail > 0 ? 1 : 0);
})();
