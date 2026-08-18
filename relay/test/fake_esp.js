#!/usr/bin/env node
/*
 * fake_esp.js — Simulates the ESP8266 tunnel client for local relay testing.
 *
 * Connects to the relay tunnel port (default localhost:9000 like the real ESP),
 * parses complete HTTP requests (head + Content-Length body), and replies with
 * a keep-alive HTTP/1.1 200 echo — exactly like ESP8266WebServer on the pump.
 *
 * Usage:  node relay/test/fake_esp.js [relayTunnelHost] [relayTunnelPort]
 */

'use strict';

const net = require('net');

const HOST = process.argv[2] || '127.0.0.1';
const PORT = parseInt(process.argv[3] || '9000', 10);

const CRLF = '\r\n';
let seq = 0;
let buf = Buffer.alloc(0);

function findHeadEnd(b) {
  for (let i = 0; i + 3 < b.length; i++) {
    if (b[i] === 0x0d && b[i + 1] === 0x0a && b[i + 2] === 0x0d && b[i + 3] === 0x0a) return i + 4;
  }
  return -1;
}

/* If the buffer holds one whole request, consume it and return a response. */
function tryRequest() {
  const headEnd = findHeadEnd(buf);
  if (headEnd === -1) return false;
  const head = buf.subarray(0, headEnd).toString('latin1');
  const m = /(?:^|\r\n)content-length:\s*(\d+)/i.exec(head);
  const cl = m ? parseInt(m[1], 10) : 0;
  if (buf.length < headEnd + cl) return false;

  const [method, path] = head.split(/\r\n/)[0].split(/\s+/);
  buf = buf.subarray(headEnd + cl);

  seq++;
  const body = `fakeESP responds to ${method} ${path} (request #${seq}, body ${cl} B)\n`;
  return `HTTP/1.1 200 OK${CRLF}` +
    `Content-Type: text/plain${CRLF}` +
    `Content-Length: ${Buffer.byteLength(body)}${CRLF}` +
    `Connection: keep-alive${CRLF}` +
    CRLF + body;
}

function connect() {
  const sock = net.createConnection({ host: HOST, port: PORT });
  sock.setNoDelay(true);
  sock.on('connect', () => console.log(`[fake_esp] connected to relay ${HOST}:${PORT}`));
  sock.on('data', (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    for (;;) {
      const resp = tryRequest();
      if (resp === false) break;
      sock.write(resp);
      console.log(`[fake_esp] replied #${seq}`);
    }
  });
  sock.on('error', (e) => console.log(`[fake_esp] error: ${e.code || e.message}`));
  sock.on('close', () => {
    console.log('[fake_esp] relay closed connection — reconnecting in 2s');
    setTimeout(connect, 2000);
  });
  return sock;
}

connect();