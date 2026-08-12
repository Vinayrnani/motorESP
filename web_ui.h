#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

// ============================================
// CONTROL PAGE (/)
// ============================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>motorESP - Pump Control</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E💧%3C/text%3E%3C/svg%3E">
<style>
:root { --blue:#1877f2; --green:#16a34a; --red:#dc2626; --amber:#d97706; --slate:#64748b; --bg:#f1f5f9; }
* { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
body { font-family:system-ui,sans-serif; margin:0; background:var(--bg); color:#0f172a; }
.container { max-width:560px; margin:0 auto; padding:12px; }
.header { background:linear-gradient(135deg,var(--blue),#0a5ac2); padding:14px 16px; border-radius:12px; color:#fff; text-align:center; margin-bottom:12px; }
.header h1 { margin:0; font-size:21px; font-weight:800; letter-spacing:.3px; }
.header .sub { font-size:12px; opacity:.85; margin-top:2px; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
.card h3 { margin:0 0 4px; font-size:14px; color:#334155; text-transform:uppercase; letter-spacing:.4px; }
.card .cardhelp { font-size:12px; color:var(--slate); margin:0 0 10px; }
.banner { border-radius:10px; padding:10px 12px; font-weight:700; font-size:13px; margin-bottom:10px; text-align:center; }
.banner-test { background:#fef3c7; border:1px solid #f59e0b; color:#92400e; }
.banner-warn { background:#fff7ed; border:1px solid var(--amber); color:#9a3412; }
.banner-info { background:#eff6ff; border:1px solid #93c5fd; color:#1e40af; }
.banner-danger { background:#fef2f2; border:1px solid #fca5a5; color:#991b1b; }
.status-hero { text-align:center; padding:4px 0 12px; }
.status-icon { font-size:44px; line-height:1.1; }
.status-big { font-size:24px; font-weight:800; margin-top:4px; }
.status-big.ok { color:var(--green); }
.status-big.stop { color:#475569; }
.status-big.warn { color:var(--amber); }
.status-big.alarm { color:var(--red); }
.status-plain { font-size:13px; color:#475569; margin-top:6px; line-height:1.45; }
.stat-grid { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; }
.stat-card { background:#f8fafc; padding:10px 6px; border-radius:10px; text-align:center; }
.stat-label { font-size:10px; color:var(--slate); font-weight:700; text-transform:uppercase; }
.stat-value { font-size:20px; font-weight:800; margin-top:2px; }
.stat-unit { font-size:10px; color:var(--slate); }
.volt-pill { display:inline-block; padding:4px 12px; border-radius:12px; font-size:12px; font-weight:800; margin-top:8px; }
.vp-normal { background:#dcfce7; color:#166534; }
.vp-warning { background:#fef3c7; color:#92400e; }
.vp-critical { background:#fee2e2; color:#991b1b; }
.btn { display:block; width:100%; padding:15px; border:none; border-radius:12px; font-size:17px; font-weight:800; color:#fff; cursor:pointer; margin-bottom:8px; }
.btn:active { transform:scale(.98); }
.btn-on { background:var(--green); }
.btn-off { background:var(--red); }
.btn-reset { background:#475569; }
.btn-help { background:#e2e8f0; color:#334155; font-size:14px; padding:12px; }
.btn-note { font-size:11px; font-weight:600; color:#fff; opacity:.9; margin-top:2px; }
.btn-off .btn-note { color:#fee2e2; }
.feed { min-height:20px; font-size:13px; font-weight:700; text-align:center; margin-top:6px; }
.feed.good { color:var(--green); } .feed.bad { color:var(--red); } .feed.info { color:var(--blue); }
.mode-row { display:flex; gap:8px; }
.mode-btn { flex:1; padding:11px 6px 9px; border:2px solid #e2e8f0; background:#fff; border-radius:10px; cursor:pointer; text-align:center; }
.mode-btn .m-title { font-size:14px; font-weight:800; color:#334155; display:block; }
.mode-btn .m-desc { font-size:10px; color:var(--slate); display:block; margin-top:2px; line-height:1.3; }
.mode-btn.active { border-color:var(--blue); background:#eff6ff; }
.mode-btn.active .m-title { color:var(--blue); }
.detail-row { display:flex; justify-content:space-between; font-size:13px; padding:5px 0; border-bottom:1px dashed #e2e8f0; }
.detail-row:last-child { border-bottom:none; }
.detail-row b { font-weight:700; }
details { background:#f8fafc; border-radius:10px; padding:10px 12px; margin-top:8px; }
summary { font-size:13px; font-weight:800; color:#334155; cursor:pointer; }
details p, details li { font-size:12.5px; color:#475569; line-height:1.5; margin:6px 0; }
details ul { margin:6px 0; padding-left:18px; }
.nav { display:flex; gap:8px; }
.nav a { flex:1; text-align:center; padding:11px 4px; background:var(--blue); border-radius:10px; text-decoration:none; color:#fff; font-weight:800; font-size:13px; }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>💧 Pump Controller</h1>
    <div class="sub" id="subLine">motorESP · connecting…</div>
  </div>

  <div id="testBanner" class="banner banner-test" style="display:none">🧪 TEST MODE — readings are simulated. No real pump data.</div>

  <div class="card">
    <div class="status-hero">
      <div class="status-icon" id="statusIcon">⚪</div>
      <div class="status-big stop" id="statusBig">OFF</div>
      <div class="volt-pill vp-normal" id="voltPill">VOLTAGE OK</div>
      <div class="status-plain" id="statusPlain">Waiting for status…</div>
    </div>
    <div class="stat-grid">
<div class="stat-card"><div class="stat-label">Voltage</div><div class="stat-value"><span id="stVolt">--</span></div></div>
      <div class="stat-card"><div class="stat-label">Current</div><div class="stat-value"><span id="stCur">--</span></div></div>
      <div class="stat-card"><div class="stat-label">Power</div><div class="stat-value"><span id="stPow">--</span></div></div>
    </div>
  </div>

  <div class="card" id="reasonCard" style="display:none">
    <h3>What is happening</h3>
    <div class="banner banner-info" id="reasonBanner"></div>
    <div class="detail-row"><span>Automatic restart</span><b id="detRetry">—</b></div>
    <div class="detail-row"><span>Retries used</span><b id="detRetriesUsed">—</b></div>
    <div class="detail-row"><span>Fast faults</span><b id="detFast">—</b></div>
    <div class="detail-row"><span>Start blocked until</span><b id="detBlocked">—</b></div>
  </div>

  <div class="card">
    <h3>Operating Mode</h3>
    <p class="cardhelp">How the pump is controlled. Tap one to change it right away.</p>
    <div class="mode-row">
      <button class="mode-btn" data-mode="0"><span class="m-title">⏹ OFF</span><span class="m-desc">Pump can't start</span></button>
      <button class="mode-btn" data-mode="1"><span class="m-title">👆 MANUAL</span><span class="m-desc">You start &amp; stop</span></button>
      <button class="mode-btn" data-mode="2"><span class="m-title">🤖 AUTO</span><span class="m-desc">Automatic schedule</span></button>
    </div>
  </div>

  <div class="card">
    <h3>Pump Control</h3>
    <p class="cardhelp">Start and stop the pump. If safety stops it, you'll see the reason above.</p>
    <button class="btn btn-on" id="btnStart">▶ START PUMP<span class="btn-note">Sends start pulse to the motor contactor</span></button>
    <button class="btn btn-off" id="btnStop">■ STOP PUMP<span class="btn-note">Sends stop pulse — motor contactor releases</span></button>
    <button class="btn btn-reset" id="btnReset">↺ RESET SAFETY ALARM<span class="btn-note">Enable this only after the fault is fixed</span></button>
    <div class="feed" id="msg"></div>
  </div>

  <div class="card">
    <h3>Help &amp; Explanation</h3>
    <details>
      <summary>What do the statuses mean?</summary>
      <ul>
        <li><b>OFF / STOPPED</b> — The pump is not running. Press <b>START PUMP</b>.</li>
        <li><b>RUNNING</b> — The pump is working. Current above ~2&nbsp;A means the motor switched on.</li>
        <li><b>SAFETY STOP</b> — The controller stopped the pump because it detected a problem. Read the reason in
            “What is happening” and press <b>RESET SAFETY ALARM</b> after fixing it.</li>
        <li><b>POWER RESTORED</b> — After a power cut the controller waits for you to press START. This is a safety feature.</li>
      </ul>
    </details>
    <details>
      <summary>What does each mode do?</summary>
      <ul>
        <li><b>OFF</b> — The pump will never start. Use when the pump must stay off (e.g. maintenance).</li>
        <li><b>MANUAL</b> — You decide. Press START or STOP whenever you want.</li>
        <li><b>AUTO</b> — The controller starts and stops the pump on a schedule (coming soon).</li>
      </ul>
    </details>
    <details>
      <summary>Why won't the pump start?</summary>
      <p>Most common reasons:</p>
      <ul>
        <li><b>Too soon since it last stopped</b> — the pump waits (default 60&nbsp;s) before it can restart. This protects the motor.</li>
        <li><b>Voltage too high</b> — the controller refuses to start while the mains voltage is dangerously high.</li>
        <li><b>A safety alarm is active</b> — press RESET after fixing the cause.</li>
        <li><b>Test mode left on</b> — a simulated reading may be blocking the start. Turn off Test mode in Settings.</li>
      </ul>
    </details>
    <details>
      <summary>What is “Voltage status”?</summary>
      <ul>
        <li><b>VOLTAGE OK</b> — normal mains voltage, usually 200–250&nbsp;V.</li>
        <li><b>WARNING</b> — voltage is high. Watch for flicker or overheating.</li>
        <li><b>CRITICAL</b> — voltage is dangerously high; the pump will not be allowed to start.</li>
      </ul>
    </details>
  </div>

  <div class="nav">
    <a href="/dashboard">📊 Dashboard</a>
    <a href="/settings">⚙️ Settings</a>
    <a href="/data">📋 Log</a>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let mode = 1;
const TRIP_PLAIN = {2:"Overload — the pump drew too much current",4:"No Water — the pump ran dry",
  8:"Voltage too high",16:"Voltage too low",64:"Power sensor fault",128:"Pump did not start"};

function setModeBtn(m) {
  mode = m;
  document.querySelectorAll('.mode-btn').forEach(b => b.classList.toggle('active', parseInt(b.dataset.mode) === m));
}
document.querySelectorAll('.mode-btn').forEach(b => b.addEventListener('click', () => {
  setModeBtn(parseInt(b.dataset.mode));
  fetch('/control?action=mode&mode=' + b.dataset.mode).then(r => r.text()).then(m => feed(m));
}));

function feed(m, cls) {
  const el = $('msg'); el.textContent = m; el.className = 'feed ' + (cls || 'info');
  clearTimeout(feed._t);
  feed._t = setTimeout(() => { if (el.textContent === m) { el.textContent = ''; el.className = 'feed'; } }, 6000);
}
function doAct(a) { return fetch('/control?action=' + a).then(r => r.text()).then(m => feed(m, m.indexOf('OK')>=0 ? 'good' : (m.indexOf('BLOCKED')>=0 ? 'bad' : 'info'))); }
$('btnStart').onclick = () => doAct('start');
$('btnStop').onclick = () => doAct('stop');
$('btnReset').onclick = () => doAct('reset');

function fmtCountdown(sec) {
  if (!sec || sec <= 0) return 'not scheduled';
  const m = Math.floor(sec / 60), s = sec % 60;
  return (m ? m + 'm ' : '') + s + 's';
}

async function refresh() {
  try {
    const s = await (await fetch('/status')).json();
    $('subLine').textContent = 'motorESP · ' + s.version + (s.mock ? ' · TEST MODE' : ' · ' + (s.rssi > -80 ? 'signal ok' : 'weak signal'));
    $('testBanner').style.display = s.mock ? 'block' : 'none';
    $('stVolt').textContent = s.voltage.toFixed(0) + ' V';
    $('stCur').textContent = s.current.toFixed(1) + ' A';
    $('stPow').textContent = s.power >= 1000 ? (s.power/1000).toFixed(2) + ' kW' : s.power.toFixed(0) + ' W';

    const pill = $('voltPill');
    pill.className = 'volt-pill ' + ({NORMAL:'vp-normal',WARNING:'vp-warning',CRITICAL:'vp-critical'}[s.voltageStatus]||'vp-normal');
    pill.textContent = {NORMAL:'VOLTAGE OK',WARNING:'VOLTAGE HIGH — WARNING',CRITICAL:'VOLTAGE CRITICAL — START BLOCKED'}[s.voltageStatus] || s.voltageStatus;

    const big = $('statusBig'), icon = $('statusIcon'), plain = $('statusPlain');
    if (s.pumpState === 'RUNNING') {
      big.className = 'status-big ok'; big.textContent = 'RUNNING'; icon.textContent = '💧';
      plain.textContent = 'Pump is running normally.';
    } else if (s.pumpState === 'TRIPPED') {
      big.className = 'status-big alarm'; big.textContent = 'SAFETY STOP'; icon.textContent = '⛔';
      const names = (s.tripNames || '').split('|').filter(Boolean);
      const lines = names.map(n => TRIP_PLAIN[n] || n).filter(Boolean);
      plain.textContent = (s.permanentLockout ?
        'PERMANENT LOCKOUT — a fault repeated many times. Press RESET only after fixing the cause.' :
        'The controller stopped the pump for safety: ' + (lines.join('; ') || 'a fault')) + '.';
    } else if (s.powerRestored) {
      big.className = 'status-big warn'; big.textContent = 'POWER RESTORED'; icon.textContent = '🔌';
      plain.textContent = 'Power came back. Press START PUMP to run the pump again (safety feature).';
    } else {
      big.className = 'status-big stop'; big.textContent = 'STOPPED'; icon.textContent = '💤';
      plain.textContent = 'Pump is stopped. Press START PUMP when you need water.';
    }

    const rc = $('reasonCard');
    function intFromName(n) { return parseInt(n, 10) || 0; }
    const tripBits = s.trips || 0;
    if (tripBits || s.permanentLockout || s.autoRetryIn > 0) {
      rc.style.display = 'block';
      const names = (s.tripNames || 'NONE').split('|').filter(n => n !== 'NONE');
      $('reasonBanner').className = 'banner ' + (s.permanentLockout ? 'banner-danger' : 'banner-warn');
      $('reasonBanner').textContent = names.length
        ? ('Reason: ' + names.map(n => TRIP_PLAIN[n] || n).join('; ') + (s.permanentLockout ? ' — PERMANENT LOCKOUT' : ''))
        : (s.permanentLockout ? 'PERMANENT LOCKOUT — press RESET after fixing the fault.' : '');
      $('detRetry').textContent = fmtCountdown(s.autoRetryIn);
      $('detRetriesUsed').textContent = s.retryCount + ' of ' + s.maxRetries;
      $('detFast').textContent = s.fastFaultCount + ' of ' + s.maxFastFaults;
      $('detBlocked').textContent = s.startFailBlock > 0 ? fmtCountdown(s.startFailBlock) + ' (start-fail block)' : (s.powerRestored ? 'until manual start' : '—');
    } else if (s.powerRestored) {
      rc.style.display = 'block';
      $('reasonBanner').className = 'banner banner-info';
      $('reasonBanner').textContent = 'Power restored — the controller waits for you to press START PUMP.';
      $('detRetry').textContent = '—'; $('detRetriesUsed').textContent = '—'; $('detFast').textContent = '—';
      $('detBlocked').textContent = 'until manual start';
    } else {
      rc.style.display = 'none';
    }

    setModeBtn(s.pumpMode);
  } catch(e) {}
}
setInterval(refresh, 3000);
refresh();
</script>
</body>
</html>
)rawliteral";

// ============================================
// DASHBOARD PAGE (/dashboard)
// ============================================
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>motorESP - Dashboard</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E💧%3C/text%3E%3C/svg%3E">
<link href="/lib/bootstrap/css/bootstrap.min.css" rel="stylesheet">
<script src="/lib/chartjs/chart.umd.min.js"></script>
<script src="/lib/dexie/dexie.min.js"></script>
<style>
body { background:#f1f5f9; }
.header { background:linear-gradient(135deg,#1877f2,#0a5ac2); padding:16px; border-radius:12px; color:#fff; text-align:center; margin:12px auto 0; max-width:980px; }
.header h1 { margin:0; font-size:22px; font-weight:800; }
.container { max-width:980px; margin:0 auto; padding:12px; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
.banner-test { background:#fef3c7; border:1px solid #f59e0b; color:#92400e; border-radius:10px; padding:8px 12px; font-weight:700; font-size:13px; margin-bottom:12px; text-align:center; }
.numerics { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; }
@media (min-width:640px){ .numerics { grid-template-columns:repeat(6,1fr); } }
.num-card { background:#f8fafc; padding:12px 6px; border-radius:10px; text-align:center; }
.num-label { font-size:10px; color:#64748b; font-weight:700; text-transform:uppercase; }
.num-value { font-size:24px; font-weight:800; }
.num-unit { font-size:11px; color:#64748b; font-weight:700; }
.badge { padding:6px 14px; border-radius:20px; font-size:12px; font-weight:700; text-transform:uppercase; }
.badge-running { background:#dcfce7; color:#166534; }
.badge-stopped { background:#e2e8f0; color:#334155; }
.badge-tripped { background:#fee2e2; color:#991b1b; }
.chart-box { height:240px; position:relative; }
.voltage-status { padding:4px 10px; border-radius:12px; font-weight:800; font-size:12px; }
.vs-normal { background:#dcfce7; color:#166534; }
.vs-warning { background:#fef3c7; color:#92400e; }
.vs-critical { background:#fee2e2; color:#991b1b; }
.refresh-control { display:flex; align-items:center; gap:8px; font-size:12px; color:rgba(255,255,255,0.9); font-weight:600; justify-content:center; margin-top:8px; }
.hist { font-size:12px; color:#64748b; }
.card h6 { margin:0 0 4px; font-weight:800; color:#334155; }
.card .capexpl { font-size:11.5px; color:#64748b; margin-bottom:8px; }
details { background:#f8fafc; border-radius:10px; padding:10px 12px; }
summary { font-size:13px; font-weight:800; color:#334155; cursor:pointer; }
details li { font-size:12px; color:#475569; line-height:1.5; }
</style>
</head>
<body>
<div class="header">
  <h1>💧 Pump Dashboard</h1>
  <div class="refresh-control">
    <span>Refresh every</span>
    <select id="pollSel" style="font-size:12px;">
      <option value="5000">5s</option><option value="3000">3s</option><option value="2000">2s</option><option value="1000">1s</option>
    </select>
  </div>
</div>
<div class="container">
  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE — numbers below are simulated input, not the real pump.</div>

  <div class="card">
    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px; flex-wrap:wrap; gap:6px;">
      <div>
        <span id="stateBadge" class="badge badge-stopped">OFF</span>
        <span id="tripBadge" class="badge badge-tripped" style="display:none">SAFETY STOP</span>
        <span id="voltageStatus" class="voltage-status vs-normal">VOLTAGE OK</span>
      </div>
      <span class="hist">Uptime <b id="uptime">--</b></span>
    </div>
    <p class="capexpl" id="stateExpl">Waiting for live data…</p>
    <div class="numerics">
      <div class="num-card"><div class="num-label">Voltage</div><div class="num-value"><span id="nVolt">--</span></div></div>
      <div class="num-card"><div class="num-label">Current</div><div class="num-value"><span id="nCur">--</span></div></div>
      <div class="num-card"><div class="num-label">Power</div><div class="num-value"><span id="nPow">--</span></div></div>
      <div class="num-card"><div class="num-label">Energy Used</div><div class="num-value"><span id="nEn">--</span></div></div>
      <div class="num-card"><div class="num-label">Frequency</div><div class="num-value"><span id="nHz">--</span></div></div>
      <div class="num-card"><div class="num-label">Power Factor</div><div class="num-value"><span id="nPF">--</span></div></div>
    </div>
  </div>

  <div class="card"><h6>⚡ Power used (W)</h6><p class="capexpl">How hard the pump is working right now. A running motor draws its normal wattage; a big jump may mean overload.</p><div class="chart-box"><canvas id="chartPower"></canvas></div></div>
  <div class="card"><h6>⚡ Mains voltage (V)</h6><p class="capexpl">Electricity supply voltage. Normal is roughly 200–250&nbsp;V. Below 190&nbsp;V or above 250&nbsp;V triggers safety.</p><div class="chart-box"><canvas id="chartVoltage"></canvas></div></div>
  <div class="card"><h6>⚡ Pump current (A)</h6><p class="capexpl">Electricity flowing into the pump. Near zero while stopped; normal running value depends on the motor (e.g. ~9.6&nbsp;A for a 1.5&nbsp;HP pump).</p><div class="chart-box"><canvas id="chartCurrent"></canvas></div></div>

  <div class="card">
    <details>
      <summary>How to read this page</summary>
      <ul>
        <li><b>Voltage</b> — mains supply level. High voltage damages motors, so the controller watches it closely.</li>
        <li><b>Current</b> — the pump motor's draw. If it exceeds the overload limit, safety stops the pump.</li>
        <li><b>Power</b> — volts × amps in real time. Charts only record while the pump runs or after a fault.</li>
        <li><b>Energy Used</b> — total electricity since the meter last started. Use it to track pumping cost.</li>
        <li><b>Frequency</b> — mains frequency, normally 50&nbsp;Hz. Useful to notice unstable supply.</li>
        <li><b>Power Factor</b> — how efficiently the pump uses electricity (0.5–1.0). Lower values are less efficient.</li>
      </ul>
    </details>
  </div>

  <div class="nav d-flex gap-2">
    <a href="/" class="btn btn-primary btn-sm">🏠 Control</a>
    <a href="/settings" class="btn btn-light btn-sm">⚙️ Settings</a>
    <a href="/data" class="btn btn-light btn-sm">📋 Log</a>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
const pollSel = $('pollSel');
let series = { power:[], voltage:[], current:[] };
let chartPower, chartVoltage, chartCurrent;

function makeChart(id, label, color) {
  return new Chart($(id), { type:'line', data:{ labels:[], datasets:[{ label, data:[], borderColor:color, backgroundColor:color+'22', fill:true, borderWidth:2, pointRadius:0, tension:0.2 }] }, options:{ animation:false, responsive:true, maintainAspectRatio:false, scales:{ y:{ beginAtZero:true } } } });
}
chartPower = makeChart('chartPower','Power (W)','#1877f2');
chartVoltage = makeChart('chartVoltage','Voltage (V)','#f59e0b');
chartCurrent = makeChart('chartCurrent','Current (A)','#16a34a');

function pushChart(chart, seriesArr, val, maxPts) {
  seriesArr.push(val);
  if (seriesArr.length > maxPts) seriesArr.shift();
  chart.data.labels = seriesArr.map((_,i)=>i);
  chart.data.datasets[0].data = seriesArr;
  chart.update();
}
const TRIP_PLAIN = {2:"Overload",4:"No Water",8:"High voltage",16:"Low voltage",64:"Sensor fault",128:"Failed start"};

async function refresh() {
  try {
    const s = await (await fetch('/status')).json();
    $('testBanner').style.display = s.mock ? 'block' : 'none';
    $('nVolt').textContent = s.voltage.toFixed(0) + ' V';
    $('nCur').textContent = s.current.toFixed(1) + ' A';
    $('nPow').textContent = s.power >= 1000 ? (s.power/1000).toFixed(2) + ' kW' : s.power.toFixed(0) + ' W';
    $('nEn').textContent = s.energyKwh.toFixed(2) + ' kWh';
    $('nHz').textContent = s.frequency.toFixed(1) + ' Hz';
    $('nPF').textContent = s.pf.toFixed(2);
    $('uptime').textContent = s.uptime;

    const badge = $('stateBadge');
    badge.className = 'badge ' + (s.pumpState==='RUNNING' ? 'badge-running' : (s.pumpState==='TRIPPED' ? 'badge-tripped' : 'badge-stopped'));
    badge.textContent = s.pumpState;
    $('tripBadge').style.display = s.trips ? 'inline-block' : 'none';

    const vs = $('voltageStatus');
    vs.className = 'voltage-status ' + ({NORMAL:'vs-normal',WARNING:'vs-warning',CRITICAL:'vs-critical'}[s.voltageStatus]||'vs-normal');
    vs.textContent = {NORMAL:'VOLTAGE OK',WARNING:'VOLTAGE HIGH — WARNING',CRITICAL:'VOLTAGE CRITICAL'}[s.voltageStatus] || s.voltageStatus;

    const names = (s.tripNames || 'NONE').split('|').filter(n => n !== 'NONE');
    $('stateExpl').textContent = s.pumpState==='RUNNING' ? 'Pump is running normally.'
      : s.pumpState==='TRIPPED' ? ('Safety stop: ' + (names.map(n => TRIP_PLAIN[n] || n).join(', ') || 'a fault') + (s.permanentLockout ? ' — PERMANENT LOCKOUT' : '') + '.')
      : s.powerRestored ? 'Power was restored — the pump waits for a manual start.'
      : 'Pump is stopped.';

    if (s.pumpState==='RUNNING' || s.pumpState==='TRIPPED') {
      pushChart(chartPower, series.power, s.power, 100);
      pushChart(chartVoltage, series.voltage, s.voltage, 100);
      pushChart(chartCurrent, series.current, s.current, 100);
    }
  } catch(e) {}
}
pollSel.addEventListener('change', () => { clearInterval(window._ti); window._ti = setInterval(refresh, parseInt(pollSel.value)); });
window._ti = setInterval(refresh, parseInt(pollSel.value));
refresh();
</script>
</body>
</html>
)rawliteral";

// ============================================
// SETTINGS PAGE (/settings)
// ============================================
const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>motorESP - Settings</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E💧%3C/text%3E%3C/svg%3E">
<style>
:root { --blue:#1877f2; --green:#16a34a; --red:#dc2626; --amber:#d97706; --slate:#64748b; --bg:#f1f5f9; }
* { box-sizing:border-box; }
body { font-family:system-ui,sans-serif; margin:0; background:var(--bg); color:#0f172a; }
.container { max-width:640px; margin:0 auto; padding:12px; }
.header { background:linear-gradient(135deg,#1877f2,#0a5ac2); padding:14px 16px; border-radius:12px; color:#fff; text-align:center; margin-bottom:12px; }
.header h1 { margin:0; font-size:21px; font-weight:800; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
.card h3 { margin:0 0 4px; font-size:14px; color:#334155; text-transform:uppercase; letter-spacing:.3px; }
.card .cardhelp { font-size:12px; color:var(--slate); margin:0 0 12px; line-height:1.5; }
.field { padding:10px 0; border-bottom:1px solid #f1f5f9; }
.field:last-child { border-bottom:none; }
.field label { font-size:14px; font-weight:700; color:#1e293b; display:block; margin-bottom:2px; }
.field .help { font-size:12px; color:var(--slate); line-height:1.45; margin:0 0 6px; }
.field input, .field select { width:100%; padding:10px; border:1px solid #cbd5e1; border-radius:10px; font-size:15px; background:#fff; }
.field .unit-hint { font-size:11px; color:#94a3b8; margin-top:2px; }
.check-row { display:flex; align-items:flex-start; gap:10px; padding:10px 0; border-bottom:1px solid #f1f5f9; }
.check-row input { width:20px; height:20px; margin-top:1px; flex-shrink:0; }
.check-row .ch-label { font-size:14px; font-weight:700; color:#1e293b; }
.check-row .ch-help { font-size:12px; color:var(--slate); line-height:1.4; }
button.save { width:100%; padding:15px; background:var(--blue); color:#fff; border:none; border-radius:12px; font-size:16px; font-weight:800; cursor:pointer; }
button.save:active { transform:scale(.98); }
button.danger { background:var(--red); margin-top:8px; font-size:14px; padding:12px; }
.msg { min-height:20px; font-size:13px; font-weight:700; text-align:center; margin-top:8px; }
.msg.good { color:var(--green); } .msg.bad { color:var(--red); }
.banner-test { background:#fef3c7; border:1px solid #f59e0b; color:#92400e; border-radius:10px; padding:8px 12px; font-weight:700; font-size:13px; margin-bottom:12px; text-align:center; }
.nav { display:flex; gap:8px; margin-top:12px; }
.nav a { flex:1; text-align:center; padding:11px 4px; background:var(--blue); border-radius:10px; text-decoration:none; color:#fff; font-weight:800; font-size:13px; }
details { background:#f8fafc; border-radius:10px; padding:10px 12px; margin-bottom:10px; }
summary { font-size:12.5px; font-weight:800; color:#334155; cursor:pointer; }
details p { font-size:12px; color:#475569; line-height:1.5; margin:6px 0 0; }
</style>
</head>
<body>
<div class="container">
  <div class="header"><h1>⚙️ Pump Settings</h1></div>
  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE currently ON — simulated readings are used instead of the real power meter.</div>

  <div class="card" id="testCard">
    <h3>🧪 Test Mode</h3>
    <p class="cardhelp">For electricians &amp; testing WITHOUT the power meter connected. It feeds fake readings so you can check protections. <b>Turn it off for normal use.</b></p>
    <div class="field">
      <label><input type="checkbox" id="cMock" style="width:auto;height:auto;transform:scale(1.3);margin-right:8px;vertical-align:middle;"> Use test readings</label>
    </div>
    <div class="field">
      <label for="cMockProfile">Simulate a situation</label>
      <p class="help">Choose what fake reading the controller should receive, then press “Apply test profile”.</p>
      <select id="cMockProfile">
        <option value="off">Pump off (no water flow)</option>
        <option value="running">Pump running normally</option>
        <option value="dryrun">Dry run (no water)</option>
        <option value="oc">Overload (too much current)</option>
      </select>
    </div>
  </div>

  <details><summary>⚠️ Please read before changing anything</summary>
    <p>These settings protect your pump motor from damage. The default values are safe for a typical
      1.5&nbsp;HP (1.1&nbsp;kW) submersible pump drawing ~9.6&nbsp;A. If you are unsure, leave everything
      as it is. Wrong values can prevent the pump from running or let it run without protection.</p>
  </details>

  <div class="card">
    <h3>🛡 Overload Protection</h3>
    <p class="cardhelp">Stops the pump when the motor draws too much electricity — this prevents motor burn-out.</p>
    <div class="field">
      <label for="ocRunning">Normal current limit (A)</label>
      <p class="help">Highest current the pump draws in normal operation. If current stays above this for the delay time below, the pump stops.</p>
      <input type="number" id="ocRunning" step="0.1" min="5" max="50"><div class="unit-hint">Example: a 9.6&nbsp;A pump → set 12&nbsp;A</div>
    </div>
    <div class="field">
      <label for="ocStartInstant">Start-up current limit (A)</label>
      <p class="help">Motors briefly draw extra current when starting. If current jumps above this instantly during start-up, it's a real fault.</p>
      <input type="number" id="ocStartInstant" min="20" max="100">
    </div>
    <div class="field">
      <label for="ocDelay">Acknowledge time (s)</label>
      <p class="help">How many seconds the overload must last before the pump stops. Short = reacts fast; long = ignores brief spikes.</p>
      <input type="number" id="ocDelay" min="1" max="30">
    </div>
  </div>

  <div class="card">
    <h3>💧 No-Water Protection (Dry Run)</h3>
    <p class="cardhelp">Running without water damages a pump quickly. This stops it when it draws too little power for too long.</p>
    <div class="field">
      <label for="dryRunCurrent">No-water current limit (A)</label>
      <p class="help">Current while the pump runs dry is usually well below normal. Below this limit counts as “no water”.</p>
      <input type="number" id="dryRunCurrent" step="0.1" min="1" max="10">
    </div>
    <div class="field">
      <label for="dryRunPower">No-water power limit (W)</label>
      <p class="help">Power while dry is also low — both current AND power must be low to confirm dry-run.</p>
      <input type="number" id="dryRunPower" min="100" max="2000">
    </div>
    <div class="field">
      <label for="dryRunDelay">Confirm time (s)</label>
      <p class="help">How long the low reading must continue before the controller stops the pump.</p>
      <input type="number" id="dryRunDelay" min="1" max="300">
    </div>
    <div class="field">
      <label for="dryRunActivation">Wait after start (s)</label>
      <p class="help">The pump needs time to draw water up after starting. Protection is switched ON only after this delay.</p>
      <input type="number" id="dryRunActivation" min="0" max="3600">
    </div>
  </div>

  <div class="card">
    <h3>⚡ Voltage Protection</h3>
    <p class="cardhelp">High or low mains voltage is dangerous for the motor. The controller watches and stops the pump.</p>
    <div class="field">
      <label for="voltOver">High voltage stop (V)</label>
      <p class="help">If voltage stays above this while running, the pump stops after the delay below.</p>
      <input type="number" id="voltOver" min="200" max="280">
    </div>
    <div class="field">
      <label for="voltUnder">Low voltage stop (V)</label>
      <p class="help">If voltage stays below this while running, the pump stops. Prevents motor damage from brown-out.</p>
      <input type="number" id="voltUnder" min="150" max="230">
    </div>
    <div class="field">
      <label for="voltWarn">Pre-start warning (V)</label>
      <p class="help">Above this, the screen shows a WARNING before starting. Start is still allowed.</p>
      <input type="number" id="voltWarn" min="240" max="280">
    </div>
    <div class="field">
      <label for="voltCritical">Pre-start block (V)</label>
      <p class="help">Above this voltage the pump is NOT allowed to start at all, until voltage falls again.</p>
      <input type="number" id="voltCritical" min="250" max="300">
    </div>
    <div class="field">
      <label for="voltageDelay">Confirm time (s)</label>
      <p class="help">How long the bad voltage must continue before the pump stops.</p>
      <input type="number" id="voltageDelay" min="1" max="60">
    </div>
    <div class="field">
      <label for="voltageLockout">Lockout after high/low voltage (s)</label>
      <p class="help">After a voltage trip the pump stays locked (cannot restart) for this long, even in AUTO mode.</p>
      <input type="number" id="voltageLockout" min="0" max="3600">
    </div>
  </div>

  <div class="card">
    <h3>🔌 Starting &amp; Stopping</h3>
    <p class="cardhelp">When the controller considers the pump “started”, and how long it protects the motor between actions.</p>
    <div class="field">
      <label for="startSuccessCurrent">Start success current (A)</label>
      <p class="help">If current rises above this shortly after the start pulse, the pump has really started.</p>
      <input type="number" id="startSuccessCurrent" step="0.1" min="0.5" max="5">
    </div>
    <div class="field">
      <label for="startVerifyDelay">Check after (s)</label>
      <p class="help">Seconds after the start pulse before the controller checks whether the motor started.</p>
      <input type="number" id="startVerifyDelay" min="1" max="10">
    </div>
    <div class="field">
      <label for="startFailBlock">Failure retry block (s)</label>
      <p class="help">If the motor didn't start, automatic restart is blocked for this long (you can still press START manually).</p>
      <input type="number" id="startFailBlock" min="1" max="600">
    </div>
    <div class="field">
      <label for="minRun">Minimum run time (s)</label>
      <p class="help">The pump must run at least this long before an OFF press is accepted — prevents rapid on/off wear.</p>
      <input type="number" id="minRun" min="10" max="300">
    </div>
    <div class="field">
      <label for="minOff">Minimum off time (s)</label>
      <p class="help">After stopping, the pump must rest at least this long before it can start again.</p>
      <input type="number" id="minOff" min="10" max="600">
    </div>
  </div>

  <div class="card">
    <h3>🔁 Automatic Restart After a Fault</h3>
    <p class="cardhelp">Choose whether the pump tries to restart by itself after a safety trip — or waits for you. If a fault repeats 3 times quickly, the pump locks permanently and only a manual reset helps.</p>
    <div class="field">
      <label for="autoRetryDelay">Wait before retry (s)</label>
      <p class="help">How long after a trip before an automatic retry happens (e.g. 300&nbsp;s = 5 minutes).</p>
      <input type="number" id="autoRetryDelay" min="60" max="3600">
    </div>
    <div class="field">
      <label for="maxRetries">Maximum retries</label>
      <p class="help">How many automatic retries are allowed for one fault before the pump waits for you.</p>
      <input type="number" id="maxRetries" min="1" max="10">
    </div>
    <div class="check-row">
      <input type="checkbox" id="tb0"><div><div class="ch-label">Auto-retry after overload</div><div class="ch-help">Recommended for overload (e.g. brief blockage that clears itself).</div></div>
    </div>
    <div class="check-row">
      <input type="checkbox" id="tb1"><div><div class="ch-label">Auto-retry after no-water (dry run)</div><div class="ch-help">Not recommended — the pump keeps running dry if retried.</div></div>
    </div>
    <div class="check-row">
      <input type="checkbox" id="tb2"><div><div class="ch-label">Auto-retry after high voltage</div><div class="ch-help">Voltage usually must be fixed at the mains; retry may just trip again.</div></div>
    </div>
    <div class="check-row">
      <input type="checkbox" id="tb3"><div><div class="ch-label">Auto-retry after low voltage</div><div class="ch-help">Like high voltage, usually a mains problem — manual start after it clears is safer.</div></div>
    </div>
    <div class="check-row">
      <input type="checkbox" id="tb4"><div><div class="ch-label">Auto-retry after sensor fault</div><div class="ch-help">If the power meter fails, monitoring is blind — retry can be dangerous.</div></div>
    </div>
    <div class="check-row">
      <input type="checkbox" id="tb5"><div><div class="ch-label">Auto-retry after failed start</div><div class="ch-help">If the motor refuses to start, retrying stresses the motor. Manual check is safer.</div></div>
    </div>
  </div>

  <div class="card">
    <h3>📊 Recording &amp; Polling</h3>
    <p class="cardhelp">How often the device saves data to its memory and checks the meter. Smaller numbers = more detail, slightly more wear.</p>
    <div class="field">
      <label for="logIntervalRunning">Save every (s) — while running</label>
      <input type="number" id="logIntervalRunning" min="5" max="60">
    </div>
    <div class="field">
      <label for="logIntervalOff">Save every (s) — while stopped</label>
      <input type="number" id="logIntervalOff" min="30" max="600">
    </div>
    <div class="field">
      <label for="pzemReadRunning">Read meter every (s) — while running</label>
      <input type="number" id="pzemReadRunning" min="1" max="5">
    </div>
  </div>

  <button class="save" id="btnSave">💾 SAVE ALL SETTINGS</button>
  <button class="save danger" id="btnClearData">🗑 Clear Recorded History (restarts device)</button>
  <div class="msg" id="msg"></div>

  <div class="nav">
    <a href="/">🏠 Control</a>
    <a href="/dashboard">📊 Dashboard</a>
    <a href="/data">📋 Log</a>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
const fields = ['ocRunning','ocStartInstant','ocDelay','dryRunCurrent','dryRunPower','dryRunDelay','dryRunActivation',
  'voltOver','voltUnder','voltWarn','voltCritical','voltageDelay','voltageLockout',
  'startSuccessCurrent','startVerifyDelay','startFailBlock','minRun','minOff',
  'autoRetryDelay','maxRetries','logIntervalRunning','logIntervalOff','pzemReadRunning'];
const mockProfileMap = {0:'off',1:'running',2:'dryrun',3:'oc'};

async function load() {
  const s = await (await fetch('/settings/api')).json();
  $('testBanner').style.display = s.mock ? 'block' : 'none';
  $('cMock').checked = s.mock;
  fields.forEach(f => { if ($(f)) $(f).value = s[f]; });
  $('tb0').checked = !!(s.tripBehavior & 1);
  $('tb1').checked = !!(s.tripBehavior & 2);
  $('tb2').checked = !!(s.tripBehavior & 4);
  $('tb3').checked = !!(s.tripBehavior & 8);
  $('tb4').checked = !!(s.tripBehavior & 16);
  $('tb5').checked = !!(s.tripBehavior & 32);
}

function feed(t, cls) {
  const el = $('msg'); el.textContent = t; el.className = 'msg ' + (cls || '');
  setTimeout(() => { if (el.textContent === t) { el.textContent = ''; el.className = 'msg'; } }, 4000);
}

$('btnSave').onclick = async () => {
  const p = new URLSearchParams();
  fields.forEach(f => { if ($(f)) p.append(f, $(f).value); });
  p.append('mock', $('cMock').checked ? '1' : '0');
  let tb = 0;
  if ($('tb0').checked) tb |= 1;
  if ($('tb1').checked) tb |= 2;
  if ($('tb2').checked) tb |= 4;
  if ($('tb3').checked) tb |= 8;
  if ($('tb4').checked) tb |= 16;
  if ($('tb5').checked) tb |= 32;
  p.append('tripBehavior', tb);
  const r = await fetch('/settings/api?' + p.toString(), { method:'POST' });
  feed(r.status === 200 ? '✓ All settings saved' : '✗ Save failed (' + r.status + ')', r.status === 200 ? 'good' : 'bad');
  $('testBanner').style.display = $('cMock').checked ? 'block' : 'none';
};

$('cMockProfile').addEventListener('change', () => {
  fetch('/settings/api?mockProfile=' + $('cMockProfile').value, { method:'POST' })
    .then(() => feed('✓ Test profile applied', 'good'));
  $('cMock').checked = true;
  $('testBanner').style.display = 'block';
});

$('btnClearData').onclick = () => {
  if (!confirm('Erase ALL recorded history and restart the device? This cannot be undone.')) return;
  fetch('/settings/clear').then(() => feed('Erasing… device will restart', ''));
};
load();
</script>
</body>
</html>
)rawliteral";

// ============================================
// DATA PAGE (/data)
// ============================================
const char DATA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>motorESP - Data Log</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E💧%3C/text%3E%3C/svg%3E">
<style>
:root { --blue:#1877f2; --green:#16a34a; --red:#dc2626; --amber:#d97706; --slate:#64748b; --bg:#f1f5f9; }
* { box-sizing:border-box; }
body { font-family:system-ui,sans-serif; margin:0; background:var(--bg); color:#0f172a; }
.container { max-width:820px; margin:0 auto; padding:12px; }
.header { background:linear-gradient(135deg,#1877f2,#0a5ac2); padding:14px 16px; border-radius:12px; color:#fff; text-align:center; margin-bottom:12px; }
.header h1 { margin:0; font-size:21px; font-weight:800; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
.card h3 { margin:0 0 4px; font-size:14px; color:#334155; text-transform:uppercase; letter-spacing:.3px; }
.card .cardhelp { font-size:12px; color:var(--slate); margin:0 0 10px; line-height:1.5; }
table { width:100%; border-collapse:collapse; font-size:12px; }
th,td { padding:7px 6px; border-bottom:1px solid #f1f5f9; text-align:center; }
th { background:#f8fafc; color:#64748b; text-transform:uppercase; font-size:10px; }
.load-more { width:100%; padding:13px; border:2px solid #e2e8f0; background:#fff; border-radius:10px; font-weight:800; cursor:pointer; font-size:14px; color:#334155; }
.load-more:active { background:#f1f5f9; }
.summary { font-size:12px; color:#475569; margin-bottom:8px; }
.bits { font-size:10px; font-weight:700; }
.bits .chip { display:inline-block; padding:2px 6px; border-radius:8px; margin:1px; white-space:nowrap; }
.chip-run { background:#dcfce7; color:#166534; }
.chip-fault { background:#fee2e2; color:#991b1b; }
.chip-auto { background:#dbeafe; color:#1e40af; }
.chip-off { background:#f1f5f9; color:#475569; }
.details { display:flex; flex-wrap:wrap; gap:6px; margin-top:10px; }
.details span { font-size:11px; background:#f8fafc; border:1px solid #e2e8f0; padding:4px 8px; border-radius:8px; color:#475569; }
.nav { display:flex; gap:8px; margin-top:12px; }
.nav a { flex:1; text-align:center; padding:11px 4px; background:var(--blue); border-radius:10px; text-decoration:none; color:#fff; font-weight:800; font-size:13px; }
details { background:#f8fafc; border-radius:10px; padding:10px 12px; }
summary { font-size:12.5px; font-weight:800; color:#334155; cursor:pointer; }
details li { font-size:12px; color:#475569; line-height:1.5; }
.banner-test { background:#fef3c7; border:1px solid #f59e0b; color:#92400e; border-radius:10px; padding:8px 12px; font-weight:700; font-size:13px; margin-bottom:12px; text-align:center; }
</style>
</head>
<body>
<div class="container">
  <div class="header"><h1>📋 Pump History</h1></div>
  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE — log contains simulated readings.</div>

  <div class="card">
    <h3>Recorded readings</h3>
    <p class="cardhelp">Every row is one automatic measurement. Older rows appear as you press <b>LOAD MORE</b>.
       The log survives power cuts (stored in the device's flash memory) and holds ~95,000 entries.</p>
    <div class="summary" id="summary">Loading…</div>
    <table>
      <thead><tr><th>Session</th><th>Time (s)</th><th>Voltage (V)</th><th>Current (A)</th><th>Used (Wh)</th><th>Power Factor</th><th>Status</th></tr></thead>
      <tbody id="rows"></tbody>
    </table>
    <div style="height:10px"></div>
    <button class="load-more" id="loadMore">⬇ LOAD MORE</button>
    <div class="details">
      <span><b>Session</b> — number of power-ons</span>
      <span><b>Time</b> — seconds since that power-on</span>
      <span><b>Used</b> — energy since previous row</span>
      <span><b>PF</b> — 0.5–1.0, efficiency</span>
      <span><b>Status</b> — what the pump was doing</span>
    </div>
  </div>

  <div class="card">
    <details>
      <summary>What do the status words mean?</summary>
      <ul>
        <li><b>RUNNING</b> — pump on and drawing current.</li>
        <li><b>OVERLOAD</b> — stopped by the overload protection (too much current).</li>
        <li><b>NO WATER</b> — stopped by dry-run protection (pump ran without water).</li>
        <li><b>HIGH VOLTAGE</b> / <b>LOW VOLTAGE</b> — stopped because mains voltage was outside safe range.</li>
        <li><b>PZEM FAULT</b> — the power meter couldn't be read; pump stopped as a safety measure.</li>
        <li><b>FAILED START</b> — the start pulse was sent but the motor never drew current.</li>
        <li><b>AUTO</b> — the device was in automatic mode at that moment.</li>
        <li><b>OFF</b> — pump stopped, no fault.</li>
      </ul>
    </details>
  </div>

  <div class="nav">
    <a href="/">🏠 Control</a>
    <a href="/dashboard">📊 Dashboard</a>
    <a href="/settings">⚙️ Settings</a>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let sinceBoot = 0, sinceTime = 0;
const RUN=1, OC=2, DRY=4, OV=8, UV=16, AUTO=32, PZEM=64, SFAIL=128;

function hexToBytes(h){ const a=[]; for(let i=0;i<h.length;i+=2) a.push(parseInt(h.substr(i,2),16)); return a; }

function chips(st) {
  const c = [];
  if (st & RUN) c.push('<span class="chip chip-run">RUNNING</span>');
  if (st & OC) c.push('<span class="chip chip-fault">OVERLOAD</span>');
  if (st & DRY) c.push('<span class="chip chip-fault">NO WATER</span>');
  if (st & OV) c.push('<span class="chip chip-fault">HIGH VOLT</span>');
  if (st & UV) c.push('<span class="chip chip-fault">LOW VOLT</span>');
  if (st & PZEM) c.push('<span class="chip chip-fault">PZEM FAULT</span>');
  if (st & SFAIL) c.push('<span class="chip chip-fault">FAILED START</span>');
  if (st & AUTO) c.push('<span class="chip chip-auto">AUTO</span>');
  if (!c.length) c.push('<span class="chip chip-off">OFF</span>');
  return '<span class="bits">' + c.join('') + '</span>';
}

async function loadMore() {
  let url = '/data/api?count=100';
  if (sinceBoot || sinceTime) url += '&boot=' + sinceBoot + '&time=' + sinceTime;
  const d = await (await fetch(url)).json();
  if (sinceBoot === 0 && sinceTime === 0) $('rows').innerHTML = '';
  const rows = hexToBytes(d.logs);
  const tbody = $('rows');
  const firstNew = tbody.children.length === 0;
  let added = 0;
  for (let i = 0; i + 11 <= rows.length; i += 11) {
    const b = rows.slice(i, i + 11);
    const timeSec = b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24);
    const volt = 200 + b[4];
    const cur = ((b[5]|(b[6]<<8))/10).toFixed(1);
    const pf = b[8] >= 254 ? '-' : (b[8]/100).toFixed(2);
    const st = b[9];
    const boot = b[10];
    const tr = document.createElement('tr');
    tr.innerHTML = '<td>'+boot+'</td><td>'+timeSec+'</td><td>'+volt+'</td><td>'+cur+'</td><td>'+b[7]+'</td><td>'+pf+'</td><td>'+chips(st)+'</td>';
    tbody.appendChild(tr);
    sinceBoot = boot; sinceTime = timeSec;
    added++;
  }
  if (firstNew && added === 0 && d.totalLogs === 0) {
    $('summary').textContent = 'No recordings yet — the device saves automatically while running.';
  } else if (firstNew) {
    $('summary').textContent = 'Total recordings: ' + d.totalLogs + (added ? ' · showing ' + added + ' newest' : '');
  }
}
$('loadMore').onclick = loadMore;
loadMore();
fetch('/status').then(r=>r.json()).then(s => { $('testBanner').style.display = s.mock ? 'block' : 'none'; });
</script>
</body>
</html>
)rawliteral";

#endif