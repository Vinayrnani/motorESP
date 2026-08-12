const http = require('http');

function fetchUrl(url) {
  return new Promise((resolve, reject) => {
    const req = http.get(url, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve(data));
    });
    req.on('error', reject);
  });
}

async function test() {
  console.log('=== Testing ESP8266 Eggubator Data Fetch ===\n');
  
  // Get status first
  const status = JSON.parse(await fetchUrl('http://192.168.100.100/status'));
  console.log('Device Status:');
  console.log('  Boot ID:', status.bootId);
  console.log('  Current Sector:', status.currentSector);
  console.log('  Uptime:', status.uptime);
  console.log('  Version:', status.version);
  console.log('');
  
  // Fetch data from boot=0, time=0 (should get oldest)
  console.log('=== Fetch #1: boot=0, time=0 ===');
  const data1 = JSON.parse(await fetchUrl('http://192.168.100.100/data?boot=0&time=0&count=50'));
  console.log('  sentCount:', data1.sentCount);
  console.log('  logs length:', data1.logs ? data1.logs.length : 0);
  
  // Decode first few entries
  if (data1.logs && data1.sentCount > 0) {
    const bytes = Buffer.from(data1.logs, 'hex');
    const entries = [];
    for (let i = 0; i < Math.min(5, data1.sentCount); i++) {
      const offset = i * 8;
      if (offset + 8 > bytes.length) break;
      const timeSec = bytes.readUInt32LE(offset);
      const temp = bytes[offset+4] / 10 + 20;
      const hum = bytes[offset+5];
      const states = bytes[offset+6];
      const bootId = bytes[offset+7];
      
      entries.push({ timeSec, temp, hum, bootId });
    }
    console.log('  First 5 entries:');
    entries.forEach((e, i) => {
      console.log(`    [${i}] timeSec=${e.timeSec}, temp=${e.temp}, hum=${e.hum}, bootId=${e.bootId}`);
    });
  }
  
  console.log('');
  
  // Fetch data from current boot
  console.log('=== Fetch #2: boot=' + status.bootId + ', time=0 ===');
  const data2 = JSON.parse(await fetchUrl('http://192.168.100.100/data?boot=' + status.bootId + '&time=0&count=50'));
  console.log('  sentCount:', data2.sentCount);
  console.log('  logs length:', data2.logs ? data2.logs.length : 0);
  
  if (data2.logs && data2.sentCount > 0) {
    const bytes = Buffer.from(data2.logs, 'hex');
    const entries = [];
    for (let i = 0; i < Math.min(5, data2.sentCount); i++) {
      const offset = i * 8;
      if (offset + 8 > bytes.length) break;
      const timeSec = bytes.readUInt32LE(offset);
      const temp = bytes[offset+4] / 10 + 20;
      const hum = bytes[offset+5];
      const states = bytes[offset+6];
      const bootId = bytes[offset+7];
      
      entries.push({ timeSec, temp, hum, bootId });
    }
    console.log('  First 5 entries:');
    entries.forEach((e, i) => {
      console.log(`    [${i}] timeSec=${e.timeSec}, temp=${e.temp}, hum=${e.hum}, bootId=${e.bootId}`);
    });
  }
  
  console.log('\n=== Analysis ===');
  console.log('If boot=0 returns same data as current boot, the ESP is returning the WRONG oldest data.');
  console.log('The circular buffer logic might be starting from wrong position.');
}

test().catch(console.error);