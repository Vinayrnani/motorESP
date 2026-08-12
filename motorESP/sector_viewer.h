#ifndef SECTOR_VIEWER_H
#define SECTOR_VIEWER_H

#include <Arduino.h>

const char SECTOR_VIEWER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sector Viewer</title>
<style>
:root {
  --primary: #3b82f6;
  --bg: #1a1a2e;
  --surface: #16213e;
  --text: #e0e0e0;
  --on: #22c55e;
  --off: #ef4444;
  --border: #2a2a4a;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: var(--bg); color: var(--text); padding: 20px; max-width: 1000px; margin: 0 auto; }
h1 { color: var(--primary); margin-bottom: 16px; font-size: 1.5rem; }
.controls { display: flex; gap: 8px; align-items: center; margin-bottom: 16px; flex-wrap: wrap; }
.controls label { font-size: 0.9rem; }
.controls input { padding: 6px 10px; background: var(--surface); border: 1px solid var(--border); border-radius: 4px; color: var(--text); width: 80px; font-size: 0.9rem; }
.controls button { padding: 6px 14px; background: var(--primary); color: #fff; border: none; border-radius: 4px; cursor: pointer; font-size: 0.9rem; }
.controls button:hover { opacity: 0.85; }
.controls button:disabled { opacity: 0.5; cursor: default; }
.hex-grid { font-family: 'Courier New', monospace; font-size: 12px; line-height: 1.5; background: var(--surface); border: 1px solid var(--border); border-radius: 4px; padding: 12px; overflow-x: auto; white-space: pre; margin-bottom: 16px; min-height: 100px; }
.hex-grid .offset { color: var(--primary); }
.hex-grid .ascii { color: #888; }
.editor-area { width: 100%; min-height: 200px; background: var(--surface); border: 1px solid var(--border); border-radius: 4px; color: var(--text); font-family: 'Courier New', monospace; font-size: 12px; padding: 12px; resize: vertical; margin-bottom: 12px; }
.actions { display: flex; gap: 8px; margin-bottom: 16px; }
.actions button { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; font-size: 0.9rem; }
.btn-save { background: var(--on); color: #000; }
.btn-save:hover { opacity: 0.85; }
.btn-back { background: var(--surface); color: var(--text); border: 1px solid var(--border) !important; }
.btn-back:hover { background: var(--border); }
.status { padding: 10px 14px; border-radius: 4px; margin-bottom: 12px; display: none; font-size: 0.9rem; }
.status.info { display: block; background: rgba(59,130,246,0.15); border-left: 3px solid var(--primary); }
.status.success { display: block; background: rgba(34,197,94,0.15); border-left: 3px solid var(--on); }
.status.error { display: block; background: rgba(239,68,68,0.15); border-left: 3px solid var(--off); }
</style>
</head>
<body>
<h1>&#128269; Sector Viewer</h1>
<div class="controls">
  <label for="sectorInput">Sector (0-255):</label>
  <input type="number" id="sectorInput" min="0" max="255" value="0">
  <button id="loadBtn" onclick="loadSector()">Load Sector</button>
  <span id="sectorInfo" style="font-size:0.8rem;color:#888;"></span>
</div>
<div id="hexDisplay" class="hex-grid">Load a sector to view its contents</div>
<textarea id="hexEditor" class="editor-area" placeholder="Edit hex bytes here (hex pairs separated by spaces, 16 per row)..."></textarea>
<div class="actions">
  <button class="btn-save" onclick="saveSector()">Save to Flash</button>
  <button class="btn-back" onclick="location.href='/'">&larr; Back to Dashboard</button>
</div>
<div id="statusMsg" class="status"></div>

<script>
let currentSector = -1;

function showStatus(msg, type) {
  const el = document.getElementById('statusMsg');
  el.className = 'status ' + type;
  el.textContent = msg;
}

async function loadSector() {
  const sector = parseInt(document.getElementById('sectorInput').value);
  if (isNaN(sector) || sector < 0 || sector > 255) {
    showStatus('Invalid sector number (0-255)', 'error');
    return;
  }
  
  const loadBtn = document.getElementById('loadBtn');
  loadBtn.disabled = true;
  loadBtn.textContent = 'Loading...';
  showStatus('Loading sector ' + sector + '...', 'info');
  
  try {
    const resp = await fetch('/sector_hex?sector=' + sector);
    if (!resp.ok) throw new Error('HTTP ' + resp.status);
    const buffer = await resp.arrayBuffer();
    const bytes = new Uint8Array(buffer);
    
    let hexDisplay = '';
    let hexEdit = '';
    for (let i = 0; i < bytes.length; i += 16) {
      const offset = sector * 4096 + i;
      hexDisplay += '<span class="offset">' + offset.toString(16).padStart(8, '0') + '</span>  ';
      let hexPart = '';
      let asciiPart = '';
      for (let j = 0; j < 16; j++) {
        if (i + j < bytes.length) {
          const b = bytes[i + j];
          hexPart += b.toString(16).padStart(2, '0') + ' ';
          asciiPart += (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.';
        } else {
          hexPart += '   ';
          asciiPart += ' ';
        }
        if (j === 7) { hexPart += ' '; }
      }
      hexDisplay += hexPart + ' <span class="ascii">' + asciiPart + '</span>\n';
      for (let j = 0; j < 16 && i + j < bytes.length; j++) {
        hexEdit += bytes[i + j].toString(16).padStart(2, '0');
        if (j < 15 && i + j + 1 < bytes.length) hexEdit += ' ';
        if (j === 7 && i + 8 < bytes.length) hexEdit += ' ';
      }
      hexEdit += '\n';
    }
    
    document.getElementById('hexDisplay').innerHTML = hexDisplay;
    document.getElementById('hexEditor').value = hexEdit;
    document.getElementById('sectorInfo').textContent = 'Sector ' + sector + ' loaded. Flash addr: 0x' + (0x200000 + sector * 4096).toString(16);
    currentSector = sector;
    showStatus('Sector ' + sector + ' loaded (' + bytes.length + ' bytes)', 'success');
  } catch (e) {
    showStatus('Error loading sector: ' + e.message, 'error');
  }
  
  loadBtn.disabled = false;
  loadBtn.textContent = 'Load Sector';
}

async function saveSector() {
  if (currentSector < 0) {
    showStatus('Load a sector first', 'error');
    return;
  }
  
  const text = document.getElementById('hexEditor').value;
  const hexStr = text.replace(/[\s\n\r]/g, '');
  if (hexStr.length === 0 || hexStr.length % 2 !== 0) {
    showStatus('Invalid hex data (must be even number of hex digits)', 'error');
    return;
  }
  
  showStatus('Saving to sector ' + currentSector + '...', 'info');
  
  try {
    const resp = await fetch('/sector_hex?sector=' + currentSector, {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' },
      body: hexStr
    });
    if (!resp.ok) throw new Error('HTTP ' + resp.status);
    showStatus('Sector ' + currentSector + ' saved successfully!', 'success');
  } catch (e) {
    showStatus('Error saving sector: ' + e.message, 'error');
  }
}

window.addEventListener('keydown', function(e) {
  if (e.key === 'Enter' && e.target === document.getElementById('sectorInput')) {
    loadSector();
  }
});
</script>
</body>
</html>
)rawliteral";

#endif
