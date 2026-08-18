#!/usr/bin/env node
/*
 * test_perf_tunnel.js — dashboard performance over the reverse tunnel
 * Usage: node test_perf_tunnel.js <BASE_URL> [loads]
 */
const { chromium } = require('playwright');

const BASE = process.argv[2] || 'http://68.233.98.190:8280';
const LOADS = parseInt(process.argv[3] || '3', 10);

(async () => {
  const browser = await chromium.launch({ headless: true });
  let best = null;
  for (let i = 0; i < LOADS; i++) {
    const page = await browser.newPage();
    const reqs = [];
    const errors = [];

    page.on('request', r => {
      reqs.push({ url: r.url(), method: r.method(), start: 0 });
    });
    page.on('requestfinished', r => {
      const e = reqs.find(x => x.url === r.url() && !x.end);
      if (e) e.end = Date.now();
    });
    page.on('requestfailed', r => {
      errors.push(r.url() + ' :: ' + (r.failure() || {}).errorText || r.url());
    });
    page.on('console', m => { if (m.type() === 'error') errors.push('console: ' + m.text()); });

    const t0 = Date.now();
    await page.goto(BASE + '/dashboard', { waitUntil: 'load', timeout: 60000 });
    const loadMs = Date.now() - t0;

    await page.waitForFunction(() => document.readyState === 'complete', { timeout: 30000 }).catch(() => {});
    const nav = await page.evaluate(() => ({
      dcl: performance.timing.domContentLoadedEventEnd - performance.timing.navigationStart,
      load: performance.timing.loadEventEnd - performance.timing.navigationStart,
    }));
    const reqStats = reqs.map(r => ({ url: r.url.replace(BASE, ''), ms: r.end ? r.end - (r.start || Date.now()) : null }));

    const firstPoll = await page.evaluate(() => new Promise(resolve => {
      const t = performance.now();
      fetch('status', { cache: 'no-store' }).then(r => r.json()).then(() => resolve(Math.round(performance.now() - t)));
    })).catch(() => null);

    console.log(`load#${i + 1}: total=${loadMs}ms dcl=${nav.dcl}ms loadEvent=${nav.load}ms firstPoll=${firstPoll}ms reqs=${reqStats.length} errors=${errors.length}`);
    for (const r of reqStats) console.log(`   ${r.url} -> ${r.ms === null ? 'PENDING' : r.ms + 'ms'}`);
    for (const e of errors.slice(0, 10)) console.log('   ERR ' + e);
    if (!best || loadMs < best.loadMs) best = { loadMs, reqStats, firstPoll };
    await page.close();
  }
  console.log('\nBEST load: ' + best.loadMs + 'ms firstPoll=' + best.firstPoll + 'ms');
  await browser.close();
})();