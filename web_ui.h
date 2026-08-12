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
<title>motorESP - Control</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E💧%3C/text%3E%3C/svg%3E">
<style>
:root { --blue:#1877f2; --green:#2ecc71; --red:#e74c3c; --amber:#f39c12; --bg:#f4f6f8; }
* { box-sizing:border-box; }
body { font-family:system-ui,sans-serif; margin:0; background:var(--bg); color:#1c1e21; }
.container { max-width:560px; margin:0 auto; padding:12px; }
.header { background:linear-gradient(135deg,var(--blue),#0a5ac2); padding:16px; border-radius:12px; color:#fff; text-align:center; margin-bottom:12px; }
.header h1 { margin:0; font-size:22px; font-weight:800; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
.stat-grid { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; }
.stat-card { background:#f8fafc; padding:10px; border-radius:10px; text-align:center; }
.stat-label { font-size:10px; color:#64748b; font-weight:700; text-transform:uppercase; }
.stat-value { font-size:20px; font-weight:800; margin-top:2px; }
.btn { display:block; width:100%; padding:16px; border:none; border-radius:12px; font-size:18px; font-weight:800; color:#fff; cursor:pointer; margin-bottom:10px; }
.btn-on { background:var(--green); }
.btn-off { background:var(--red); }
.btn-reset { background:#64748b; padding:10px; font-size:14px; }
.mode-row { display:flex; gap:8px; }
.mode-btn { flex:1; padding:12px; border:2px solid #e2e8f0; background:#fff; border-radius:10px; font-weight:700; cursor:pointer; }
.mode-btn.active { border-color:var(--blue); color:var(--blue); background:#eff6ff; }
.badges { display:flex; gap:6px; flex-wrap:wrap; margin-bottom:10px; }
.badge { padding:6px 12px; border-radius:20px; font-size:11px; font-weight:700; text-transform:uppercase; }
.badge-running { background:#e8f8ef; color:var(--green); }
.badge-stopped { background:#fef2f2; color:var(--red); }
.badge-tripped { background:#fff4e5; color:var(--amber); }
.badge-warn { background:#fff; color:var(--amber); border:1px solid var(--amber); }
.banner { background:#fff4e5; border:1px solid var(--amber); color:#92400e; padding:10px; border-radius:10px; font-weight:700; font-size:13px; margin-bottom:12px; text-align:center; }
.msg { margin-top:8px; font-size:12px; font-weight:700; min-height:16px; }
.nav { display:flex; gap:8px; }
.nav a { flex:1; text-align:center; padding:10px; background:#e2e8f0; border-radius:10px; text-decoration:none; color:#334155; font-weight:700; font-size:14px; }
</style>
</head>
<body>
<div class="container">
  <div class="header"><h1>💧 motorESP</h1></div>

  <div class="card">
    <div class="badges">
      <span id="stateBadge" class="badge badge-stopped">OFF</span>
      <span id="tripBadge" class="badge badge-warn" style="display:none">TRIPPED</span>
      <span id="modeBadge" class="badge badge-stopped">MANUAL</span>
    </div>
    <div class="stat-grid">
      <div class="stat-card"><div class="stat-label">Voltage</div><div class="stat-value" id="stVolt">--</div></div>
      <div class="stat-card"><div class="stat-label">Current</div><div class="stat-value" id="stCur">--</div></div>
      <div class="stat-card"><div class="stat-label">Power</div><div class="stat-value" id="stPow">--</div></div>
    </div>
    <div class="msg" id="msg"></div>
  </div>

  <div class="card">
    <div class="mode-row">
      <button class="mode-btn" data-mode="0">OFF</button>
      <button class="mode-btn" data-mode="1">MANUAL</button>
      <button class="mode-btn" data-mode="2">AUTO</button>
    </div>
  </div>

  <div class="card">
    <button class="btn btn-on" id="btnStart">PUMP ON</button>
    <button class="btn btn-off" id="btnStop">PUMP OFF</button>
    <button class="btn btn-reset" id="btnReset">RESET TRIPS</button>
  </div>

  <div class="nav">
    <a href="/dashboard">Dashboard</a>
    <a href="/settings">Settings</a>
    <a href="/data">Data</a>
    <a href="/sector_viewer">Sectors</a>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let mode = 1;

function setModeBtn(m) {
  mode = m;
  document.querySelectorAll('.mode-btn').forEach(b => b.classList.toggle('active', parseInt(b.dataset.mode) === m));
}
document.querySelectorAll('.mode-btn').forEach(b => b.addEventListener('click', () => {
  fetch('/control?action=mode&mode=' + b.dataset.mode).then(r => r.text()).then(m => showMsg(m));
}));

function showMsg(m){ $('msg').textContent = m; setTimeout(()=>{ if($('msg').textContent===m) $('msg').textContent=''; }, 4000); }
function doAct(a){ return fetch('/control?action=' + a).then(r => r.text()).then(m => showMsg(m)); }
$('btnStart').onclick = () => doAct('start');
$('btnStop').onclick = () => doAct('stop');
$('btnReset').onclick = () => doAct('reset');

async function refresh() {
  try {
    const s = await (await fetch('/status')).json();
    $('stVolt').textContent = s.voltage.toFixed(0);
    $('stCur').textContent = s.current.toFixed(1);
    $('stPow').textContent = s.power >= 1000 ? (s.power/1000).toFixed(2)+'k' : s.power.toFixed(0);
    const badge = $('stateBadge');
    badge.className = 'badge ' + (s.pumpState==='RUNNING' ? 'badge-running' : (s.pumpState==='TRIPPED' ? 'badge-tripped' : 'badge-stopped'));
    badge.textContent = s.pumpState;
    $('tripBadge').style.display = s.trips ? 'block' : 'none';
    $('modeBadge').textContent = ['OFF','MANUAL','AUTO'][s.pumpMode] || '?';
    setModeBtn(s.pumpMode);
    if (s.trips > 0) $('msg').textContent = 'TRIP: ' + s.tripNames + (s.permanentLockout ? ' (PERMANENT — RESET REQUIRED)' : '');
    else if (s.powerRestored) $('msg').textContent = 'POWER RESTORED — MANUAL START REQUIRED';
    else $('msg').textContent = '';
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
body { background:#f4f6f8; }
.header { background:linear-gradient(135deg,#1877f2,#0a5ac2); padding:16px; border-radius:12px; color:#fff; text-align:center; margin:12px auto 0; max-width:980px; }
.header h1 { margin:0; font-size:22px; font-weight:800; }
.container { max-width:980px; margin:0 auto; padding:12px; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
.numerics { display:grid; grid-template-columns:repeat(6,1fr); gap:8px; }
.num-card { background:#f8fafc; padding:12px; border-radius:10px; text-align:center; }
.num-label { font-size:10px; color:#64748b; font-weight:700; text-transform:uppercase; }
.num-value { font-size:26px; font-weight:800; }
.badge { padding:6px 14px; border-radius:20px; font-size:12px; font-weight:700; text-transform:uppercase; }
.badge-running { background:#e8f8ef; color:#16a34a; }
.badge-stopped { background:#fef2f2; color:#dc2626; }
.badge-tripped { background:#fff4e5; color:#d97706; }
.chart-box { height:260px; position:relative; }
.voltage-status { padding:4px 10px; border-radius:12px; font-weight:700; font-size:12px; }
.vs-normal { background:#e8f8ef; color:#16a34a; }
.vs-warning { background:#fff4e5; color:#d97706; }
.vs-critical { background:#fef2f2; color:#dc2626; }
.refresh-control { display:flex; align-items:center; gap:8px; font-size:12px; color:rgba(255,255,255,0.9); font-weight:600; }
.hist { font-size:12px; color:#64748b; }
</style>
</head>
<body>
<div class="header">
  <h1>💧 motorESP Dashboard</h1>
  <div class="refresh-control" style="justify-content:center; margin-top:8px;">
    <span>Poll</span>
    <select id="pollSel" style="font-size:12px;">
      <option value="5000">5s</option><option value="3000">3s</option><option value="2000">2s</option><option value="1000">1s</option>
    </select>
  </div>
</div>
<div class="container">
  <div class="card">
    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px;">
      <div>
        <span id="stateBadge" class="badge badge-stopped">OFF</span>
        <span id="tripBadge" class="badge badge-tripped" style="display:none">TRIPPED</span>
        <span id="voltageStatus" class="voltage-status vs-normal">NORMAL</span>
      </div>
      <span style="font-size:12px; color:#64748b;">Uptime <b id="uptime">--</b></span>
    </div>
    <div class="numerics">
      <div class="num-card"><div class="num-label">Voltage</div><div class="num-value" id="nVolt">--</div></div>
      <div class="num-card"><div class="num-label">Current</div><div class="num-value" id="nCur">--</div></div>
      <div class="num-card"><div class="num-label">Power</div><div class="num-value" id="nPow">--</div></div>
      <div class="num-card"><div class="num-label">Energy</div><div class="num-value" id="nEn">--</div></div>
      <div class="num-card"><div class="num-label">Freq</div><div class="num-value" id="nHz">--</div></div>
      <div class="num-card"><div class="num-label">PF</div><div class="num-value" id="nPF">--</div></div>
    </div>
  </div>

  <div class="card"><h6>Power (W)</h6><div class="chart-box"><canvas id="chartPower"></canvas></div></div>
  <div class="card"><h6>Voltage (V)</h6><div class="chart-box"><canvas id="chartVoltage"></canvas></div></div>
  <div class="card"><h6>Current (A)</h6><div class="chart-box"><canvas id="chartCurrent"></canvas></div></div>

  <div class="nav d-flex gap-2">
    <a href="/" class="btn btn-light btn-sm">Control</a>
    <a href="/settings" class="btn btn-light btn-sm">Settings</a>
    <a href="/data" class="btn btn-light btn-sm">Data</a>
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
chartPower = makeChart('chartPower','Power','#1877f2');
chartVoltage = makeChart('chartVoltage','Voltage','#f39c12');
chartCurrent = makeChart('chartCurrent','Current','#2ecc71');

function pushChart(chart, seriesArr, label, maxPts) {
  seriesArr.push(label);
  if (seriesArr.length > maxPts) seriesArr.shift();
  chart.data.labels = seriesArr.map((_,i)=>i);
  chart.data.datasets[0].data = seriesArr;
  chart.update();
}

async function refresh() {
  try {
    const s = await (await fetch('/status')).json();
    $('nVolt').textContent = s.voltage.toFixed(0);
    $('nCur').textContent = s.current.toFixed(1);
    $('nPow').textContent = s.power >= 1000 ? (s.power/1000).toFixed(2)+' kW' : s.power.toFixed(0)+' W';
    $('nEn').textContent = s.energyKwh.toFixed(2)+' kWh';
    $('nHz').textContent = s.frequency.toFixed(1);
    $('nPF').textContent = s.pf.toFixed(2);
    $('uptime').textContent = s.uptime;
    const badge = $('stateBadge');
    badge.className = 'badge ' + (s.pumpState==='RUNNING' ? 'badge-running' : (s.pumpState==='TRIPPED' ? 'badge-tripped' : 'badge-stopped'));
    badge.textContent = s.pumpState + (s.trips ? ' (' + s.tripNames + ')' : '');
    $('tripBadge').style.display = s.powerRestored ? 'none' : 'none';
    const vs = $('voltageStatus');
    vs.className = 'voltage-status ' + ({NORMAL:'vs-normal',WARNING:'vs-warning',CRITICAL:'vs-critical'}[s.voltageStatus]||'vs-normal');
    vs.textContent = s.voltageStatus;
    if (s.pumpState==='RUNNING' || s.pumpState==='TRIPPED') {
      pushChart(chartPower, series.power, s.power, 100);
      pushChart(chartVoltage, series.voltage, s.voltage, 100);
      pushChart(chartCurrent, series.current, s.current, 100);
    }
  } catch(e) {}
}
setInterval(refresh, 5000);
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
body { font-family:system-ui,sans-serif; margin:0; background:#f4f6f8; color:#1c1e21; }
.container { max-width:640px; margin:0 auto; padding:12px; }
.header { background:linear-gradient(135deg,#1877f2,#0a5ac2); padding:16px; border-radius:12px; color:#fff; text-align:center; margin-bottom:12px; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
.card h3 { margin:0 0 10px; font-size:15px; color:#334155; }
.row { display:flex; justify-content:space-between; align-items:center; padding:6px 0; border-bottom:1px solid #f1f5f9; }
.row label { font-size:13px; color:#475569; }
.row input, .row select { width:130px; padding:6px; border:1px solid #cbd5e1; border-radius:8px; font-size:13px; }
button.save { width:100%; padding:14px; background:var(--blue,#1877f2); color:#fff; border:none; border-radius:12px; font-size:16px; font-weight:800; cursor:pointer; }
button.save:hover { background:#0a5ac2; }
.msg { min-height:18px; font-size:12px; font-weight:700; text-align:center; margin-top:8px; }
.nav { display:flex; gap:8px; }
.nav a { flex:1; text-align:center; padding:10px; background:#e2e8f0; border-radius:10px; text-decoration:none; color:#334155; font-weight:700; font-size:14px; }
.checkbox { width:auto !important; }
</style>
</head>
<body>
<div class="container">
  <div class="header"><h1>💧 motorESP Settings</h1></div>

  <div class="card">
    <h3>Mock Mode</h3>
    <div class="row"><label>Enable Mock PZEM</label><input type="checkbox" id="cMock" class="checkbox"></div>
    <div class="row"><label>Mock Profile</label>
      <select id="cMockProfile"><option value="off">Off</option><option value="running">Running</option><option value="dryrun">Dry Run</option><option value="oc">Overcurrent</option></select></div>
  </div>

  <div class="card">
    <h3>Overcurrent</h3>
    <div class="row"><label>Running threshold (A)</label><input type="number" id="ocRunning" step="0.1"></div>
    <div class="row"><label>Start instant (A)</label><input type="number" id="ocStartInstant"></div>
    <div class="row"><label>OC delay (s)</label><input type="number" id="ocDelay"></div>
  </div>

  <div class="card">
    <h3>Dry Run</h3>
    <div class="row"><label>Current (A)</label><input type="number" id="dryRunCurrent" step="0.1"></div>
    <div class="row"><label>Power (W)</label><input type="number" id="dryRunPower"></div>
    <div class="row"><label>Delay (s)</label><input type="number" id="dryRunDelay"></div>
    <div class="row"><label>Activation after start (s)</label><input type="number" id="dryRunActivation"></div>
  </div>

  <div class="card">
    <h3>Voltage</h3>
    <div class="row"><label>Over run (V)</label><input type="number" id="voltOver"></div>
    <div class="row"><label>Under run (V)</label><input type="number" id="voltUnder"></div>
    <div class="row"><label>Pre-start warning (V)</label><input type="number" id="voltWarn"></div>
    <div class="row"><label>Pre-start critical (V)</label><input type="number" id="voltCritical"></div>
    <div class="row"><label>Voltage delay (s)</label><input type="number" id="voltageDelay"></div>
    <div class="row"><label>Voltage lockout (s)</label><input type="number" id="voltageLockout"></div>
  </div>

  <div class="card">
    <h3>Start Logic</h3>
    <div class="row"><label>Start success current (A)</label><input type="number" id="startSuccessCurrent" step="0.1"></div>
    <div class="row"><label>Verify delay (s)</label><input type="number" id="startVerifyDelay"></div>
    <div class="row"><label>Start fail block (s)</label><input type="number" id="startFailBlock"></div>
    <div class="row"><label>Min run (s)</label><input type="number" id="minRun"></div>
    <div class="row"><label>Min off (s)</label><input type="number" id="minOff"></div>
  </div>

  <div class="card">
    <h3>Auto-Retry</h3>
    <div class="row"><label>Retry delay (s)</label><input type="number" id="autoRetryDelay"></div>
    <div class="row"><label>Max retries</label><input type="number" id="maxRetries"></div>
    <div class="row"><label>⏱ OC: Retry</label><input type="checkbox" id="tb0" class="checkbox"></div>
    <div class="row"><label>⏱ DryRun: Retry</label><input type="checkbox" id="tb1" class="checkbox"></div>
    <div class="row"><label>⏱ OverVolt: Retry</label><input type="checkbox" id="tb2" class="checkbox"></div>
    <div class="row"><label>⏱ UnderVolt: Retry</label><input type="checkbox" id="tb3" class="checkbox"></div>
    <div class="row"><label>⏱ PZEM Fault: Retry</label><input type="checkbox" id="tb4" class="checkbox"></div>
    <div class="row"><label>⏱ StartFail: Retry</label><input type="checkbox" id="tb5" class="checkbox"></div>
  </div>

  <div class="card">
    <h3>Logging</h3>
    <div class="row"><label>Log interval running (s)</label><input type="number" id="logIntervalRunning"></div>
    <div class="row"><label>Log interval off (s)</label><input type="number" id="logIntervalOff"></div>
    <div class="row"><label>PZEM read running (s)</label><input type="number" id="pzemReadRunning"></div>
  </div>

  <button class="save" id="btnSave">SAVE SETTINGS</button>
  <div class="msg" id="msg"></div>

  <div class="nav" style="margin-top:12px;">
    <a href="/">Control</a>
    <a href="/dashboard">Dashboard</a>
    <a href="/data">Data</a>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
const fields = ['ocRunning','ocStartInstant','ocDelay','dryRunCurrent','dryRunPower','dryRunDelay','dryRunActivation',
  'voltOver','voltUnder','voltWarn','voltCritical','voltageDelay','voltageLockout',
  'startSuccessCurrent','startVerifyDelay','startFailBlock','minRun','minOff',
  'autoRetryDelay','maxRetries','logIntervalRunning','logIntervalOff','pzemReadRunning'];

async function load() {
  const s = await (await fetch('/settings/api')).json();
  $('cMock').checked = s.mock;
  fields.forEach(f => { if ($(f)) $(f).value = s[f]; });
  $('tb0').checked = !!(s.tripBehavior & 1);
  $('tb1').checked = !!(s.tripBehavior & 2);
  $('tb2').checked = !!(s.tripBehavior & 4);
  $('tb3').checked = !!(s.tripBehavior & 8);
  $('tb4').checked = !!(s.tripBehavior & 16);
  $('tb5').checked = !!(s.tripBehavior & 32);
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
  $('msg').textContent = 'Saved (' + r.status + ')';
  setTimeout(()=> $('msg').textContent = '', 3000);
};
$('cMockProfile').addEventListener('change', () => {
  fetch('/settings/api?mockProfile=' + $('cMockProfile').value, { method:'POST' });
});
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
body { font-family:system-ui,sans-serif; margin:0; background:#f4f6f8; color:#1c1e21; }
.container { max-width:820px; margin:0 auto; padding:12px; }
.header { background:linear-gradient(135deg,#1877f2,#0a5ac2); padding:16px; border-radius:12px; color:#fff; text-align:center; margin-bottom:12px; }
.card { background:#fff; border-radius:12px; padding:16px; margin-bottom:12px; box-shadow:0 1px 4px rgba(0,0,0,0.06); }
table { width:100%; border-collapse:collapse; font-size:12px; }
th,td { padding:6px 8px; border-bottom:1px solid #f1f5f9; text-align:center; }
th { background:#f8fafc; color:#64748b; text-transform:uppercase; font-size:10px; }
.load-more { width:100%; padding:12px; border:2px solid #e2e8f0; background:#fff; border-radius:10px; font-weight:700; cursor:pointer; }
nav a { font-size:13px; font-weight:700; color:#1877f2; text-decoration:none; margin-right:12px; }
.summary { font-size:12px; color:#475569; margin-bottom:8px; }
.bits { font-size:10px; font-family:monospace; }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>💧 motorESP Data</h1>
    <nav><a href="/">Control</a><a href="/dashboard">Dashboard</a><a href="/settings">Settings</a></nav>
  </div>
  <div class="card">
    <div class="summary" id="summary">Loading...</div>
    <table><thead><tr><th>Boot</th><th>t(s)</th><th>V</th><th>I(A)</th><th>Wh</th><th>PF</th><th>State</th></tr></thead>
    <tbody id="rows"></tbody></table>
    <div style="height:10px"></div>
    <button class="load-more" id="loadMore">LOAD MORE</button>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let sinceBoot = 0, sinceTime = 0;
const stateMap = {1:'RUNNING',2:'OC',4:'DRYRUN',8:'OVERVOLT',16:'UNDERVOLT',32:'AUTO',64:'PZEM',128:'STARTFAIL'};

function hexToBytes(h){ const a=[]; for(let i=0;i<h.length;i+=2) a.push(parseInt(h.substr(i,2),16)); return a; }

async function loadMore() {
  let url = '/data/api?count=100';
  if (sinceBoot || sinceTime) url += '&boot=' + sinceBoot + '&time=' + sinceTime;
  const d = await (await fetch(url)).json();
  if (sinceBoot === 0 && sinceTime === 0) $('rows').innerHTML = '';
  const rows = hexToBytes(d.logs);
  const tbody = $('rows');
  for (let i = 0; i + 10 < rows.length; i += 11) {
    const b = rows.slice(i, i + 11);
    const timeSec = b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24);
    const volt = 200 + b[4];
    const cur = ((b[5]|(b[6]<<8))/10).toFixed(1);
    const pf = b[8];
    const st = b[9];
    const boot = b[10];
    const names = Object.keys(stateMap).filter(k => st & k).map(k => stateMap[k]).join('|') || 'OFF';
    const tr = document.createElement('tr');
    tr.innerHTML = '<td>'+boot+'</td><td>'+timeSec+'</td><td>'+volt+'</td><td>'+cur+'</td><td>'+b[7]+'</td><td>'+(pf>=254?'-':pf)+'</td><td class="bits">'+names+'</td>';
    tbody.appendChild(tr);
    sinceBoot = boot; sinceTime = timeSec;
  }
  $('summary').textContent = 'Total logs: ' + d.totalLogs + ' · shown to boot ' + sinceBoot + ' t=' + sinceTime;
}
$('loadMore').onclick = loadMore;
loadMore();
</script>
</body>
</html>
)rawliteral";

#endif