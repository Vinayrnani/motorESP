#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

const char _TAB_BAR[] PROGMEM = R"rawliteral(
<nav class="tab-bar">
  <a href="/" TAB_CTRL><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
  <a href="/dashboard" TAB_DASH><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
  <a href="/settings" TAB_SET><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
  <a href="/data" TAB_DATA><svg viewBox="0 0 24 24"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14,2 14,8 20,8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>Data</a>
</nav>)rawliteral";

// ============================================
// CONTROL PAGE (/)
// ============================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>motorESP</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Ctext y='.9em' font-size='90'%3E💧%3C/text%3E%3C/svg%3E">
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f1f5f9;color:#0f172a;min-height:100dvh;overflow-x:hidden}
.app{max-width:480px;margin:0 auto;padding:16px 16px 180px}

.hdr{display:flex;align-items:center;justify-content:space-between;padding:6px 0;margin-bottom:8px}
.hdr-left{display:flex;align-items:center;gap:10px}
.hdr-logo{width:40px;height:40px;background:linear-gradient(135deg,#3b82f6,#06b6d4);border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:20px;box-shadow:0 2px 8px rgba(59,130,246,.3)}
.hdr h1{font-size:17px;font-weight:700;color:#0f172a;letter-spacing:-.3px}
.hdr-sub{font-size:11px;color:#64748b;margin-top:1px}

.test-badge{display:none;align-items:center;gap:6px;background:#fef3c7;color:#92400e;padding:8px 14px;border-radius:12px;font-size:12px;font-weight:600;margin-bottom:12px;border:1px solid #fde68a}
.test-badge.on{display:flex}

.hero{background:#fff;border:1px solid #e2e8f0;border-radius:20px;padding:28px 20px;margin-bottom:16px;text-align:center;position:relative;overflow:hidden;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.status-ring{width:100px;height:100px;border-radius:50%;margin:0 auto 16px;display:flex;align-items:center;justify-content:center;position:relative}
.status-ring::before{content:'';position:absolute;inset:0;border-radius:50%;border:3px solid transparent;transition:border-color .3s}
.ring-ok .status-ring::before{border-color:#22c55e;box-shadow:0 0 20px rgba(34,197,94,.15)}
.ring-stop .status-ring::before{border-color:#cbd5e1}
.ring-warn .status-ring::before{border-color:#f59e0b;box-shadow:0 0 20px rgba(245,158,11,.15)}
.ring-alarm .status-ring::before{border-color:#ef4444;box-shadow:0 0 20px rgba(239,68,68,.15);animation:pulse-ring 1.5s ease-in-out infinite}
@keyframes pulse-ring{0%,100%{box-shadow:0 0 20px rgba(239,68,68,.15)}50%{box-shadow:0 0 30px rgba(239,68,68,.3)}}
.status-icon{font-size:40px;line-height:1}
.hero-status{font-size:22px;font-weight:800;letter-spacing:-.5px;margin-bottom:4px}
.hero-status.ok{color:#16a34a}.hero-status.stop{color:#94a3b8}.hero-status.warn{color:#d97706}.hero-status.alarm{color:#dc2626}
.hero-desc{font-size:13px;color:#64748b;line-height:1.5;max-width:280px;margin:0 auto}
.voltage-tag{display:inline-flex;align-items:center;gap:4px;padding:4px 12px;border-radius:20px;font-size:11px;font-weight:700;margin-top:12px;letter-spacing:.3px}
.vt-ok{background:#dcfce7;color:#166534}.vt-warn{background:#fef3c7;color:#92400e}.vt-crit{background:#fee2e2;color:#991b1b}.vt-off{background:#f1f5f9;color:#64748b}

.readings{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:16px}
.rd{background:#fff;border:1px solid #e2e8f0;border-radius:14px;padding:14px 8px;text-align:center;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.rd-label{font-size:10px;color:#64748b;font-weight:600;text-transform:uppercase;letter-spacing:.8px;margin-bottom:4px}
.rd-val{font-size:22px;font-weight:800;color:#0f172a;font-variant-numeric:tabular-nums}

.reason-card{display:none;background:#fff;border:1px solid #e2e8f0;border-radius:14px;padding:14px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.reason-card.on{display:block}
.reason-banner{border-radius:10px;padding:10px 14px;font-size:12px;font-weight:600;margin-bottom:10px;line-height:1.5}
.rb-warn{background:#fef3c7;color:#92400e;border:1px solid #fde68a}
.rb-danger{background:#fee2e2;color:#991b1b;border:1px solid #fecaca}
.rb-info{background:#dbeafe;color:#1e40af;border:1px solid #bfdbfe}
.rr{display:flex;justify-content:space-between;padding:6px 0;font-size:12px;border-bottom:1px solid #f1f5f9}
.rr:last-child{border-bottom:none}
.rr span{color:#64748b}.rr b{color:#0f172a;font-variant-numeric:tabular-nums}

.sticky-ctrl{position:fixed;bottom:52px;left:0;right:0;z-index:9;padding:10px 16px 10px;background:#fff;border-top:1px solid #e2e8f0;box-shadow:0 -2px 10px rgba(0,0,0,.04);pointer-events:none}
.sticky-ctrl>*{pointer-events:auto}
.sticky-inner{max-width:480px;margin:0 auto}
.mode-bar{display:flex;background:#fff;border:1px solid #e2e8f0;border-radius:12px;padding:3px;gap:3px;box-shadow:0 1px 3px rgba(0,0,0,.06);margin-bottom:6px}
.mode-btn{flex:1;padding:7px 0;border:none;border-radius:10px;background:transparent;color:#94a3b8;font-size:11px;font-weight:700;cursor:pointer;transition:all .2s;letter-spacing:.2px}
.mode-btn.active{background:#3b82f6;color:#fff;box-shadow:0 2px 8px rgba(59,130,246,.25)}
.mode-btn:active{transform:scale(.97)}

.actions{display:flex;flex-direction:column;gap:6px;margin-bottom:4px}
.act{padding:13px;border:none;border-radius:12px;font-size:14px;font-weight:700;cursor:pointer;transition:all .15s;letter-spacing:.3px;display:flex;align-items:center;justify-content:center;gap:6px}
.act:active{transform:scale(.97)}
.act:disabled{opacity:.35;cursor:not-allowed;transform:none;filter:grayscale(.4)}
.act-sub{font-size:10px;font-weight:600;opacity:.75;letter-spacing:.2px}
.act:not(:disabled) .act-sub{display:none}
.act-row{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.go{background:linear-gradient(135deg,#22c55e,#16a34a);color:#fff;box-shadow:0 4px 14px rgba(34,197,94,.3);font-size:15px;padding:14px}
.stop-btn{background:linear-gradient(135deg,#ef4444,#dc2626);color:#fff;box-shadow:0 4px 14px rgba(239,68,68,.25);font-size:15px;padding:14px}
.reset-btn{background:#e2e8f0;color:#64748b;font-size:12px;padding:9px;border-radius:10px;font-weight:600}
.reset-btn:not(:disabled){background:linear-gradient(135deg,#6366f1,#4f46e5);color:#fff;box-shadow:0 2px 8px rgba(99,102,241,.2)}

.floating-msg{position:fixed;top:12px;left:50%;transform:translateX(-50%);z-index:100;background:#1e293b;color:#fff;padding:8px 16px;border-radius:10px;font-size:12px;font-weight:700;box-shadow:0 4px 14px rgba(0,0,0,.15);max-width:90vw;text-align:center;display:none;pointer-events:none}
.floating-msg.good{background:#16a34a}.floating-msg.bad{background:#dc2626}.floating-msg.info{background:#3b82f6}

.tab-bar{position:fixed;bottom:0;left:0;right:0;background:#fff;display:flex;border-top:1px solid #e2e8f0;z-index:10;box-shadow:0 -2px 10px rgba(0,0,0,.04);padding-bottom:env(safe-area-inset-bottom,0)}
.tab-bar a{flex:1;display:flex;flex-direction:column;align-items:center;padding:10px 0 8px;text-decoration:none;color:#94a3b8;font-size:10px;font-weight:600;gap:3px;position:relative;transition:color .15s}
.tab-bar a.active{color:#3b82f6}
.tab-bar a.active::before{content:'';position:absolute;top:0;left:25%;right:25%;height:2px;background:#3b82f6;border-radius:0 0 2px 2px}
.tab-bar a:active{opacity:.7}
.tab-bar svg{width:20px;height:20px;stroke-width:2;stroke:currentColor;fill:none;flex-shrink:0}

.offline-toast{position:fixed;top:12px;left:50%;transform:translateX(-50%);background:#b45309;color:#fff;padding:6px 14px;border-radius:10px;font-size:11px;font-weight:700;z-index:99;display:none;box-shadow:0 4px 12px rgba(0,0,0,.2)}
</style>
</head>
<body>
<div class="floating-msg" id="msg"></div>
<div class="app">
  <div class="hdr">
    <div class="hdr-left">
      <div class="hdr-logo">💧</div>
      <div><h1>Pump Control</h1><div class="hdr-sub" id="subLine">connecting…</div></div>
    </div>
  </div>

  <div class="test-badge" id="testBanner">🧪 TEST MODE — simulated readings</div>

  <div class="hero ring-stop" id="heroWrap">
    <div class="status-ring"><span class="status-icon" id="statusIcon">⏸</span></div>
    <div class="hero-status stop" id="statusBig">OFF</div>
    <div class="hero-desc" id="statusPlain">Waiting for status…</div>
    <div class="voltage-tag vt-off" id="voltPill">METER OFFLINE</div>
  </div>

  <div class="readings">
    <div class="rd"><div class="rd-label">Voltage</div><div class="rd-val"><span id="stVolt">--</span></div></div>
    <div class="rd"><div class="rd-label">Current</div><div class="rd-val"><span id="stCur">--</span></div></div>
    <div class="rd"><div class="rd-label">Power</div><div class="rd-val"><span id="stPow">--</span></div></div>
  </div>

  <div class="reason-card" id="reasonPanel">
    <div class="reason-banner rb-info" id="reasonBanner"></div>
    <div class="rr"><span>Auto restart</span><b id="detRetry">—</b></div>
    <div class="rr"><span>Retries used</span><b id="detRetriesUsed">—</b></div>
    <div class="rr"><span>Quick repeats</span><b id="detFast">—</b></div>
    <div class="rr"><span id="detBlockedLbl">Start blocked</span><b id="detBlocked">—</b></div>
  </div>
</div>

<div class="sticky-ctrl">
  <div class="sticky-inner">
    <div class="mode-bar">
      <button class="mode-btn" data-mode="0">OFF</button>
      <button class="mode-btn" data-mode="1">MANUAL</button>
      <button class="mode-btn" data-mode="2">AUTO RETRY</button>
    </div>
    <div class="actions">
      <div class="act-row">
        <button class="act go" id="btnStart">▶ START<span class="act-sub"></span></button>
        <button class="act stop-btn" id="btnStop">⏹ STOP<span class="act-sub"></span></button>
      </div>
      <button class="act reset-btn" id="btnReset">↺ RESET<span class="act-sub"></span></button>
    </div>
  </div>
</div>

<nav class="tab-bar">
  <a href="/" class="active"><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
  <a href="/dashboard"><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
  <a href="/settings"><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
  <a href="/data"><svg viewBox="0 0 24 24"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14,2 14,8 20,8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>Data</a>
</nav>

<div class="offline-toast" id="offlineToast">⚠ offline — retrying…</div>

<script>
const $=id=>document.getElementById(id);
const TRIP_PLAIN={2:"Overload — the pump drew too much current",4:"No Water — the pump ran dry",
  8:"Voltage too high",16:"Voltage too low",64:"Power sensor fault",128:"Pump did not start"};
let _offlineEl=null, _offlineT=null;
function setupOffline(){
  _offlineEl=$('offlineToast');
  const of=window.fetch;
  window.fetch=function(){
    const p=of.apply(this,arguments);
    p.then(()=>{if(_offlineEl)_offlineEl.style.display='none';if(_offlineT){clearTimeout(_offlineT);_offlineT=null}})
     .catch(()=>{if(_offlineEl){_offlineEl.style.display='block';if(_offlineT)clearTimeout(_offlineT);_offlineT=setTimeout(()=>{if(_offlineEl)_offlineEl.style.display='none'},8000)}});
    return p;
  };
}
setupOffline();

function setModeBtn(m){document.querySelectorAll('.mode-btn').forEach(b=>b.classList.toggle('active',parseInt(b.dataset.mode)===m))}
document.querySelectorAll('.mode-btn').forEach(b=>b.addEventListener('click',()=>{
  setModeBtn(parseInt(b.dataset.mode));
  fetch('/control?action=mode&mode='+b.dataset.mode).then(r=>r.text()).then(m=>feed(m));
}));

function feed(m,cls){const el=$('msg');el.textContent=m;el.className='floating-msg '+(cls||'info');el.style.display='block';clearTimeout(feed._t);feed._t=setTimeout(()=>{el.style.display='none'},6000)}
function doAct(a){return fetch('/control?action='+a).then(r=>r.text()).then(m=>{feed(m,m.indexOf('OK')>=0?'good':(m.indexOf('BLOCKED')>=0?'bad':'info'));window.scrollTo({top:0,behavior:'smooth'})}).catch(()=>feed('Request failed','bad'))}
$('btnStart').onclick=()=>doAct('start');
$('btnStop').onclick=()=>doAct('stop');
$('btnReset').onclick=()=>{if(confirm('Reset the safety stop and unlock the pump?'))doAct('reset')};

function fmtCountdown(sec){if(!sec||sec<=0)return'not scheduled';const m=Math.floor(sec/60),s=sec%60;return(m?m+'m ':'')+s+'s'}

async function refresh(){
  try{
    const s=await(await fetch('/status')).json();
    $('subLine').textContent='v'+s.version+(s.mock?' · TEST':' · '+(s.rssi>-80?'ok':'weak'));
    $('testBanner').classList.toggle('on',s.mock);
    if(s.pzemValid||s.mock){$('stVolt').textContent=s.voltage.toFixed(0)+' V';$('stCur').textContent=s.current.toFixed(1)+' A';$('stPow').textContent=s.power>=1000?(s.power/1000).toFixed(2)+' kW':s.power.toFixed(0)+' W'}
    else{$('stVolt').textContent='--';$('stCur').textContent='--';$('stPow').textContent='--'}

    const pill=$('voltPill');
    if(!s.pzemValid&&!s.mock){pill.className='voltage-tag vt-off';pill.textContent='METER OFFLINE'}
    else{pill.className='voltage-tag '+({NORMAL:'vt-ok',WARNING:'vt-warn',CRITICAL:'vt-crit'}[s.voltageStatus]||'vt-ok');
      pill.textContent={NORMAL:'VOLTAGE OK',WARNING:'VOLTAGE HIGH',CRITICAL:'VOLTAGE CRITICAL'}[s.voltageStatus]||s.voltageStatus}

    const big=$('statusBig'),plain=$('statusPlain'),wrap=$('heroWrap'),icon=$('statusIcon');
    if(s.pumpState==='RUNNING'){big.className='hero-status ok';big.textContent='RUNNING';wrap.className='hero ring-ok';icon.textContent='⚡';plain.textContent='Pump is running normally.'}
    else if(s.pumpState==='TRIPPED'){big.className='hero-status alarm';big.textContent='SAFETY STOP';wrap.className='hero ring-alarm';icon.textContent='⚠';
      const names=(s.tripNames||'').split('|').filter(Boolean);const lines=names.map(n=>TRIP_PLAIN[n]||n).filter(Boolean);
      plain.textContent=(s.permanentLockout?'PERMANENT LOCKOUT — fault repeated too many times.':'The pump was stopped for safety: '+(lines.join('; ')||'a fault')+'. Press RESET, then START.')}
    else{big.className='hero-status stop';big.textContent='STOPPED';wrap.className='hero ring-stop';icon.textContent='⏸';plain.textContent='Pump is idle. Press START when you need water.'}

    const rp=$('reasonPanel');
    if(s.trips||s.permanentLockout||s.autoRetryIn>0){rp.classList.add('on');
      const names=(s.tripNames||'NONE').split('|').filter(n=>n!=='NONE');
      $('reasonBanner').className='reason-banner '+(s.permanentLockout?'rb-danger':'rb-warn');
      $('reasonBanner').textContent=names.length?('Reason: '+names.map(n=>TRIP_PLAIN[n]||n).join('; ')+(s.permanentLockout?' — LOCKOUT':'')):(s.permanentLockout?'PERMANENT LOCKOUT — press RESET after fixing the fault.':'');
      $('detRetry').textContent=fmtCountdown(s.autoRetryIn);$('detRetriesUsed').textContent=s.retryCount+' of '+s.maxRetries;
      $('detFast').textContent=s.fastFaultCount+' of '+s.maxFastFaults;$('detBlockedLbl').textContent='Start blocked';
      $('detBlocked').textContent=s.startFailBlock>0?fmtCountdown(s.startFailBlock)+' (start-fail)':'not blocked'}
    else{rp.classList.remove('on')}

    setModeBtn(s.pumpMode);refreshButtons(s);
  }catch(e){$('statusPlain').textContent='Cannot reach the controller — retrying…';setBtn($('btnStart'),true,'NO CONNECTION','');setBtn($('btnStop'),true,'NO CONNECTION','')}
}

function setBtn(btn,disabled,label,cnt){btn.disabled=!!disabled;btn.firstChild.textContent=label;const c=btn.querySelector('.act-sub');c.textContent=cnt||''}
function fmtSec(sec){if(!sec||sec<=0)return'';if(sec>=60)return Math.floor(sec/60)+'m '+(sec%60)+'s';return sec+'s'}

function refreshButtons(s){
  const running=s.pumpState==='RUNNING',starting=s.pumpState==='STARTING',stopping=s.pumpState==='STOPPING',tripped=s.pumpState==='TRIPPED';
  let startMsg='',startCnt='';
  if(tripped||s.trips||s.permanentLockout){startMsg='START';startCnt='RESET NEEDED'}
  else if(starting){startMsg='STARTING'}else if(stopping){startMsg='WAIT'}
  else if(running){startMsg='RUNNING'}
  else if(s.minOffLeft>0){startMsg='BLOCKED';startCnt=fmtSec(s.minOffLeft)}
  else if(s.startFailBlock>0){startMsg='BLOCKED';startCnt=fmtSec(s.startFailBlock)}
  else if(s.voltageLockLeft>0){startMsg='BLOCKED';startCnt='bad voltage'}
  else if(s.voltageStatus==='CRITICAL'){startMsg='START';startCnt='bad voltage'}
  else if(!s.pzemValid&&!s.mock){startMsg='START';startCnt='NO METER'}
  setBtn($('btnStart'),startMsg!=='',startMsg||'START',startCnt);
  $('btnStart').title=(startCnt==='RESET NEEDED')?'Safety stop active — press RESET first':'';
  let stopMsg='',stopCnt='',stopDisabled=true;
  if(stopping){stopMsg='STOPPING'}else if(running){if(s.minRunLeft>0){stopMsg='BLOCKED';stopCnt=fmtSec(s.minRunLeft)}else{stopMsg='STOP';stopDisabled=false}}
  else if(starting){stopMsg='STOPPING'}else{stopMsg='STOP';stopCnt='pump off'}
  setBtn($('btnStop'),stopDisabled,stopMsg,stopCnt);
  setBtn($('btnReset'),!(tripped||s.trips||s.permanentLockout),'RESET','');
}
setInterval(refresh,3000);refresh();
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
<script src="/lib/chartjs/chart.umd.min.js"></script>
<script src="/lib/dexie/dexie.min.js"></script>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f1f5f9;color:#0f172a}
.container{max-width:980px;margin:0 auto;padding:16px;padding-bottom:70px}
.hdr{display:flex;align-items:center;justify-content:space-between;padding:14px 0;margin-bottom:12px}
.hdr-left{display:flex;align-items:center;gap:10px}
.hdr-logo{width:40px;height:40px;background:linear-gradient(135deg,#3b82f6,#06b6d4);border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:20px;box-shadow:0 2px 8px rgba(59,130,246,.3)}
.hdr h1{font-size:17px;font-weight:700;color:#0f172a}
.hdr-right{display:flex;align-items:center;gap:8px;font-size:12px;color:#64748b}
.hdr-right select{font-size:12px;padding:4px 8px;border:1px solid #e2e8f0;border-radius:8px;background:#fff}
.card{background:#fff;border:1px solid #e2e8f0;border-radius:14px;padding:16px;margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.card h6{margin:0 0 8px;font-weight:700;color:#334155;font-size:13px}
.banner-test{background:#fef3c7;border:1px solid #fde68a;color:#92400e;border-radius:10px;padding:8px 12px;font-weight:600;font-size:12px;margin-bottom:12px;text-align:center}
.numerics{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.numerics.stale .num-card{opacity:.45;transition:opacity .3s}
@media(min-width:640px){.numerics{grid-template-columns:repeat(6,1fr)}}
.num-card{background:#f8fafc;padding:12px 6px;border-radius:10px;text-align:center;border:1px solid #f1f5f9}
.num-label{font-size:10px;color:#64748b;font-weight:600;text-transform:uppercase;letter-spacing:.5px}
.num-value{font-size:22px;font-weight:800;color:#0f172a;font-variant-numeric:tabular-nums}
.badge{padding:5px 12px;border-radius:20px;font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.3px}
.badge-running{background:#dcfce7;color:#166534}
.badge-stopped{background:#f1f5f9;color:#475569}
.badge-tripped{background:#fee2e2;color:#991b1b}
.chart-box{height:220px;position:relative}
.voltage-status{padding:4px 10px;border-radius:12px;font-weight:700;font-size:11px}
.vs-normal{background:#dcfce7;color:#166534}
.vs-none{background:#f1f5f9;color:#64748b}
.vs-warning{background:#fef3c7;color:#92400e}
.vs-critical{background:#fee2e2;color:#991b1b}
.hist{font-size:12px;color:#64748b}
.hint{font-size:11px;color:#64748b;text-align:center;margin-top:8px}
.tab-bar{position:fixed;bottom:0;left:0;right:0;background:#fff;display:flex;border-top:1px solid #e2e8f0;z-index:10;box-shadow:0 -2px 10px rgba(0,0,0,.04);padding-bottom:env(safe-area-inset-bottom,0)}
.tab-bar a{flex:1;display:flex;flex-direction:column;align-items:center;padding:10px 0 8px;text-decoration:none;color:#94a3b8;font-size:10px;font-weight:600;gap:3px;position:relative;transition:color .15s}
.tab-bar a.active{color:#3b82f6}
.tab-bar a.active::before{content:'';position:absolute;top:0;left:25%;right:25%;height:2px;background:#3b82f6;border-radius:0 0 2px 2px}
.tab-bar a:active{opacity:.7}
.tab-bar svg{width:20px;height:20px;stroke-width:2;stroke:currentColor;fill:none;flex-shrink:0}
.offline-toast{position:fixed;top:12px;left:50%;transform:translateX(-50%);background:#b45309;color:#fff;padding:6px 14px;border-radius:10px;font-size:11px;font-weight:700;z-index:99;display:none;box-shadow:0 4px 12px rgba(0,0,0,.2)}
</style>
</head>
<body>
<div class="container">
  <div class="hdr">
    <div class="hdr-left">
      <div class="hdr-logo">💧</div>
      <h1>Dashboard</h1>
    </div>
    <div class="hdr-right">
      <span>Refresh</span>
      <select id="pollSel"><option value="5000">5s</option><option value="3000">3s</option><option value="2000">2s</option><option value="1000">1s</option></select>
      <span id="pollActive" style="font-size:11px;color:#94a3b8">(5s)</span>
    </div>
  </div>

  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE — numbers below are simulated.</div>

  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:6px">
      <div style="display:flex;gap:6px;align-items:center;flex-wrap:wrap">
        <span id="stateBadge" class="badge badge-stopped">OFF</span>
        <span id="tripBadge" class="badge badge-tripped" style="display:none">SAFETY STOP</span>
        <span id="voltageStatus" class="voltage-status vs-normal">VOLTAGE OK</span>
      </div>
      <span class="hist">Uptime <b id="uptime">--</b></span>
    </div>
    <div class="hist" id="lastUpd" style="text-align:center;font-size:11px;margin-top:6px">—</div>
    <div class="numerics">
      <div class="num-card"><div class="num-label">Voltage</div><div class="num-value"><span id="nVolt">--</span></div></div>
      <div class="num-card"><div class="num-label">Current</div><div class="num-value"><span id="nCur">--</span></div></div>
      <div class="num-card"><div class="num-label">Power</div><div class="num-value"><span id="nPow">--</span></div></div>
      <div class="num-card"><div class="num-label">Energy</div><div class="num-value"><span id="nEn">--</span></div></div>
      <div class="num-card"><div class="num-label">Freq</div><div class="num-value"><span id="nHz">--</span></div></div>
      <div class="num-card"><div class="num-label">PF</div><div class="num-value"><span id="nPF">--</span></div><div id="pfHint" style="display:none;font-size:10px;color:#94a3b8;margin-top:2px">load &gt; 0.5A</div></div>
    </div>
    <div class="hint" id="dashMeterHint" style="display:none">Power meter not detected — enable TEST MODE in Control → ⋯</div>
  </div>

  <div class="card"><h6>⚡ Power (W)</h6><div class="chart-box"><canvas id="chartPower"></canvas><div id="chFall1" style="display:none;color:#64748b;font-size:12px;padding:8px">Chart library failed to load.</div></div></div>
  <div class="card"><h6>⚡ Voltage (V)</h6><div class="chart-box"><canvas id="chartVoltage"></canvas><div id="chFall2" style="display:none;color:#64748b;font-size:12px;padding:8px">Chart library failed to load.</div></div></div>
  <div class="card"><h6>⚡ Current (A)</h6><div class="chart-box"><canvas id="chartCurrent"></canvas><div id="chFall3" style="display:none;color:#64748b;font-size:12px;padding:8px">Chart library failed to load.</div></div></div>

  <nav class="tab-bar">
    <a href="/"><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
    <a href="/dashboard" class="active"><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
    <a href="/settings"><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
    <a href="/data"><svg viewBox="0 0 24 24"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14,2 14,8 20,8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>Data</a>
  </nav>
</div>

<div class="offline-toast" id="offlineToast">⚠ offline — retrying…</div>

<script>
const $=id=>document.getElementById(id);
let _offlineEl=null,_offlineT=null;
(function(){_offlineEl=$('offlineToast');const of=window.fetch;window.fetch=function(){const p=of.apply(this,arguments);p.then(()=>{if(_offlineEl)_offlineEl.style.display='none';if(_offlineT){clearTimeout(_offlineT);_offlineT=null}}).catch(()=>{if(_offlineEl){_offlineEl.style.display='block';if(_offlineT)clearTimeout(_offlineT);_offlineT=setTimeout(()=>{if(_offlineEl)_offlineEl.style.display='none'},8000)}});return p}})();
const pollSel=$('pollSel');
let series={power:[],voltage:[],current:[]},lastGoodTs=null,chartPower,chartVoltage,chartCurrent;
if(typeof Chart==='undefined')for(const id of['chFall1','chFall2','chFall3']){const el=$(id);if(el)el.style.display='block'}
function makeChart(id,label,color){if(typeof Chart==='undefined')return null;return new Chart($(id),{type:'line',data:{labels:[],datasets:[{label,data:[],borderColor:color,backgroundColor:color+'22',fill:true,borderWidth:2,pointRadius:0,tension:.2}]},options:{animation:false,responsive:true,maintainAspectRatio:false,scales:{y:{beginAtZero:true}}}})}
chartPower=makeChart('chartPower','Power (W)','#3b82f6');chartVoltage=makeChart('chartVoltage','Voltage (V)','#f59e0b');chartCurrent=makeChart('chartCurrent','Current (A)','#22c55e');
function pushChart(c,a,v,n){if(!c)return;a.push(v);if(a.length>n)a.shift();c.data.labels=a.map((_,i)=>i);c.data.datasets[0].data=a;c.update()}
const TRIP_PLAIN={2:"Overload",4:"No Water",8:"High voltage",16:"Low voltage",64:"Sensor fault",128:"Failed start"};
async function refresh(){try{const s=await(await fetch('/status')).json();$('testBanner').style.display=s.mock?'block':'none';
  if(s.pzemValid||s.mock){$('nVolt').textContent=s.voltage.toFixed(0)+' V';$('nCur').textContent=s.current.toFixed(1)+' A';$('nPow').textContent=s.power>=1000?(s.power/1000).toFixed(2)+' kW':s.power.toFixed(0)+' W';$('nHz').textContent=s.frequency.toFixed(1)+' Hz';$('nPF').textContent=s.current>.5?s.pf.toFixed(2):'--';$('pfHint').style.display=s.current>.5?'none':'block';$('nEn').textContent=s.energyKwh.toFixed(2)+' kWh'}
  else{$('nVolt').textContent='--';$('nCur').textContent='--';$('nPow').textContent='--';$('nHz').textContent='--';$('nPF').textContent='--';$('nEn').textContent='--';$('pfHint').style.display='none'}
  $('uptime').textContent=s.uptime;const b=$('stateBadge');b.className='badge '+(s.pumpState==='RUNNING'?'badge-running':(s.pumpState==='TRIPPED'?'badge-tripped':'badge-stopped'));b.textContent=s.pumpState;$('tripBadge').style.display=s.trips?'inline-block':'none';
  const vs=$('voltageStatus');if(!s.pzemValid&&!s.mock){vs.className='voltage-status vs-none';vs.textContent='METER OFFLINE'}else{vs.className='voltage-status '+({NORMAL:'vs-normal',WARNING:'vs-warning',CRITICAL:'vs-critical'}[s.voltageStatus]||'vs-normal');vs.textContent={NORMAL:'VOLTAGE OK',WARNING:'VOLTAGE HIGH',CRITICAL:'VOLTAGE CRITICAL'}[s.voltageStatus]||s.voltageStatus}
  $('dashMeterHint').style.display=(!s.pzemValid&&!s.mock)?'block':'none';
  if(s.pumpState==='RUNNING'||s.pumpState==='TRIPPED'){pushChart(chartPower,series.power,s.power,100);pushChart(chartVoltage,series.voltage,s.voltage,100);pushChart(chartCurrent,series.current,s.current,100)}
  lastGoodTs=new Date();document.querySelector('.numerics').classList.remove('stale');$('lastUpd').textContent='last update '+lastGoodTs.toLocaleTimeString()
}catch(e){$('lastUpd').textContent=lastGoodTs?'⚠ STALE — last '+lastGoodTs.toLocaleTimeString()+' · unreachable':'unreachable — retrying…';const n=document.querySelector('.numerics');if(n)n.classList.add('stale')}}
function setPollActive(ms){const e=$('pollActive');if(e)e.textContent='('+ms/1000+'s)'}
pollSel.addEventListener('change',()=>{clearInterval(window._ti);setPollActive(parseInt(pollSel.value));window._ti=setInterval(refresh,parseInt(pollSel.value))});
setPollActive(parseInt(pollSel.value));window._ti=setInterval(refresh,parseInt(pollSel.value));refresh();
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
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f1f5f9;color:#0f172a}
.container{max-width:640px;margin:0 auto;padding:16px;padding-bottom:70px}
.hdr{display:flex;align-items:center;gap:10px;padding:14px 0;margin-bottom:12px}
.hdr-logo{width:40px;height:40px;background:linear-gradient(135deg,#3b82f6,#06b6d4);border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:20px;box-shadow:0 2px 8px rgba(59,130,246,.3)}
.hdr h1{font-size:17px;font-weight:700;color:#0f172a}

.sec{margin-bottom:16px}
.sec-hdr{display:flex;align-items:center;justify-content:space-between;padding:14px 16px;background:#fff;border:1px solid #e2e8f0;border-radius:14px;cursor:pointer;transition:all .15s;box-shadow:0 1px 3px rgba(0,0,0,.04);user-select:none;-webkit-user-select:none}
.sec-hdr:active{transform:scale(.98)}
.sec-hdr .sec-left{display:flex;align-items:center;gap:10px}
.sec-hdr .sec-icon{width:36px;height:36px;border-radius:10px;display:flex;align-items:center;justify-content:center;font-size:18px;flex-shrink:0}
.sec-hdr .sec-title{font-size:15px;font-weight:700;color:#0f172a}
.sec-hdr .sec-sub{font-size:11px;color:#64748b;margin-top:1px}
.sec-hdr .chevron{width:20px;height:20px;color:#94a3b8;transition:transform .2s;flex-shrink:0}
.sec.open .sec-hdr .chevron{transform:rotate(180deg)}
.sec-body{max-height:0;overflow:hidden;transition:max-height .3s ease}
.sec.open .sec-body{max-height:2000px}
.sec-inner{padding:4px 0 0}

.card{background:#fff;border:1px solid #e2e8f0;border-radius:14px;padding:16px;margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.card h3{margin:0 0 4px;font-size:14px;color:#334155;font-weight:700}
.card .cardhelp{font-size:12px;color:#64748b;margin:0 0 12px;line-height:1.5}
.field{padding:10px 0;border-bottom:1px solid #f1f5f9}
.field:last-child{border-bottom:none}
.field label{font-size:14px;font-weight:700;color:#1e293b;display:block;margin-bottom:2px}
.field .help{font-size:12px;color:#64748b;line-height:1.45;margin:0 0 6px}
.field input,.field select{width:100%;padding:10px;border:1px solid #e2e8f0;border-radius:10px;font-size:15px;background:#fff;transition:border-color .15s}
.field input:focus,.field select:focus{outline:none;border-color:#3b82f6;box-shadow:0 0 0 3px rgba(59,130,246,.1)}
.field .unit-hint{font-size:11px;color:#94a3b8;margin-top:2px}
.check-row{display:flex;align-items:flex-start;gap:10px;padding:10px 0;border-bottom:1px solid #f1f5f9}
.check-row input{width:20px;height:20px;margin-top:1px;flex-shrink:0;accent-color:#3b82f6}
.check-row .ch-label{font-size:14px;font-weight:700;color:#1e293b}
.check-row .ch-help{font-size:12px;color:#64748b;line-height:1.4}
button.save{width:100%;padding:15px;background:linear-gradient(135deg,#3b82f6,#2563eb);color:#fff;border:none;border-radius:12px;font-size:16px;font-weight:700;cursor:pointer;box-shadow:0 4px 14px rgba(59,130,246,.25);transition:all .15s}
button.save:active{transform:scale(.98)}
button.danger{background:linear-gradient(135deg,#ef4444,#dc2626);margin-top:20px;font-size:14px;padding:12px;box-shadow:0 4px 14px rgba(239,68,68,.25)}
button.danger-arm{box-shadow:0 0 0 3px #fca5a5;animation:pulse-red 1s infinite;font-weight:700}
@keyframes pulse-red{50%{opacity:.7}}
.msg{min-height:20px;font-size:13px;font-weight:700;text-align:center;margin-top:8px}
.msg.good{color:#16a34a}.msg.bad{color:#dc2626}
.toast{position:fixed;top:14px;left:50%;transform:translateX(-50%);z-index:100;background:#16a34a;color:#fff;padding:10px 18px;border-radius:10px;font-size:14px;font-weight:700;box-shadow:0 4px 14px rgba(0,0,0,.15);max-width:92vw;text-align:center}
.toast.bad{background:#dc2626}
.banner-test{background:#fef3c7;border:1px solid #fde68a;color:#92400e;border-radius:10px;padding:8px 12px;font-weight:600;font-size:12px;margin-bottom:12px;text-align:center}
.tab-bar{position:fixed;bottom:0;left:0;right:0;background:#fff;display:flex;border-top:1px solid #e2e8f0;z-index:10;box-shadow:0 -2px 10px rgba(0,0,0,.04);padding-bottom:env(safe-area-inset-bottom,0)}
.tab-bar a{flex:1;display:flex;flex-direction:column;align-items:center;padding:10px 0 8px;text-decoration:none;color:#94a3b8;font-size:10px;font-weight:600;gap:3px;position:relative;transition:color .15s}
.tab-bar a.active{color:#3b82f6}
.tab-bar a.active::before{content:'';position:absolute;top:0;left:25%;right:25%;height:2px;background:#3b82f6;border-radius:0 0 2px 2px}
.tab-bar a:active{opacity:.7}
.tab-bar svg{width:20px;height:20px;stroke-width:2;stroke:currentColor;fill:none;flex-shrink:0}
.range-hint{display:inline-block;margin-top:4px;font-size:11px;font-weight:600;color:#64748b;background:#f1f5f9;border-radius:6px;padding:1px 6px}
.offline-toast{position:fixed;top:12px;left:50%;transform:translateX(-50%);background:#b45309;color:#fff;padding:6px 14px;border-radius:10px;font-size:11px;font-weight:700;z-index:99;display:none;box-shadow:0 4px 12px rgba(0,0,0,.2)}
</style>
</head>
<body>
<div class="container">
  <div class="hdr"><div class="hdr-logo">⚙️</div><h1>Pump Settings</h1></div>
  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE currently ON — simulated readings used.</div>

  <div class="sec open" id="secBasic">
    <div class="sec-hdr" onclick="toggleSec('secBasic')">
      <div class="sec-left">
        <div class="sec-icon" style="background:#dcfce7;color:#166534">⚡</div>
        <div><div class="sec-title">Basic Settings</div><div class="sec-sub">Protections &amp; test mode</div></div>
      </div>
      <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6,9 12,15 18,9"/></svg>
    </div>
    <div class="sec-body"><div class="sec-inner">

      <div class="card" id="testCard">
        <h3>🧪 Test Mode</h3>
        <p class="cardhelp">Test protections without the real power meter.</p>
        <div class="field"><label><input type="checkbox" id="cMock" style="width:auto;height:auto;transform:scale(1.3);margin-right:8px;vertical-align:middle"> Use test readings</label></div>
        <div class="field"><label for="cMockProfile">Scenario</label><p class="help">Choose what fake reading the controller should receive.</p><div id="mockProfileHint" style="display:none;font-size:11px;color:#64748b">Turn on "Use test readings" above first.</div><select id="cMockProfile" disabled><option value="off">Pump off</option><option value="running">Running normally</option><option value="dryrun">Dry run</option><option value="oc">Overload</option></select></div>
      </div>

      <div class="card"><h3>🛡 Overload Protection</h3><p class="cardhelp">Stops the pump when the motor draws too much current.</p>
        <div class="field"><label for="ocRunning">Normal current limit (A)</label><p class="help">Highest normal current. If exceeded for the delay, pump stops.</p><input type="number" id="ocRunning" step="0.1" min="5" max="50"><div class="unit-hint">e.g. 9.6A pump → set 12A</div></div>
        <div class="field"><label for="ocStartInstant">Start-up current limit (A)</label><p class="help">Instant spike threshold during start-up.</p><input type="number" id="ocStartInstant" min="20" max="100"></div>
        <div class="field"><label for="ocDelay">Confirm delay (s)</label><p class="help">Overload must last this long before stopping.</p><input type="number" id="ocDelay" min="1" max="30"></div>
      </div>

      <div class="card"><h3>💧 No-Water Protection</h3><p class="cardhelp">Stops the pump when it runs dry.</p>
        <div class="field"><label for="dryRunCurrent">No-water current (A)</label><input type="number" id="dryRunCurrent" step="0.1" min="1" max="10"></div>
        <div class="field"><label for="dryRunPower">No-water power (W)</label><p class="help">Both current AND power must be low.</p><input type="number" id="dryRunPower" min="100" max="2000"></div>
        <div class="field"><label for="dryRunDelay">Confirm delay (s)</label><input type="number" id="dryRunDelay" min="1" max="300"></div>
        <div class="field"><label for="dryRunActivation">Wait after start (s)</label><p class="help">Protection activates after this delay.</p><input type="number" id="dryRunActivation" min="0" max="3600"></div>
      </div>

      <div class="card"><h3>⚡ Voltage Protection</h3><p class="cardhelp">Stops the pump on dangerous mains voltage.</p>
        <div class="field"><label for="voltOver">Over-voltage trip (V)</label><input type="number" id="voltOver" min="200" max="280"></div>
        <div class="field"><label for="voltUnder">Under-voltage stop (V)</label><input type="number" id="voltUnder" min="150" max="230"></div>
        <div class="field"><label for="voltWarn">Pre-start warning (V)</label><input type="number" id="voltWarn" min="240" max="280"></div>
        <div class="field"><label for="voltCritical">Start block (V)</label><input type="number" id="voltCritical" min="250" max="300"></div>
        <div class="field"><label for="voltageDelay">Confirm delay (s)</label><input type="number" id="voltageDelay" min="1" max="60"></div>
        <div class="field"><label for="voltageLockout">Lockout after trip (s)</label><input type="number" id="voltageLockout" min="0" max="3600"></div>
      </div>

    </div></div>
  </div>

  <div class="sec" id="secAdvanced">
    <div class="sec-hdr" onclick="toggleSec('secAdvanced')">
      <div class="sec-left">
        <div class="sec-icon" style="background:#ede9fe;color:#5b21b6">🔧</div>
        <div><div class="sec-title">Advanced Settings</div><div class="sec-sub">Start/stop, auto-restart, recording</div></div>
      </div>
      <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6,9 12,15 18,9"/></svg>
    </div>
    <div class="sec-body"><div class="sec-inner">

      <div class="card"><h3>🔌 Starting &amp; Stopping</h3><p class="cardhelp">Start confirmation and min run/off times.</p>
        <div class="field"><label for="startSuccessCurrent">Start success current (A)</label><input type="number" id="startSuccessCurrent" step="0.1" min="0.5" max="5"></div>
        <div class="field"><label for="startVerifyDelay">Verify delay (s)</label><input type="number" id="startVerifyDelay" min="1" max="10"></div>
        <div class="field"><label for="startFailBlock">Failure retry block (s)</label><input type="number" id="startFailBlock" min="1" max="600"></div>
        <div class="field"><label for="minRun">Minimum run (s)</label><input type="number" id="minRun" min="10" max="300"></div>
        <div class="field"><label for="minOff">Minimum off (s)</label><input type="number" id="minOff" min="10" max="600"></div>
      </div>

      <div class="card"><h3>🔁 Auto Restart</h3><p class="cardhelp">Auto-restart after a safety trip. 3 quick repeats = permanent lockout.</p>
        <div class="field"><label for="autoRetryDelay">Retry delay (s)</label><input type="number" id="autoRetryDelay" min="60" max="3600"></div>
        <div class="field"><label for="maxRetries">Max retries</label><input type="number" id="maxRetries" min="1" max="10"></div>
        <div class="check-row"><input type="checkbox" id="tb0"><div><div class="ch-label">Auto-retry after overload</div><div class="ch-help">Recommended for brief blockages.</div></div></div>
        <div class="check-row"><input type="checkbox" id="tb1"><div><div class="ch-label">Auto-retry after dry run</div><div class="ch-help">Not recommended — keeps running dry.</div></div></div>
        <div class="check-row"><input type="checkbox" id="tb2"><div><div class="ch-label">Auto-retry after high voltage</div><div class="ch-help">Usually a mains issue.</div></div></div>
        <div class="check-row"><input type="checkbox" id="tb3"><div><div class="ch-label">Auto-retry after low voltage</div><div class="ch-help">Usually a mains issue.</div></div></div>
        <div class="check-row"><input type="checkbox" id="tb4"><div><div class="ch-label">Auto-retry after sensor fault</div><div class="ch-help">Monitoring is blind — risky.</div></div></div>
        <div class="check-row"><input type="checkbox" id="tb5"><div><div class="ch-label">Auto-retry after failed start</div><div class="ch-help">Stresses the motor.</div></div></div>
      </div>

      <div class="card"><h3>📊 Recording</h3>
        <div class="field"><label for="logIntervalRunning">Log interval running (s)</label><input type="number" id="logIntervalRunning" min="5" max="60"></div>
        <div class="field"><label for="logIntervalOff">Log interval off (s)</label><input type="number" id="logIntervalOff" min="30" max="600"></div>
        <div class="field"><label for="pzemReadRunning">Meter read interval (s)</label><input type="number" id="pzemReadRunning" min="1" max="5"></div>
      </div>

      <div class="card" style="border-color:#fecaca"><h3 style="color:#991b1b">⚠ Danger Zone</h3>
        <button class="save" id="btnClearData" style="background:linear-gradient(135deg,#ef4444,#dc2626);margin-top:8px;font-size:14px;padding:12px;box-shadow:0 4px 14px rgba(239,68,68,.25)">🗑 Clear History (restarts)</button>
        <button class="save" id="btnDefaults" style="background:linear-gradient(135deg,#6366f1,#4f46e5);margin-top:8px;font-size:14px;padding:12px;box-shadow:0 4px 14px rgba(99,102,241,.25)">↺ Reset to Defaults</button>
      </div>

    </div></div>
  </div>

  <button class="save" id="btnSave" style="margin-top:4px">💾 SAVE ALL SETTINGS</button>
  <div class="msg" id="msg"></div>

  <nav class="tab-bar">
    <a href="/"><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
    <a href="/dashboard"><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
    <a href="/settings" class="active"><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
    <a href="/data"><svg viewBox="0 0 24 24"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14,2 14,8 20,8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>Data</a>
  </nav>
</div>

<div class="offline-toast" id="offlineToast">⚠ offline — retrying…</div>

<script>
const $=id=>document.getElementById(id);
let _offlineEl=null,_offlineT=null;
(function(){_offlineEl=$('offlineToast');const of=window.fetch;window.fetch=function(){const p=of.apply(this,arguments);p.then(()=>{if(_offlineEl)_offlineEl.style.display='none';if(_offlineT){clearTimeout(_offlineT);_offlineT=null}}).catch(()=>{if(_offlineEl){_offlineEl.style.display='block';if(_offlineT)clearTimeout(_offlineT);_offlineT=setTimeout(()=>{if(_offlineEl)_offlineEl.style.display='none'},8000)}});return p}})();
function toggleSec(id){$(id).classList.toggle('open')}
const fields=['ocRunning','ocStartInstant','ocDelay','dryRunCurrent','dryRunPower','dryRunDelay','dryRunActivation',
  'voltOver','voltUnder','voltWarn','voltCritical','voltageDelay','voltageLockout',
  'startSuccessCurrent','startVerifyDelay','startFailBlock','minRun','minOff',
  'autoRetryDelay','maxRetries','logIntervalRunning','logIntervalOff','pzemReadRunning'];
(function(){fields.forEach(f=>{const el=$(f);if(!el||!el.min||el.min===''&&el.max==='')return;const lab=el.closest('.field')&&el.closest('.field').querySelector('label');if(!lab||lab.querySelector('.range-hint'))return;const r=document.createElement('span');r.className='range-hint';r.textContent='range '+el.min+'–'+el.max;lab.appendChild(r)})})();
function syncMockUI(){$('cMockProfile').disabled=!$('cMock').checked;$('cMockProfile').closest('.field').style.opacity=$('cMock').checked?1:.55;$('mockProfileHint').style.display=$('cMock').checked?'none':'block'}
async function load(){const s=await(await fetch('/settings/api')).json();$('testBanner').style.display=s.mock?'block':'none';$('cMock').checked=s.mock;syncMockUI();fields.forEach(f=>{if($(f))$(f).value=s[f]});
  $('tb0').checked=!!(s.tripBehavior&1);$('tb1').checked=!!(s.tripBehavior&2);$('tb2').checked=!!(s.tripBehavior&4);$('tb3').checked=!!(s.tripBehavior&8);$('tb4').checked=!!(s.tripBehavior&16);$('tb5').checked=!!(s.tripBehavior&32)}
function feed(t,cls){const el=$('msg');el.textContent=t;el.className='msg '+(cls||'');const ts=$('toast');if(ts){ts.textContent=t;ts.className='toast '+(cls==='bad'?'bad':'');ts.style.display='block'}clearTimeout(feed._t);feed._t=setTimeout(()=>{if(el.textContent===t){el.textContent='';el.className='msg'}const t2=$('toast');if(t2)t2.style.display='none'},4000)}
$('btnSave').onclick=async()=>{const bad=fields.filter(f=>{const el=$(f);if(!el||!el.value)return false;const v=parseFloat(el.value),mn=el.min?parseFloat(el.min):-Infinity,mx=el.max?parseFloat(el.max):Infinity;return isNaN(v)||v<mn||v>mx});
  if(bad.length){feed('Out of range: '+bad.join(', '),'bad');return}const btn=$('btnSave');btn.disabled=true;btn.textContent='Saving…';const p=new URLSearchParams();
  fields.forEach(f=>{if($(f))p.append(f,$(f).value)});p.append('mock',$('cMock').checked?'1':'0');let tb=0;
  if($('tb0').checked)tb|=1;if($('tb1').checked)tb|=2;if($('tb2').checked)tb|=4;if($('tb3').checked)tb|=8;if($('tb4').checked)tb|=16;if($('tb5').checked)tb|=32;p.append('tripBehavior',tb);
  try{const r=await fetch('/settings/api?'+p.toString(),{method:'POST'});feed(r.status===200?'✓ Settings saved':'✗ Save failed ('+r.status+')',r.status===200?'good':'bad');$('testBanner').style.display=$('cMock').checked?'block':'none'}
  catch(e){feed('✗ Save failed — unreachable','bad')}btn.disabled=false;btn.textContent='💾 SAVE ALL SETTINGS'};
$('cMock').addEventListener('change',()=>{syncMockUI();feed($('cMock').checked?'Test readings ON on save':'Test readings OFF on save')});
$('cMockProfile').addEventListener('change',()=>{fetch('/settings/api?mockProfile='+$('cMockProfile').value,{method:'POST'}).then(()=>feed('✓ Profile applied','good'));$('cMock').checked=true;syncMockUI();$('testBanner').style.display='block'});
const bcd=$('btnClearData');bcd.onclick=()=>{if(bcd.dataset.armed==='1'){bcd.dataset.armed='0';bcd.textContent='🗑 Clear History (restarts)';fetch('/settings/clear').then(()=>feed('Erasing…'));return}bcd.dataset.armed='1';bcd.textContent='⚠ TAP AGAIN to confirm';bcd.classList.add('danger-arm');clearTimeout(bcd._t);bcd._t=setTimeout(()=>{bcd.dataset.armed='0';bcd.textContent='🗑 Clear History (restarts)';bcd.classList.remove('danger-arm')},5000)};
const bdf=$('btnDefaults');bdf.onclick=async()=>{if(bdf.dataset.armed!=='1'){bdf.dataset.armed='1';bdf.textContent='⚠ TAP AGAIN to reset';bdf.classList.add('danger-arm');clearTimeout(bdf._t);bdf._t=setTimeout(()=>{bdf.dataset.armed='0';bdf.textContent='↺ Reset to Defaults';bdf.classList.remove('danger-arm')},5000);return}
  bdf.dataset.armed='0';bdf.textContent='↺ Reset to Defaults';bdf.classList.remove('danger-arm');
  try{const r=await fetch('/settings/api?action=defaults',{method:'POST'});if(!r.ok)throw 0;feed('✓ Reset to defaults','good');load()}catch(e){feed('✗ Failed','bad')}};
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
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f1f5f9;color:#0f172a}
.container{max-width:820px;margin:0 auto;padding:16px;padding-bottom:70px}
.hdr{display:flex;align-items:center;gap:10px;padding:14px 0;margin-bottom:12px}
.hdr-logo{width:40px;height:40px;background:linear-gradient(135deg,#3b82f6,#06b6d4);border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:20px;box-shadow:0 2px 8px rgba(59,130,246,.3)}
.hdr h1{font-size:17px;font-weight:700;color:#0f172a}
.card{background:#fff;border:1px solid #e2e8f0;border-radius:14px;padding:16px;margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,.04)}
.card h3{margin:0 0 4px;font-size:14px;color:#334155;font-weight:700}
.card .cardhelp{font-size:12px;color:#64748b;margin:0 0 10px;line-height:1.5}
table{width:100%;border-collapse:collapse;font-size:12px}
th,td{padding:7px 6px;border-bottom:1px solid #f1f5f9;text-align:center}
th{background:#f8fafc;color:#64748b;text-transform:uppercase;font-size:10px;font-weight:600}
.load-more{width:100%;padding:13px;border:2px solid #e2e8f0;background:#fff;border-radius:12px;font-weight:700;cursor:pointer;font-size:14px;color:#334155;transition:all .15s}
.load-more:active{background:#f1f5f9;transform:scale(.98)}
.summary{font-size:12px;color:#475569;margin-bottom:8px}
.msg{font-size:13px;font-weight:700;text-align:center;border-radius:8px;padding:10px;margin-top:10px}
.msg.good{color:#166534;background:#f0fdf4;border:1px solid #86efac}
.msg.bad{color:#991b1b;background:#fef2f2;border:1px solid #fca5a5}
.bits{font-size:10px;font-weight:700}
.bits .chip{display:inline-block;padding:2px 6px;border-radius:8px;margin:1px;white-space:nowrap}
.chip-run{background:#dcfce7;color:#166534}
.chip-fault{background:#fee2e2;color:#991b1b}
.chip-auto{background:#dbeafe;color:#1e40af}
.chip-off{background:#f1f5f9;color:#475569}
.details{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px}
.details span{font-size:11px;background:#f8fafc;border:1px solid #e2e8f0;padding:4px 8px;border-radius:8px;color:#475569}
.tab-bar{position:fixed;bottom:0;left:0;right:0;background:#fff;display:flex;border-top:1px solid #e2e8f0;z-index:10;box-shadow:0 -2px 10px rgba(0,0,0,.04);padding-bottom:env(safe-area-inset-bottom,0)}
.tab-bar a{flex:1;display:flex;flex-direction:column;align-items:center;padding:10px 0 8px;text-decoration:none;color:#94a3b8;font-size:10px;font-weight:600;gap:3px;position:relative;transition:color .15s}
.tab-bar a.active{color:#3b82f6}
.tab-bar a.active::before{content:'';position:absolute;top:0;left:25%;right:25%;height:2px;background:#3b82f6;border-radius:0 0 2px 2px}
.tab-bar a:active{opacity:.7}
.tab-bar svg{width:20px;height:20px;stroke-width:2;stroke:currentColor;fill:none;flex-shrink:0}
.banner-test{background:#fef3c7;border:1px solid #fde68a;color:#92400e;border-radius:10px;padding:8px 12px;font-weight:600;font-size:12px;margin-bottom:12px;text-align:center}
.offline-toast{position:fixed;top:12px;left:50%;transform:translateX(-50%);background:#b45309;color:#fff;padding:6px 14px;border-radius:10px;font-size:11px;font-weight:700;z-index:99;display:none;box-shadow:0 4px 12px rgba(0,0,0,.2)}
</style>
</head>
<body>
<div class="container">
  <div class="hdr"><div class="hdr-logo">📋</div><h1>Pump History</h1></div>
  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE — log contains simulated readings.</div>

  <div class="card">
    <h3>Recorded readings</h3>
    <p class="cardhelp">One row per measurement. Newest first; LOAD MORE adds 100 older rows.</p>
    <div class="summary" id="summary">Loading…</div>
    <div style="overflow-x:auto">
    <table>
      <thead><tr><th>Session</th><th>Time (s)</th><th>V (V)</th><th>I (A)</th><th>Used (Wh)</th><th>PF</th><th>Status</th></tr></thead>
      <tbody id="rows"></tbody>
    </table>
    </div>
    <div style="height:10px"></div>
    <button class="load-more" id="loadMore">⬇ LOAD MORE</button>
    <div id="loadErr" class="msg bad" style="display:none"></div>
    <div id="loadOk" class="msg good" style="display:none"></div>
    <div class="details">
      <span><b>Session</b> — power-on count</span>
      <span><b>Time</b> — seconds since power-on</span>
      <span><b>Used</b> — energy since previous row</span>
      <span><b>PF</b> — 0.5–1.0 efficiency</span>
      <span><b>Status</b> — pump state</span>
    </div>
  </div>

  <nav class="tab-bar">
    <a href="/"><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
    <a href="/dashboard"><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
    <a href="/settings"><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="14"/><line x1="4" y1="10" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="12"/><line x1="12" y1="8" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="16"/><line x1="20" y1="12" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="9" y1="8" x2="15" y2="8"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
    <a href="/data" class="active"><svg viewBox="0 0 24 24"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14,2 14,8 20,8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>Data</a>
  </nav>
</div>

<div class="offline-toast" id="offlineToast">⚠ offline — retrying…</div>

<script>
const $=id=>document.getElementById(id);
let _offlineEl=null,_offlineT=null;
(function(){_offlineEl=$('offlineToast');const of=window.fetch;window.fetch=function(){const p=of.apply(this,arguments);p.then(()=>{if(_offlineEl)_offlineEl.style.display='none';if(_offlineT){clearTimeout(_offlineT);_offlineT=null}}).catch(()=>{if(_offlineEl){_offlineEl.style.display='block';if(_offlineT)clearTimeout(_offlineT);_offlineT=setTimeout(()=>{if(_offlineEl)_offlineEl.style.display='none'},8000)}});return p}})();
let sinceBoot=0,sinceTime=0;const RUN=1,OC=2,DRY=4,OV=8,UV=16,AUTO=32,PZEM=64,SFAIL=128;
function hexToBytes(h){const a=[];for(let i=0;i<h.length;i+=2)a.push(parseInt(h.substr(i,2),16));return a}
function chips(st){const c=[];if(st&RUN)c.push('<span class="chip chip-run">RUNNING</span>');if(st&OC)c.push('<span class="chip chip-fault">OVERLOAD</span>');if(st&DRY)c.push('<span class="chip chip-fault">NO WATER</span>');if(st&OV)c.push('<span class="chip chip-fault">HIGH V</span>');if(st&UV)c.push('<span class="chip chip-fault">LOW V</span>');if(st&PZEM)c.push('<span class="chip chip-fault">PZEM</span>');if(st&SFAIL)c.push('<span class="chip chip-fault">START FAIL</span>');if(st&AUTO)c.push('<span class="chip chip-auto">AUTO</span>');if(!c.length)c.push('<span class="chip chip-off">OFF</span>');return'<span class="bits">'+c.join('')+'</span>'}
async function loadMore(){const btn=$('loadMore'),errEl=$('loadErr');btn.disabled=true;btn.textContent='Loading…';if(errEl)errEl.style.display='none';let url='/data/api?count=100';if(sinceBoot||sinceTime)url+='&boot='+sinceBoot+'&time='+sinceTime;let d;try{d=await(await fetch(url)).json()}catch(e){btn.disabled=false;btn.textContent='⬇ LOAD MORE';if(errEl){errEl.style.display='block';errEl.textContent='Could not load — check connection.'}return}
  btn.disabled=false;btn.textContent='⬇ LOAD MORE';if(sinceBoot===0&&sinceTime===0)$('rows').innerHTML='';const rows=hexToBytes(d.logs),tbody=$('rows');let added=0;
  for(let i=0;i+11<=rows.length;i+=11){const b=rows.slice(i,i+11),timeSec=b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24),volt=200+b[4],cur=((b[5]|(b[6]<<8))/10).toFixed(1),pf=b[8]>=254?'-':(b[8]/100).toFixed(2),st=b[9],boot=b[10];
    const tr=document.createElement('tr');tr.innerHTML='<td>'+boot+'</td><td>'+timeSec+'</td><td>'+volt+'</td><td>'+cur+'</td><td>'+b[7]+'</td><td>'+pf+'</td><td>'+chips(st)+'</td>';tbody.appendChild(tr);sinceBoot=boot;sinceTime=timeSec;added++}
  const okEl=$('loadOk');if(okEl){okEl.style.display='block';okEl.textContent=added>0?'✓ Loaded '+added+' row'+(added>1?'s':''):'✓ Up to date';clearTimeout(loadMore._okT);loadMore._okT=setTimeout(()=>{okEl.style.display='none'},4000)}
  if(added===0&&d.totalLogs===0){$('summary').textContent='No entries yet — saves automatically while running.'}
  else{const shown=tbody.children.length,total=d.totalLogs;if(shown>=total){$('summary').textContent=total+' total · all '+shown+' shown';btn.disabled=true;btn.textContent='✓ All loaded'}
  else{const pct=total?Math.round(shown/total*100):0;btn.disabled=false;btn.textContent='⬇ LOAD MORE ('+shown+'/'+total+' · ~'+pct+'%)';$('summary').textContent=total+' total · '+shown+' shown (newest first)'}}
}
$('loadMore').onclick=loadMore;loadMore();
fetch('/status').then(r=>r.json()).then(s=>{$('testBanner').style.display=s.mock?'block':'none'});
</script>
</body>
</html>
)rawliteral";

#endif
