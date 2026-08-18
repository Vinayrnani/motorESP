#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>

const char _TAB_BAR[] PROGMEM = R"rawliteral(
<nav class="tab-bar">
  <a href="/" TAB_CTRL><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
  <a href="/dashboard" TAB_DASH><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
  <a href="/settings" TAB_SET><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
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
.hdr-right{display:flex;align-items:center;gap:10px}
.power-toggle{position:relative;display:inline-block;width:50px;height:28px;cursor:pointer}
.power-toggle input{opacity:0;width:0;height:0;position:absolute}
.power-track{position:absolute;inset:0;background:#cbd5e1;border-radius:14px;transition:background .25s}
.power-toggle input:checked+.power-track{background:#22c55e}
.power-knob{position:absolute;top:3px;left:3px;width:22px;height:22px;background:#fff;border-radius:50%;box-shadow:0 1px 4px rgba(0,0,0,.15);transition:transform .25s;pointer-events:none}
.power-toggle input:checked~.power-knob{transform:translateX(22px)}
.power-label{font-size:11px;font-weight:700;letter-spacing:.3px;min-width:28px;text-align:right}
.power-label.on{color:#22c55e}.power-label.off{color:#94a3b8}

body.off-mode .hero,
body.off-mode .readings,
body.off-mode .reason-card,
body.off-mode .test-badge,
body.off-mode .sticky-ctrl{opacity:.3;pointer-events:none;filter:grayscale(.5)}
body.off-mode .sticky-ctrl{transform:none}

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
.hero-tags{display:flex;flex-wrap:wrap;gap:6px;justify-content:center;margin-top:10px}
.hero-tag{display:inline-flex;align-items:center;gap:4px;padding:3px 10px;border-radius:20px;font-size:10px;font-weight:700;letter-spacing:.3px}
.ht-sch{background:#dbeafe;color:#1e40af}.ht-retry{background:#ede9fe;color:#5b21b6}.ht-maxrun{background:#fef3c7;color:#92400e}

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
    <div class="hdr-right">
      <label class="power-toggle" id="powerToggle"><input type="checkbox" id="powerChk" checked><span class="power-track"></span><span class="power-knob"></span></label>
      <span class="power-label on" id="powerLbl">ON</span>
    </div>
  </div>

  <div class="test-badge" id="testBanner">🧪 TEST MODE</div>

  <div class="hero ring-stop" id="heroWrap">
    <div class="status-ring"><span class="status-icon" id="statusIcon">⏸</span></div>
    <div class="hero-status stop" id="statusBig">OFF</div>
    <div class="hero-desc" id="statusPlain">Waiting for status…</div>
    <div class="voltage-tag vt-off" id="voltPill">METER OFFLINE</div>
    <div class="hero-tags" id="heroTags"></div>
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
  <a href="/settings"><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
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

$('powerChk').addEventListener('change',()=>{
  const on=$('powerChk').checked;
  $('powerLbl').textContent=on?'ON':'OFF';
  $('powerLbl').className='power-label '+(on?'on':'off');
  document.body.classList.toggle('off-mode',!on);
  fetch('/control?action=power&state='+(on?1:0)).then(r=>r.text()).then(m=>{feed(m,m.indexOf('BLOCKED')>=0?'bad':'info');setTimeout(refresh,300)}).catch(()=>feed('Request failed','bad'));
});

function feed(m,cls){const el=$('msg');el.textContent=m;el.className='floating-msg '+(cls||'info');el.style.display='block';clearTimeout(feed._t);feed._t=setTimeout(()=>{el.style.display='none'},6000)}
function doAct(a){return fetch('/control?action='+a).then(r=>r.text()).then(m=>{feed(m,m.indexOf('OK')>=0?'good':(m.indexOf('BLOCKED')>=0?'bad':'info'));window.scrollTo({top:0,behavior:'smooth'});setTimeout(refresh,300)}).catch(()=>feed('Request failed','bad'))}
$('btnStart').onclick=()=>doAct('start');
$('btnStop').onclick=()=>doAct('stop');
$('btnReset').onclick=()=>{if(confirm('Reset the safety stop and unlock the pump?'))doAct('reset')};

function fmtCountdown(sec){if(!sec||sec<=0)return'not scheduled';const m=Math.floor(sec/60),s=sec%60;return(m?m+'m ':'')+s+'s'}
let S={},pollTime=0;
function calcTimers(o){
  const now=o.sMs||0;
  o.minRunLeft=o.minRunEnd>0?Math.max(0,Math.ceil((o.minRunEnd-now)/1000)):0;
  o.minOffLeft=o.minOffEnd>0?Math.max(0,Math.ceil((o.minOffEnd-now)/1000)):0;
  o.startFailBlock=o.sfBlockEnd>0?Math.max(0,Math.ceil((o.sfBlockEnd-now)/1000)):0;
  o.voltageLockLeft=o.vLockEnd>0?Math.max(0,Math.ceil((o.vLockEnd-now)/1000)):0;
  o.autoRetryIn=o.arAt>0?Math.max(0,Math.ceil((o.arAt-now)/1000)):0;
  o.maxRunTimeLeft=o.maxRunTimeEnd>0?Math.max(0,Math.ceil((o.maxRunTimeEnd-now)/1000)):0;
  return o;
}

async function refresh(){
  try{
    const s=await(await fetch('/status')).json();
    S=s;pollTime=Date.now();calcTimers(s);
    $('subLine').textContent='v'+s.version+(s.mock?' · TEST':' · '+(s.rssi>-80?'ok':'weak'));
    $('testBanner').classList.toggle('on',s.mock);
    if(s.pzemValid||s.mock){$('stVolt').textContent=s.voltage.toFixed(0)+' V';$('stCur').textContent=s.current.toFixed(1)+' A';$('stPow').textContent=s.power>=1000?(s.power/1000).toFixed(2)+' kW':s.power.toFixed(0)+' W'}
    else{$('stVolt').textContent='--';$('stCur').textContent='--';$('stPow').textContent='--'}

    const pill=$('voltPill');
    if(!s.pzemValid&&!s.mock){pill.className='voltage-tag vt-off';pill.textContent='METER OFFLINE'}
    else{pill.className='voltage-tag '+({NORMAL:'vt-ok',WARNING:'vt-warn',CRITICAL:'vt-crit'}[s.voltageStatus]||'vt-ok');
      pill.textContent={NORMAL:'VOLTAGE OK',WARNING:'VOLTAGE HIGH',CRITICAL:'VOLTAGE CRITICAL'}[s.voltageStatus]||s.voltageStatus}

    const big=$('statusBig'),plain=$('statusPlain'),wrap=$('heroWrap'),icon=$('statusIcon');
    if(s.pumpState==='RUNNING'){big.className='hero-status ok';big.textContent='RUNNING';wrap.className='hero ring-ok';icon.textContent='⚡';plain.textContent='Running normally.'}
    else if(s.pumpState==='STARTING'){big.className='hero-status ok';big.textContent='STARTING…';wrap.className='hero ring-ok';icon.textContent='🔄';plain.textContent='Starting…'}
    else if(s.pumpState==='TRIPPED'){big.className='hero-status alarm';big.textContent='SAFETY STOP';wrap.className='hero ring-alarm';icon.textContent='⚠';
      const names=(s.tripNames||'').split('|').filter(Boolean);const lines=names.map(n=>TRIP_PLAIN[n]||n).filter(Boolean);
      plain.textContent=(s.permanentLockout?'PERMANENT LOCKOUT — too many faults.':'Stopped for safety: '+(lines.join('; ')||'a fault')+'. Press RESET, then START.')}
    else if(s.pumpMode===0){big.className='hero-status stop';big.textContent='OFF';wrap.className='hero ring-stop';icon.textContent='⏸';plain.textContent='Pump is off. Turn power ON, then press START.'}
    else{big.className='hero-status stop';big.textContent='STOPPED';wrap.className='hero ring-stop';icon.textContent='⏸';
      plain.textContent=s.maxRunTimeStop?'Stopped: max run time reached.':(!s.pzemValid&&!s.mock)?'Pump is idle. Enable test mode in Settings.':'Pump is idle.'}

    updateHeroTags(s);
    updateReasonPanel(s);

    setPowerUI(s);refreshButtons(s);
  }catch(e){$('statusPlain').textContent='Cannot reach the controller — retrying…';setBtn($('btnStart'),true,'NO CONNECTION','');setBtn($('btnStop'),true,'NO CONNECTION','')}
}

function setBtn(btn,disabled,label,cnt){btn.disabled=!!disabled;btn.firstChild.textContent=label;const c=btn.querySelector('.act-sub');c.textContent=cnt||''}
function setPowerUI(s){const on=s.pumpEnabled!==0;$('powerChk').checked=on;$('powerLbl').textContent=on?'ON':'OFF';$('powerLbl').className='power-label '+(on?'on':'off');document.body.classList.toggle('off-mode',!on)}
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
  else if(starting){stopMsg='STOPPING'}else if(tripped){stopMsg='STOP';stopCnt='safety stop'}else{stopMsg='STOP';stopCnt='pump off'}
  setBtn($('btnStop'),stopDisabled,stopMsg,stopCnt);
  setBtn($('btnReset'),!(tripped||s.trips||s.permanentLockout),'RESET','');
}
function updateHeroTags(s){
  const tags=[];
  if(s.scheduleActive)tags.push('<span class="hero-tag ht-sch">⏰ Schedule</span>');
  if(s.tripBehavior)tags.push('<span class="hero-tag ht-retry">🔁 Auto Retry</span>');
  if(s.maxRunTimeStop)tags.push('<span class="hero-tag ht-maxrun">⏱ Max Run Time</span>');
  else if(s.maxRunTime>0&&s.maxRunTimeLeft>0&&(s.pumpState==='RUNNING'||s.pumpState==='STARTING'))tags.push('<span class="hero-tag ht-maxrun">⏱ '+fmtCountdown(s.maxRunTimeLeft)+'</span>');
  $('heroTags').innerHTML=tags.join('');
}
function updateReasonPanel(s){
  const rp=$('reasonPanel');
  if(s.trips||s.permanentLockout||s.autoRetryIn>0){rp.classList.add('on');
    const names=(s.tripNames||'NONE').split('|').filter(n=>n!=='NONE');
    $('reasonBanner').className='reason-banner '+(s.permanentLockout?'rb-danger':'rb-warn');
    $('reasonBanner').textContent=names.length?('Reason: '+names.map(n=>TRIP_PLAIN[n]||n).join('; ')+(s.permanentLockout?' — LOCKOUT':'')):(s.permanentLockout?'PERMANENT LOCKOUT — press RESET after fixing the fault.':'');
    $('detRetry').textContent=fmtCountdown(s.autoRetryIn);$('detRetriesUsed').textContent=s.retryCount+' of '+s.maxRetries;
    $('detFast').textContent=s.fastFaultCount+' of '+s.maxFastFaults;$('detBlockedLbl').textContent='Start blocked';
    $('detBlocked').textContent=s.startFailBlock>0?fmtCountdown(s.startFailBlock)+' (start-fail)':'not blocked'}
  else{rp.classList.remove('on')}
}
function tickLocal(){
  if(!S.sMs)return;
  const L=Object.assign({},S);
  L.sMs=S.sMs+(Date.now()-pollTime);
  calcTimers(L);
  refreshButtons(L);updateHeroTags(L);updateReasonPanel(L);
}
setInterval(refresh,3000);refresh();setInterval(tickLocal,1000);
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
<script src="/lib/hammerjs/hammer.min.js"></script>
<script src="/lib/chartjs-plugin-zoom/chartjs-plugin-zoom.min.js"></script>
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
.chart-box{height:350px;position:relative}
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
.boot-row{display:flex;gap:8px;align-items:center;margin-bottom:10px}
.boot-row select{flex:1;padding:8px 10px;border:1px solid #e2e8f0;border-radius:10px;font-size:13px;background:#fff}
.boot-row button{padding:8px 12px;border:1px solid #e2e8f0;border-radius:10px;background:#fff;font-size:13px;cursor:pointer;white-space:nowrap;color:#475569}
.boot-row button:active{background:#f1f5f9}
.chart-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.chart-hdr h6{margin:0}
.chart-hdr button{padding:4px 10px;border:1px solid #e2e8f0;border-radius:8px;background:#fff;font-size:11px;cursor:pointer;color:#475569}
.chart-label{font-size:10px;color:#64748b;text-align:center;margin-top:4px}
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
    </div>
  </div>

  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE</div>

  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;flex-wrap:wrap;gap:6px">
      <div style="display:flex;gap:6px;align-items:center;flex-wrap:wrap">
        <span id="stateBadge" class="badge badge-stopped">OFF</span>
        <span id="tripBadge" class="badge badge-tripped" style="display:none">SAFETY STOP</span>
        <span id="voltageStatus" class="voltage-status vs-normal">VOLTAGE OK</span>
      </div>
      <span class="hist">Uptime <b id="uptime">--</b> · <span id="lastUpd">—</span></span>
    </div>
    <div class="numerics">
      <div class="num-card"><div class="num-label">Voltage</div><div class="num-value"><span id="nVolt">--</span></div></div>
      <div class="num-card"><div class="num-label">Current</div><div class="num-value"><span id="nCur">--</span></div></div>
      <div class="num-card"><div class="num-label">Power</div><div class="num-value"><span id="nPow">--</span></div></div>
      <div class="num-card"><div class="num-label">Energy</div><div class="num-value"><span id="nEn">--</span></div></div>
      <div class="num-card"><div class="num-label">Freq</div><div class="num-value"><span id="nHz">--</span></div></div>
      <div class="num-card"><div class="num-label">PF</div><div class="num-value"><span id="nPF">--</span></div><div id="pfHint" style="display:none;font-size:10px;color:#94a3b8;margin-top:2px">load &gt; 0.5A</div></div>
    </div>
    <div class="hint" id="dashMeterHint" style="display:none">No power meter. Enable test mode in Settings.</div>
  </div>

  <div class="card">
    <div class="boot-row">
      <select id="bootSel"><option value="0">Current boot</option></select>
      <button onclick="loadBootData()">Load</button>
    </div>
    <div class="chart-hdr"><h6>Power &amp; Current</h6><button onclick="chartMain&&chartMain.resetZoom()">Reset zoom</button></div>
    <div class="chart-box"><canvas id="chartMain"></canvas></div>
    <div class="chart-label" id="chartLabel">Loading…</div>
  </div>

  <nav class="tab-bar">
    <a href="/"><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
    <a href="/dashboard" class="active"><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
    <a href="/settings"><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
    <a href="/data"><svg viewBox="0 0 24 24"><path d="M14 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V8z"/><polyline points="14,2 14,8 20,8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>Data</a>
  </nav>
</div>

<div class="offline-toast" id="offlineToast">⚠ offline — retrying…</div>

<script>
const $=id=>document.getElementById(id);
let _offlineEl=null,_offlineT=null;
(function(){_offlineEl=$('offlineToast');const of=window.fetch;window.fetch=function(){const p=of.apply(this,arguments);p.then(()=>{if(_offlineEl)_offlineEl.style.display='none';if(_offlineT){clearTimeout(_offlineT);_offlineT=null}}).catch(()=>{if(_offlineEl){_offlineEl.style.display='block';if(_offlineT)clearTimeout(_offlineT);_offlineT=setTimeout(()=>{if(_offlineEl)_offlineEl.style.display='none'},8000)}});return p}})();

function hexToBytes(h){const b=[];for(let i=0;i<h.length;i+=2)b.push(parseInt(h.substr(i,2),16));return b}

const pollSel=$('pollSel');
let chartMain,selectedBoot=0,livePower=[],liveCurrent=[],liveVoltage=[],liveTimestamps=[];

function createChart(){
  if(typeof Chart==='undefined')return null;
  return new Chart($('chartMain'),{type:'line',data:{labels:[],datasets:[
    {label:'Voltage (V)',data:[],borderColor:'#f59e0b',backgroundColor:'transparent',fill:false,borderWidth:2,pointRadius:0,tension:.3,yAxisID:'yVolt',order:3},
    {label:'Power (W)',data:[],borderColor:'#3b82f6',backgroundColor:'transparent',fill:false,borderWidth:2,pointRadius:0,tension:.3,yAxisID:'yPower',order:2},
    {label:'Current (A)',data:[],borderColor:'#22c55e',backgroundColor:'transparent',fill:false,borderWidth:2,pointRadius:0,tension:.3,yAxisID:'yCurrent',order:1}
  ]},options:{animation:false,responsive:true,maintainAspectRatio:false,interaction:{intersect:false,mode:'index'},
    layout:{padding:{left:0,right:0}},
    plugins:{legend:{position:'top',labels:{boxWidth:12,padding:8,font:{size:10,weight:'600'}}},
      tooltip:{callbacks:{label:function(c){return c.dataset.label+': '+c.parsed.y.toFixed(c.datasetIndex===0?0:c.datasetIndex===1?0:2)}}},
      zoom:{pan:{enabled:true,mode:'x'},zoom:{wheel:{enabled:true},pinch:{enabled:true},drag:{enabled:false},mode:'x'}}},
    scales:{x:{display:true, ticks:{maxRotation:45,font:{size:9},maxTicksLimit:10,autoSkip:true}},
      yVolt:{type:'linear',position:'left',beginAtZero:false,grid:{color:'rgba(0,0,0,.05)'},ticks:{font:{size:10},callback:function(v){return v.toFixed(0)+'V'}}},
      yPower:{type:'linear',position:'right',beginAtZero:true,grid:{drawOnChartArea:false},ticks:{font:{size:10}}},
      yCurrent:{type:'linear',position:'right',offset:true,beginAtZero:true,grid:{drawOnChartArea:false},ticks:{font:{size:10}}}}}});
}
chartMain=createChart();

function updateChart(powers,currents,label,timestamps,voltages){
  if(!chartMain)return;
  const n=Math.max(powers.length,currents.length,voltages?voltages.length:0);
  const labels=timestamps?timestamps.map(t=>{const d=new Date(t);return d.getHours().toString().padStart(2,'0')+':'+d.getMinutes().toString().padStart(2,'0')}):Array.from({length:n},(_,i)=>i);
  if(voltages&&voltages.length){
    const mn=Math.min(...voltages),mx=Math.max(...voltages);
    const pad=Math.max(5,Math.ceil((mx-mn)/2));
    chartMain.options.scales.yVolt.suggestedMin=mn-pad;
    chartMain.options.scales.yVolt.suggestedMax=mx+pad;
  }
  chartMain.data.labels=labels;
  chartMain.data.datasets[0].data=voltages||[];
  chartMain.data.datasets[1].data=powers;
  chartMain.data.datasets[2].data=currents;
  chartMain.update('none');
  if($('chartLabel'))$('chartLabel').textContent=label||'';
}

function decodeLogs(hex,bootFilter){
  const raw=hexToBytes(hex),powers=[],currents=[];
  for(let i=0;i+11<=raw.length;i+=11){
    const b=raw.slice(i,i+11);
    const boot=b[10];
    if(bootFilter!==undefined&&boot!==bootFilter)continue;
    const t=b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24);
    const v=200+b[4];
    const a=(b[5]|(b[6]<<8))/10;
    const pf=b[8];
    if(pf>=254)continue;
    powers.push(Math.round(v*a));
    currents.push(parseFloat(a.toFixed(1)));
  }
  return{powers,currents};
}

async function fetchBootLogs(bootId,count,bootStartMs){
  let all=[],cursorT=0;
  for(let page=0;page<30;page++){
    let url='/data/api?count='+count+'&dir=fromboot&boot='+bootId;
    if(cursorT>0)url+='&time='+cursorT;
    const d=await(await fetch(url)).json();
    if(!d.logs||!d.sentCount)break;
    const raw=hexToBytes(d.logs);
    let lastT=0,matched=0;
    for(let i=0;i+11<=raw.length;i+=11){
      const b=raw.slice(i,i+11);
      const t=b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24);
      const boot=b[10];
      if(boot===bootId&&b[8]<254){all.push({t,b});matched++}
      lastT=t;
    }
    cursorT=lastT;
    if(d.sentCount<count||!matched)break;
  }
  all.sort((a,b)=>a.t-b.t);
  const powers=[],currents=[],voltages=[],timestamps=[];
  for(const e of all){
    const v=200+e.b[4];
    const a=(e.b[5]|(e.b[6]<<8))/10;
    powers.push(Math.round(v*a));
    currents.push(parseFloat(a.toFixed(1)));
    voltages.push(v);
    if(bootStartMs)timestamps.push(bootStartMs+e.t*1000);
  }
  return{powers,currents,voltages,timestamps,total:all.length};
}

function fmtDur(s){if(!s)return'';const h=Math.floor(s/3600),m=Math.floor((s%3600)/60);return(h?h+'h ':'')+m+'m'}
function fmtWh(w){return w>=1000?(w/1000).toFixed(2)+' kWh':w+' Wh'}
async function discoverBoots(){
  try{
    const d=await(await fetch('/data/boots')).json();
    const sel=$('bootSel');
    while(sel.options.length>1)sel.remove(1);
    (d.boots||[]).slice().reverse().forEach(b=>{
      const o=document.createElement('option');o.value=b.id;
      o.textContent='Boot #'+b.id+' — '+fmtDur(b.duration)+' · '+fmtWh(b.totalWh);
      sel.appendChild(o);
    });
  }catch(e){}
}

async function loadBootData(){
  const sel=$('bootSel'),bootId=parseInt(sel.value);
  if(!bootId){selectedBoot=0;livePower=[];liveCurrent=[];liveVoltage=[];liveTimestamps=[];updateChart([],[],'Live — waiting for data…');return}
  selectedBoot=bootId;
  if($('chartLabel'))$('chartLabel').textContent='Loading boot #'+bootId+'…';
  try{
    const opt=[...sel.options].find(o=>o.value==bootId);
    const durMs=(opt?opt.textContent.match(/(\d+)h\s*(\d+)m/):null)||[];
    const h=parseInt(durMs[1])||0,m=parseInt(durMs[2])||0;
    const bootDurMs=(h*3600+m*60)*1000;
    const st=await(await fetch('/status')).json();
    const serverNowMs=st.timeValid?st.timeUnix*1000:Date.now();
    const bootStartMs=serverNowMs-bootDurMs;
    const r=await fetchBootLogs(bootId,200,bootStartMs);
    updateChart(r.powers,r.currents,'Boot #'+bootId+' — '+r.total+' entries',r.timestamps,r.voltages);
  }catch(e){if($('chartLabel'))$('chartLabel').textContent='Failed to load boot #'+bootId}
}
$('bootSel').addEventListener('change',loadBootData);

async function refresh(){
  try{
    const s=await(await fetch('/status')).json();
    $('testBanner').style.display=s.mock?'block':'none';
    if(s.pzemValid||s.mock){$('nVolt').textContent=s.voltage.toFixed(0)+' V';$('nCur').textContent=s.current.toFixed(1)+' A';$('nPow').textContent=s.power>=1000?(s.power/1000).toFixed(2)+' kW':s.power.toFixed(0)+' W';$('nHz').textContent=s.frequency.toFixed(1)+' Hz';$('nPF').textContent=s.current>.5?s.pf.toFixed(2):'--';$('pfHint').style.display=s.current>.5?'none':'block';$('nEn').textContent=s.energyKwh.toFixed(2)+' kWh'}
    else{$('nVolt').textContent='--';$('nCur').textContent='--';$('nPow').textContent='--';$('nHz').textContent='--';$('nPF').textContent='--';$('nEn').textContent='--';$('pfHint').style.display='none'}
    $('uptime').textContent=s.uptime;const b=$('stateBadge');b.className='badge '+(s.pumpState==='RUNNING'?'badge-running':(s.pumpState==='TRIPPED'?'badge-tripped':'badge-stopped'));b.textContent=s.pumpState;$('tripBadge').style.display=s.trips?'inline-block':'none';
    const vs=$('voltageStatus');if(!s.pzemValid&&!s.mock){vs.className='voltage-status vs-none';vs.textContent='METER OFFLINE'}else{vs.className='voltage-status '+({NORMAL:'vs-normal',WARNING:'vs-warning',CRITICAL:'vs-critical'}[s.voltageStatus]||'vs-normal');vs.textContent={NORMAL:'VOLTAGE OK',WARNING:'VOLTAGE HIGH',CRITICAL:'VOLTAGE CRITICAL'}[s.voltageStatus]||s.voltageStatus}
    $('dashMeterHint').style.display=(!s.pzemValid&&!s.mock)?'block':'none';
    if(!selectedBoot&&(s.pumpState==='RUNNING'||s.pumpState==='STARTING')){
      livePower.push(Math.round(s.power));liveCurrent.push(s.current);liveVoltage.push(s.voltage);liveTimestamps.push(Date.now());
      if(livePower.length>200){livePower.shift();liveCurrent.shift();liveVoltage.shift();liveTimestamps.shift()}
      updateChart(livePower,liveCurrent,'Live — '+livePower.length+' points',liveTimestamps,liveVoltage);
    }
    lastGoodTs=new Date();document.querySelector('.numerics').classList.remove('stale');$('lastUpd').textContent='last update '+lastGoodTs.toLocaleTimeString()
  }catch(e){$('lastUpd').textContent=lastGoodTs?'⚠ STALE — last '+lastGoodTs.toLocaleTimeString()+' · unreachable':'unreachable — retrying…';const n=document.querySelector('.numerics');if(n)n.classList.add('stale')}}
pollSel.addEventListener('change',()=>{clearInterval(window._ti);window._ti=setInterval(refresh,parseInt(pollSel.value))});
window._ti=setInterval(refresh,parseInt(pollSel.value));refresh();discoverBoots();
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
button.danger-arm{box-shadow:0 0 0 3px #fca5a5;animation:pulse-red 1s infinite;font-weight:700}
@keyframes pulse-red{50%{opacity:.7}}
.msg{min-height:20px;font-size:13px;font-weight:700;text-align:center;margin-top:8px}
.msg.good{color:#16a34a}.msg.bad{color:#dc2626}
.time-row{display:flex;gap:10px;align-items:center}
.time-row input[type="time"]{flex:1;padding:10px;border:1px solid #e2e8f0;border-radius:10px;font-size:15px;background:#fff}
.time-row span{font-size:12px;color:#64748b;font-weight:600}
.days-row{display:flex;gap:4px;flex-wrap:wrap;margin-top:6px}
.day-chip{width:36px;height:36px;border-radius:50%;border:2px solid #e2e8f0;background:#fff;font-size:11px;font-weight:700;color:#94a3b8;display:flex;align-items:center;justify-content:center;cursor:pointer;transition:all .15s;user-select:none;-webkit-user-select:none}
.day-chip.active{background:#3b82f6;border-color:#3b82f6;color:#fff}
.day-chip:active{transform:scale(.9)}
.sch-hint{font-size:11px;color:#64748b;text-align:center;padding:8px;background:#f8fafc;border-radius:8px;margin-top:8px}
.sch-hint.warn{color:#92400e;background:#fef3c7;border:1px solid #fde68a}
.sch-overlap{font-size:11px;color:#92400e;background:#fef3c7;border:1px solid #fde68a;border-radius:8px;padding:6px 10px;margin-top:8px;text-align:center}
.banner-test{background:#fef3c7;border:1px solid #fde68a;color:#92400e;border-radius:10px;padding:8px 12px;font-weight:600;font-size:12px;margin-bottom:12px;text-align:center}
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
  <div class="hdr"><div class="hdr-logo">⚙️</div><h1>Pump Settings</h1></div>
  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE currently ON — simulated readings used.</div>

  <div class="sec" id="secBasic">
    <div class="sec-hdr" onclick="toggleSec('secBasic')">
      <div class="sec-left">
        <div class="sec-icon" style="background:#dcfce7;color:#166534">⚡</div>
        <div><div class="sec-title">Basic Settings</div><div class="sec-sub">Protections &amp; test</div></div>
      </div>
      <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6,9 12,15 18,9"/></svg>
    </div>
    <div class="sec-body"><div class="sec-inner">

      <div class="card" id="testCard">
        <h3>🧪 Test Mode</h3>
        <p class="cardhelp">Simulate meter readings for testing protections.</p>
        <div class="field"><label><input type="checkbox" id="cMock" style="width:auto;height:auto;transform:scale(1.3);margin-right:8px;vertical-align:middle"> Use test readings</label></div>
        <div class="field"><label for="cMockProfile">Scenario</label><p class="help">Simulated scenario for the power meter.</p><div id="mockProfileHint" style="display:none;font-size:11px;color:#64748b">Turn on "Use test readings" above first.</div><select id="cMockProfile" disabled><option value="off">Pump off</option><option value="running">Running normally</option><option value="dryrun">Dry run</option><option value="oc">Overload</option></select></div>
      </div>

      <div class="card"><h3>🛡 Overload Protection</h3><p class="cardhelp">Stops the pump when the motor draws too much current.</p>
        <div class="field"><label for="ocRunning">Normal current limit (A)</label><p class="help">Pump stops if current exceeds this for the delay period.</p><input type="number" id="ocRunning" step="0.1" min="5" max="50"><div class="unit-hint">e.g. 9.6A pump → set 12A</div></div>
        <div class="field"><label for="ocStartInstant">Start-up current limit (A)</label><p class="help">Instant spike threshold during start-up.</p><input type="number" id="ocStartInstant" min="20" max="100"></div>
        <div class="field"><label for="ocDelay">Confirm delay (s)</label><p class="help">Overload must last this long before stopping.</p><input type="number" id="ocDelay" min="1" max="30"></div>
      </div>

      <div class="card"><h3>💧 No-Water Protection</h3><p class="cardhelp">Stops the pump when it runs dry.</p>
        <div class="field"><label for="dryRunCurrent">Dry-run current (A)</label><input type="number" id="dryRunCurrent" step="0.1" min="1" max="10"></div>
        <div class="field"><label for="dryRunPower">Dry-run power (W)</label><p class="help">Both current AND power must be low.</p><input type="number" id="dryRunPower" min="100" max="2000"></div>
        <div class="field"><label for="dryRunDelay">Confirm delay (s)</label><input type="number" id="dryRunDelay" min="1" max="300"></div>
        <div class="field"><label for="dryRunActivation">Wait after start (s)</label><p class="help">Protection activates after this delay.</p><input type="number" id="dryRunActivation" min="0" max="3600"></div>
      </div>

      <div class="card"><h3>⚡ Voltage Protection</h3><p class="cardhelp">Stops the pump on dangerous mains voltage.</p>
        <div class="field"><label for="voltOver">Over-voltage trip (V)</label><input type="number" id="voltOver" min="200" max="280"></div>
        <div class="field"><label for="voltUnder">Under-voltage stop (V)</label><input type="number" id="voltUnder" min="150" max="230"></div>
        <div class="field"><label for="voltWarn">Warning voltage (V)</label><input type="number" id="voltWarn" min="240" max="280"></div>
        <div class="field"><label for="voltCritical">Critical voltage (V)</label><input type="number" id="voltCritical" min="250" max="300"></div>
        <div class="field"><label for="voltageDelay">Confirm delay (s)</label><input type="number" id="voltageDelay" min="1" max="60"></div>
        <div class="field"><label for="voltageLockout">Voltage lockout (s)</label><input type="number" id="voltageLockout" min="0" max="3600"></div>
      </div>

      <div class="card"><h3>⏱ Max Run Time</h3><p class="cardhelp">Stops the pump after a set duration. 0 = disabled.</p>
        <div class="field"><label for="maxRunTime">Max run time (s)</label><p class="help">Pump stops automatically after this time. Default 10800s (3h).</p><input type="number" id="maxRunTime" min="0" max="86400" step="60"></div>
      </div>

    </div></div>
  </div>

  <div class="sec" id="secSchedule">
    <div class="sec-hdr" onclick="toggleSec('secSchedule')">
      <div class="sec-left">
        <div class="sec-icon" style="background:#dbeafe;color:#1e40af">⏰</div>
        <div><div class="sec-title">Schedule</div><div class="sec-sub">Auto start/stop by time</div></div>
      </div>
      <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6,9 12,15 18,9"/></svg>
    </div>
    <div class="sec-body"><div class="sec-inner">

      <div class="sch-hint" id="schTimeHint">⏳ Waiting for time sync…</div>

      <div class="card" id="schCard0"><h3>Schedule 1</h3>
        <div class="field"><label><input type="checkbox" id="sch0Enabled" class="sch-cb" data-i="0" style="width:auto;height:auto;transform:scale(1.3);margin-right:8px;vertical-align:middle"> Enable</label></div>
        <div class="field"><label>Start time</label><div class="time-row"><input type="time" id="sch0Start" value="06:00"></div></div>
        <div class="field"><label>Stop time</label><div class="time-row"><input type="time" id="sch0Stop" value="18:00"></div></div>
        <div class="field"><label>Active days</label><div class="days-row" data-sch="0">
          <div class="day-chip active" data-bit="1">S</div>
          <div class="day-chip active" data-bit="2">M</div>
          <div class="day-chip active" data-bit="4">T</div>
          <div class="day-chip active" data-bit="8">W</div>
          <div class="day-chip active" data-bit="16">T</div>
          <div class="day-chip active" data-bit="32">F</div>
          <div class="day-chip active" data-bit="64">S</div>
        </div></div>
        <div class="sch-overlap" id="schOverlap0" style="display:none">⚠ Overlaps with another schedule</div>
      </div>

      <div class="card" id="schCard1"><h3>Schedule 2</h3>
        <div class="field"><label><input type="checkbox" id="sch1Enabled" class="sch-cb" data-i="1" style="width:auto;height:auto;transform:scale(1.3);margin-right:8px;vertical-align:middle"> Enable</label></div>
        <div class="field"><label>Start time</label><div class="time-row"><input type="time" id="sch1Start" value="06:00"></div></div>
        <div class="field"><label>Stop time</label><div class="time-row"><input type="time" id="sch1Stop" value="18:00"></div></div>
        <div class="field"><label>Active days</label><div class="days-row" data-sch="1">
          <div class="day-chip active" data-bit="1">S</div>
          <div class="day-chip active" data-bit="2">M</div>
          <div class="day-chip active" data-bit="4">T</div>
          <div class="day-chip active" data-bit="8">W</div>
          <div class="day-chip active" data-bit="16">T</div>
          <div class="day-chip active" data-bit="32">F</div>
          <div class="day-chip active" data-bit="64">S</div>
        </div></div>
        <div class="sch-overlap" id="schOverlap1" style="display:none">⚠ Overlaps with another schedule</div>
      </div>

      <div class="card" id="schCard2"><h3>Schedule 3</h3>
        <div class="field"><label><input type="checkbox" id="sch2Enabled" class="sch-cb" data-i="2" style="width:auto;height:auto;transform:scale(1.3);margin-right:8px;vertical-align:middle"> Enable</label></div>
        <div class="field"><label>Start time</label><div class="time-row"><input type="time" id="sch2Start" value="06:00"></div></div>
        <div class="field"><label>Stop time</label><div class="time-row"><input type="time" id="sch2Stop" value="18:00"></div></div>
        <div class="field"><label>Active days</label><div class="days-row" data-sch="2">
          <div class="day-chip active" data-bit="1">S</div>
          <div class="day-chip active" data-bit="2">M</div>
          <div class="day-chip active" data-bit="4">T</div>
          <div class="day-chip active" data-bit="8">W</div>
          <div class="day-chip active" data-bit="16">T</div>
          <div class="day-chip active" data-bit="32">F</div>
          <div class="day-chip active" data-bit="64">S</div>
        </div></div>
        <div class="sch-overlap" id="schOverlap2" style="display:none">⚠ Overlaps with another schedule</div>
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
        <div class="field"><label for="startSuccessCurrent">Min run current (A)</label><input type="number" id="startSuccessCurrent" step="0.1" min="0.5" max="5"></div>
        <div class="field"><label for="startVerifyDelay">Start verify delay (s)</label><input type="number" id="startVerifyDelay" min="1" max="10"></div>
        <div class="field"><label for="startFailBlock">Start-fail lockout (s)</label><input type="number" id="startFailBlock" min="1" max="600"></div>
        <div class="field"><label for="minRun">Minimum run (s)</label><input type="number" id="minRun" min="10" max="300"></div>
        <div class="field"><label for="minOff">Minimum off (s)</label><input type="number" id="minOff" min="10" max="600"></div>
      </div>

      <div class="card"><h3>🔁 Auto Restart</h3><p class="cardhelp">3 fast repeats = permanent lockout (manual reset only).</p>
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
  'startSuccessCurrent','startVerifyDelay','startFailBlock','minRun','minOff','maxRunTime',
  'autoRetryDelay','maxRetries','logIntervalRunning','logIntervalOff','pzemReadRunning'];
function syncMockUI(){$('cMockProfile').disabled=!$('cMock').checked;$('cMockProfile').closest('.field').style.opacity=$('cMock').checked?1:.55;$('mockProfileHint').style.display=$('cMock').checked?'none':'block'}
async function load(){const s=await(await fetch('/settings/api')).json();$('testBanner').style.display=s.mock?'block':'none';$('cMock').checked=s.mock;syncMockUI();fields.forEach(f=>{if($(f))$(f).value=s[f]});
  $('tb0').checked=!!(s.tripBehavior&1);$('tb1').checked=!!(s.tripBehavior&2);$('tb2').checked=!!(s.tripBehavior&4);$('tb3').checked=!!(s.tripBehavior&8);$('tb4').checked=!!(s.tripBehavior&16);$('tb5').checked=!!(s.tripBehavior&32);
  for(let i=0;i<3;i++){
    $('sch'+i+'Enabled').checked=!!s['sch'+i+'Enabled'];
    $('sch'+i+'Start').value=String(s['sch'+i+'StartH']||0).padStart(2,'0')+':'+String(s['sch'+i+'StartM']||0).padStart(2,'0');
    $('sch'+i+'Stop').value=String(s['sch'+i+'StopH']||0).padStart(2,'0')+':'+String(s['sch'+i+'StopM']||0).padStart(2,'0');
    const days=s['sch'+i+'Days']||0x7F;
    document.querySelectorAll('.days-row[data-sch="'+i+'"] .day-chip').forEach(c=>{c.classList.toggle('active',!!(days&parseInt(c.dataset.bit)))});
    const ov=$('schOverlap'+i);if(ov)ov.style.display=(s.schOverlap&(1<<i))?'block':'none';
  }
  const hint=$('schTimeHint');
  if(s.timeValid){hint.className='sch-hint';hint.textContent='✓ Time synced'}
  else{hint.className='sch-hint warn';hint.textContent='⏳ Waiting for time sync — schedule inactive until time is available (NTP or browser sync)'}}
function feed(t,cls){const el=$('msg');el.textContent=t;el.className='msg '+(cls||'');clearTimeout(feed._t);feed._t=setTimeout(()=>{if(el.textContent===t){el.textContent='';el.className='msg'}},4000)}
$('btnSave').onclick=async()=>{const bad=fields.filter(f=>{const el=$(f);if(!el||!el.value)return false;const v=parseFloat(el.value),mn=el.min?parseFloat(el.min):-Infinity,mx=el.max?parseFloat(el.max):Infinity;return isNaN(v)||v<mn||v>mx});
  if(bad.length){feed('Out of range: '+bad.join(', '),'bad');return}const btn=$('btnSave');btn.disabled=true;btn.textContent='Saving…';const p=new URLSearchParams();
  fields.forEach(f=>{if($(f))p.append(f,$(f).value)});p.append('mock',$('cMock').checked?'1':'0');let tb=0;
  if($('tb0').checked)tb|=1;if($('tb1').checked)tb|=2;if($('tb2').checked)tb|=4;if($('tb3').checked)tb|=8;if($('tb4').checked)tb|=16;if($('tb5').checked)tb|=32;p.append('tripBehavior',tb);
  for(let i=0;i<3;i++){
    p.append('sch'+i+'Enabled',$('sch'+i+'Enabled').checked?'1':'0');
    const st=$('sch'+i+'Start').value.split(':');p.append('sch'+i+'StartH',parseInt(st[0]||0));p.append('sch'+i+'StartM',parseInt(st[1]||0));
    const sp=$('sch'+i+'Stop').value.split(':');p.append('sch'+i+'StopH',parseInt(sp[0]||0));p.append('sch'+i+'StopM',parseInt(sp[1]||0));
    let days=0;document.querySelectorAll('.days-row[data-sch="'+i+'"] .day-chip.active').forEach(c=>days|=parseInt(c.dataset.bit));p.append('sch'+i+'Days',days||0x7F);
  }
  try{const r=await fetch('/settings/api?'+p.toString(),{method:'POST'});feed(r.status===200?'✓ Settings saved':'✗ Save failed ('+r.status+')',r.status===200?'good':'bad');$('testBanner').style.display=$('cMock').checked?'block':'none'}
  catch(e){feed('✗ Save failed — unreachable','bad')}btn.disabled=false;btn.textContent='💾 SAVE ALL SETTINGS'};
$('cMock').addEventListener('change',()=>{syncMockUI();feed($('cMock').checked?'Test readings ON on save':'Test readings OFF on save')});
$('cMockProfile').addEventListener('change',()=>{fetch('/settings/api?mockProfile='+$('cMockProfile').value,{method:'POST'}).then(()=>feed('✓ Profile applied','good'));$('cMock').checked=true;syncMockUI();$('testBanner').style.display='block'});
const bcd=$('btnClearData');bcd.onclick=()=>{if(bcd.dataset.armed==='1'){bcd.dataset.armed='0';bcd.textContent='🗑 Clear History (restarts)';fetch('/settings/clear').then(()=>feed('Erasing…'));return}bcd.dataset.armed='1';bcd.textContent='⚠ TAP AGAIN to confirm';bcd.classList.add('danger-arm');clearTimeout(bcd._t);bcd._t=setTimeout(()=>{bcd.dataset.armed='0';bcd.textContent='🗑 Clear History (restarts)';bcd.classList.remove('danger-arm')},5000)};
const bdf=$('btnDefaults');bdf.onclick=async()=>{if(bdf.dataset.armed!=='1'){bdf.dataset.armed='1';bdf.textContent='⚠ TAP AGAIN to reset';bdf.classList.add('danger-arm');clearTimeout(bdf._t);bdf._t=setTimeout(()=>{bdf.dataset.armed='0';bdf.textContent='↺ Reset to Defaults';bdf.classList.remove('danger-arm')},5000);return}
  bdf.dataset.armed='0';bdf.textContent='↺ Reset to Defaults';bdf.classList.remove('danger-arm');
  try{const r=await fetch('/settings/api?action=defaults',{method:'POST'});if(!r.ok)throw 0;feed('✓ Reset to defaults','good');load()}catch(e){feed('✗ Failed','bad')}};
document.querySelectorAll('.days-row').forEach(row=>{row.querySelectorAll('.day-chip').forEach(c=>c.addEventListener('click',()=>c.classList.toggle('active')))});
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
  <div id="testBanner" class="banner-test" style="display:none">🧪 TEST MODE</div>

  <div class="card">
    <h3>Recorded readings</h3>
    <p class="cardhelp">Newest first. LOAD MORE adds 100 rows.</p>
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
  </div>

  <nav class="tab-bar">
    <a href="/"><svg viewBox="0 0 24 24"><path d="M12 2.69l5.66 5.66a8 8 0 11-11.31 0z"/></svg>Control</a>
    <a href="/dashboard"><svg viewBox="0 0 24 24"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>Dashboard</a>
    <a href="/settings"><svg viewBox="0 0 24 24"><line x1="4" y1="21" x2="4" y2="3"/><line x1="12" y1="21" x2="12" y2="3"/><line x1="20" y1="21" x2="20" y2="3"/><line x1="1" y1="14" x2="7" y2="14"/><line x1="17" y1="16" x2="23" y2="16"/></svg>Settings</a>
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
async function loadMore(){const btn=$('loadMore'),errEl=$('loadErr');btn.disabled=true;btn.textContent='Loading…';if(errEl)errEl.style.display='none';let url='/data/api?count=100&dir=back';if(sinceBoot||sinceTime)url+='&boot='+sinceBoot+'&time='+sinceTime;let d;try{d=await(await fetch(url)).json()}catch(e){btn.disabled=false;btn.textContent='⬇ LOAD MORE';if(errEl){errEl.style.display='block';errEl.textContent='Could not load — check connection.'}return}
  btn.disabled=false;btn.textContent='⬇ LOAD MORE';if(sinceBoot===0&&sinceTime===0)$('rows').innerHTML='';const rows=hexToBytes(d.logs),tbody=$('rows');let added=0;
  for(let i=0;i+11<=rows.length;i+=11){const b=rows.slice(i,i+11),timeSec=b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24),volt=200+b[4],cur=((b[5]|(b[6]<<8))/10).toFixed(1),pf=b[8]>=254?'-':(b[8]/100).toFixed(2),st=b[9],boot=b[10];
    const tr=document.createElement('tr');tr.innerHTML='<td>'+boot+'</td><td>'+timeSec+'</td><td>'+volt+'</td><td>'+cur+'</td><td>'+b[7]+'</td><td>'+pf+'</td><td>'+chips(st)+'</td>';tbody.appendChild(tr);sinceBoot=boot;sinceTime=timeSec;added++}
  const okEl=$('loadOk');if(okEl){okEl.style.display='block';okEl.textContent=added>0?'✓ Loaded '+added+' row'+(added>1?'s':''):'✓ Up to date';clearTimeout(loadMore._okT);loadMore._okT=setTimeout(()=>{okEl.style.display='none'},4000)}
  if(added===0&&d.totalLogs===0){$('summary').textContent='No entries yet — saves automatically while running.'}
  else{const shown=tbody.children.length,total=d.totalLogs;if(shown>=total||added===0){$('summary').textContent=total+' total · all '+shown+' shown';btn.disabled=true;btn.textContent='✓ All loaded'}
  else{const pct=total?Math.round(shown/total*100):0;btn.disabled=false;btn.textContent='⬇ LOAD MORE ('+shown+'/'+total+' · ~'+pct+'%)';$('summary').textContent=total+' total · '+shown+' shown (newest first)'}}
}
$('loadMore').onclick=loadMore;loadMore();
fetch('/status').then(r=>r.json()).then(s=>{$('testBanner').style.display=s.mock?'block':'none'});
</script>
</body>
</html>
)rawliteral";

#endif
