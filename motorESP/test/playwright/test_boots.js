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
  const status = JSON.parse(await fetchUrl('http://192.168.100.100/status'));
  console.log('Current bootId:', status.bootId, ', Sector:', status.currentSector);
  console.log('');
  
  // Test various boot IDs
  const bootIds = [0, 1, 10, 20, 30, 40, 50, 60, 70, status.bootId];
  
  for (const bootId of bootIds) {
    const data = JSON.parse(await fetchUrl(`http://192.168.100.100/data?boot=${bootId}&time=0&count=5`));
    if (data.logs && data.sentCount > 0) {
      const bytes = Buffer.from(data.logs, 'hex');
      const timeSec = bytes.readUInt32LE(0);
      const bootIdInData = bytes[7];
      console.log(`boot=${bootId}: sent=${data.sentCount}, first entry: timeSec=${timeSec}, bootId=${bootIdInData}`);
    }
  }
}

test().catch(console.error);