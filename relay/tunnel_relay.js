#!/usr/bin/env node
/*
 * tunnel_relay.js — Raw TCP reverse-tunnel relay for the motorESP pump controller.
 *
 *   Browser ──► Port B (8080, public HTTP) ──┐
 *                                            │  this relay
 *   ESP8266 ──► Port A (9000, outbound TCP) ─┘
 *
 * The ESP behind CGNAT keeps a persistent outbound TCP connection to Port A.
 * The ESP serves exactly ONE HTTP request at a time over that single stream,
 * so the relay does the multiplexing:
 *   1. buffer each browser's bytes until a WHOLE request arrived (head + body),
 *   2. enqueue requests FIFO, forward each to the ESP exactly once,
 *   3. parse the ESP byte stream for COMPLETE HTTP responses (Content-Length,
 *      chunked, or bodyless 204/304/HEAD) and write each to the browser that
 *      owns it.
 *
 * If the ESP drops, the queue is kept and auto-flushed on reconnect.
 *
 * Zero dependencies:  node tunnel_relay.js
 * Env: TUNNEL_PORT (9000) · HTTP_PORT (8280) · IDLE_MS (90000)
 */

'use strict';

const net = require('net');

const TUNNEL_PORT = parseInt(process.env.TUNNEL_PORT || '9000', 10);
const HTTP_PORT   = parseInt(process.env.HTTP_PORT   || '8280', 10);
const IDLE_MS     = parseInt(process.env.IDLE_MS     || '90000', 10);

const MAX_BROWSERS  = 32;                 /* cloud VM — plenty of RAM/sockets */
const MAX_REQ_BYTES = 8 * 1024 * 1024;   /* buffered request size cap      */
const MAX_QUEUE     = 128;               /* requests waiting for the ESP   */
const STALL_MS      = 15000;             /* head sent to ESP, no response — the ESP
                                          * may have silently eaten it; tear the
                                          * tunnel down so queued browsers fail fast
                                          * (502) and the ESP's reconnect gets the
                                          * next request a fresh chance          */

/* Static-asset cache: the ESP serves lib assets as immutable gzip blobs.
 * The tunnel is slow (~19KB/s), so once fetched, serve them locally.    */
const MAX_CACHE_BYTES = 32 * 1024 * 1024;
const TTL_CACHE_MS    = 15000;           /* short TTL for near-static GETs  */
const PREFETCH_PATHS = [
  '/lib/chartjs/chart.umd.min.js',
  '/lib/hammerjs/hammer.min.js',
  '/lib/chartjs-plugin-zoom/chartjs-plugin-zoom.min.js',
  '/lib/bootstrap/css/bootstrap.min.css',
];
const assetCache = new Map();           /* path -> {at, response}          */
let cacheBytes = 0, warmed = false, prefetchTimer = null;

const CRLF = '\r\n';
const log  = (...a) => console.log(new Date().toISOString(), ...a);

/* ---------------------------- HTTP framing ---------------------------- */

/** Index just past "CRLFCRLF" (end of head) or -1 if incomplete. */
function findHeadEnd(buf) {
  for (let i = 0; i + 3 < buf.length; i++) {
    if (buf[i] === 0x0d && buf[i + 1] === 0x0a &&
        buf[i + 2] === 0x0d && buf[i + 3] === 0x0a) return i + 4;
  }
  return -1;
}

/** Parse head (first line + lowercase-name header map). */
function parseHead(buf) {
  const lines = buf.toString('latin1').split(CRLF);
  const h = { first: lines[0] || '' };
  for (let i = 1; i < lines.length; i++) {
    const m = /^([^:\s]+):\s*(.*)$/.exec(lines[i]);
    if (m) h[m[1].toLowerCase()] = m[2].trim();
  }
  return h;
}

/** Index past the final "0\r\n\r\n" of a chunked body, or -1. */
function chunkedBodyEnd(buf, start) {
  let pos = start;
  for (;;) {
    const lineEnd = buf.indexOf(CRLF, pos);
    if (lineEnd === -1) return -1;
    const sizeStr = buf.subarray(pos, lineEnd).toString('latin1').split(';')[0].trim();
    const size = parseInt(sizeStr, 16);
    if (!Number.isFinite(size)) return -1;
    if (size === 0) {
      const tail = lineEnd + 2;
      return (buf.length >= tail + 2 && buf[tail] === 0x0d && buf[tail + 1] === 0x0a)
        ? tail + 2 : -1;
    }
    pos = lineEnd + 2 + size + 2;
    if (pos > buf.length) return -1;
  }
}

/**
 * Is an HTTP request complete? (request line + headers + Content-Length body)
 */
function isRequestComplete(buf) {
  const headEnd = findHeadEnd(buf);
  if (headEnd === -1) return false;
  const cl = parseInt(parseHead(buf.subarray(0, headEnd))['content-length'] || '0', 10);
  return buf.length >= headEnd + (Number.isFinite(cl) ? cl : 0);
}

/**
 * Carve ONE complete HTTP response out of the ESP byte stream.
 * Returns { consumed, status, body } or null while incomplete.
 */
function tryParseResponse(buf, isHead) {
  const headEnd = findHeadEnd(buf);
  if (headEnd === -1) return null;
  const head = buf.subarray(0, headEnd).toString('latin1');
  const statusMatch = /^HTTP\/\d\.\d (\d{3})/.exec(head);
  if (!statusMatch) return null;
  const status = parseInt(statusMatch[1], 10);
  const headers = parseHead(buf.subarray(0, headEnd));

  let bodyEnd = -1;                                  /* index, or -1 = unknown */
  const isChunked = (headers['transfer-encoding'] || '').toLowerCase().includes('chunked');
  if (isChunked) {
    bodyEnd = chunkedBodyEnd(buf, headEnd);
  } else if (headers['content-length'] !== undefined) {
    const cl = parseInt(headers['content-length'], 10);
    bodyEnd = Number.isFinite(cl) ? headEnd + cl : headEnd;
  } else if (status < 200 || status === 204 || status === 304 || isHead) {
    bodyEnd = headEnd;                               /* bodyless                */
  }
  if (bodyEnd === -1) return null;
  if (buf.length < bodyEnd) return null;

  return { consumed: bodyEnd, status, body: buf.subarray(0, bodyEnd) };
}

const errPage = (code, why) => {
  const body = `<!DOCTYPE html><html><head><title>${code} ${why}</title></head>` +
    `<body style="font-family:monospace;margin:2rem"><h1>${code} ${why}</h1>` +
    `<p>motorESP reverse-tunnel relay: no ESP8266 tunnel is currently connected.` +
    ` Retry in a few seconds.</p></body></html>`;
  return `HTTP/1.1 ${code} ${why}${CRLF}` +
    `Content-Type: text/html; charset=utf-8${CRLF}` +
    `Content-Length: ${Buffer.byteLength(body)}${CRLF}` +
    `Connection: close${CRLF}${CRLF}${body}`;
};

/* ------------------------------ global state ------------------------------ */

let espSocket    = null;          /* single active ESP tunnel socket          */
let espBuf       = Buffer.alloc(0);   /* ESP response bytes being assembled  */
let reqQueue     = [];            /* FIFO {sock, buf, method, sent}           */
const browserBufs = new Map();    /* sock -> partial request bytes            */

function cachePut(path, response, ttl) {
  const entry = { at: Date.now(), ttl, response };
  if (cacheBytes + response.length > MAX_CACHE_BYTES) {
    for (const k of assetCache.keys()) {            /* evict oldest            */
      const ev = assetCache.get(k);
      assetCache.delete(k);
      cacheBytes -= ev.response.length;
      if (cacheBytes + response.length <= MAX_CACHE_BYTES) break;
    }
  }
  assetCache.set(path, entry);
  cacheBytes += response.length;
  log(`cached ${path} (${response.length} B${ttl ? ', ttl ' + ttl + 'ms' : ''})`);
}

function cacheGet(path) {
  const entry = assetCache.get(path);
  if (!entry) return null;
  if (entry.ttl && Date.now() - entry.at > entry.ttl) {
    assetCache.delete(path); cacheBytes -= entry.response.length;
    return null;
  }
  return entry.response;
}

/* Warm the asset cache while the tunnel is idle (one asset per quiet period). */
function armPrefetch() {
  if (warmed) return;
  clearTimeout(prefetchTimer);
  prefetchTimer = setTimeout(() => {
    if (!espSocket || reqQueue.length > 0 || browserBufs.size > 0) { armPrefetch(); return; }
    const miss = PREFETCH_PATHS.find((p) => !assetCache.has(p) && !reqQueue.some((r) => r.path === p));
    if (!miss) { warmed = true; return; }
    const req = `GET ${miss} HTTP/1.1\r\nHost: relay-warmup\r\nConnection: close\r\n\r\n`;
    reqQueue.push({ sock: { writable: false, remotePort: 'warmup' },
                    buf: Buffer.from(req), method: 'GET', sent: false, prefetch: true,
                    path: miss, cacheable: true });
    log(`prefetch ${miss}`);
    flushToEsp();
  }, 3000);
}

/* ------------------------------ ESP side (A) ------------------------------ */

/* ESP tunnel data: assemble responses and route complete ones FIFO. */
function espData(chunk) {
  espBuf = Buffer.concat([espBuf, chunk]);

  while (reqQueue.length > 0) {
    const head = reqQueue[0];
    const parsed = tryParseResponse(espBuf, head.method === 'HEAD');
    if (!parsed) break;                       /* response still incomplete    */
    espBuf = espBuf.subarray(parsed.consumed);
    reqQueue.shift();
    if (head.cacheable && parsed.status === 200) {
      cachePut(head.path, Buffer.from(parsed.body), head.cacheTtlMs || 0);
    }
    if (head.sock.writable) {
      head.sock.write(parsed.body);
      log(`resp ${parsed.status} (${parsed.body.length} B) -> browser ${head.sock.remotePort}`);
    } else {
      log(`prefetch done ${head.path} (${parsed.body.length} B)`);
    }
  }

  if (reqQueue.length === 0 && espBuf.length > 0) {
    log(`discarded ${espBuf.length} unsolicited esp byte(s) (no queued request)`);
    espBuf = Buffer.alloc(0);
  }

  /* The head request was answered — flush the NEXT queued request to the
   * ESP immediately. Without this, the next request only gets forwarded
   * when ANOTHER browser request arrives (25s+ stalls on curl sequences). */
  if (reqQueue.length > 0) flushToEsp();
  if (reqQueue.length === 0) armPrefetch();
}

const tunnelServer = net.createServer((sock) => {
  if (espSocket && espSocket !== sock) {
    log(`replacing stale esp connection from ${espSocket.remoteAddress}:${espSocket.remotePort}`);
    espSocket.destroy();
  }
  espSocket = sock;
  espBuf = Buffer.alloc(0);            /* clean slate for the new tunnel */
  sock.setNoDelay(true);
  sock.on('data', espData);
  sock.on('error', () => {});
  sock.on('close', () => {
     if (espSocket === sock) {
       espSocket = null;
       espBuf = Buffer.alloc(0);       /* drop partial bytes from the dead socket */
       warmed = false;
      /* Every queued request was already flushed to the dead ESP and will
       * never be answered: fail them so browsers don't hang.             */
      const lost = reqQueue.length;
      for (const r of reqQueue) {
        if (r.sock.writable) r.sock.end(errPage(502, 'Bad Gateway'));
      }
      reqQueue = [];
      log(`esp disconnected — ${lost} request(s) failed with 502`);
    }
  });
  armPrefetch();
});

/* ----------------------------- browser side (B) ---------------------------- */

function flushToEsp() {
  /* Mark head request(s) as written to the ESP. Entries stay in the queue
   * until their matching response is parsed (see espData) — the queue IS
   * the response-routing table.                                         */
  while (espSocket && espSocket.writable && reqQueue.length > 0 && !reqQueue[0].sent) {
    reqQueue[0].sent = true;
    reqQueue[0].sentAt = Date.now();
    espSocket.write(reqQueue[0].buf);
  }
}

/* Stall watchdog: if the head request was flushed to the ESP and stayed
 * unanswered for STALL_MS, the ESP probably ate it (intermittent silent
 * phases) — destroy the tunnel socket. The close handler 502s the whole
 * queue (fast failure for browsers instead of a hang) and the ESP
 * re-dials on its own 3-30s backoff with a fresh chance.                */
setInterval(() => {
  if (!espSocket || reqQueue.length === 0) return;
  const head = reqQueue[0];
  if (head.noStall) return;                 /* OTA: silent-by-design         */
  if (head.sent && head.sentAt && Date.now() - head.sentAt >= STALL_MS) {
    log(`esp silent — head ${head.method} ${head.path || '?'} unanswered ${STALL_MS}ms — forcing reconnect`);
    espSocket.destroy();
  }
}, 5000);

const httpServer = net.createServer((sock) => {
  if (browserBufs.size >= MAX_BROWSERS) { sock.end(errPage(503, 'Service Unavailable')); return; }

  browserBufs.set(sock, Buffer.alloc(0));
  sock.setNoDelay(true);

  const armIdle = () => {
    clearTimeout(sock._idle);
    sock._idle = setTimeout(() => { log(`browser ${sock.remotePort} idle — closing`); sock.destroy(); }, IDLE_MS);
  };
  armIdle();

  sock.on('data', (chunk) => {
    armIdle();
    let buf = Buffer.concat([browserBufs.get(sock) || Buffer.alloc(0), chunk]);
    if (buf.length > MAX_REQ_BYTES) { sock.end(errPage(413, 'Payload Too Large')); return; }

    if (!isRequestComplete(buf)) { browserBufs.set(sock, buf); return; }

    browserBufs.delete(sock);                    /* whole request received     */
    if (!espSocket) { sock.end(errPage(502, 'Bad Gateway')); return; }
    if (reqQueue.length >= MAX_QUEUE) { sock.end(errPage(503, 'Service Unavailable')); return; }

    const head = parseHead(buf);
    const method = (head.first.split(/\s+/)[0] || 'GET').toUpperCase();
    const path   = head.first.split(/\s+/)[1] || '/';

    const cached = cacheGet(path);
    if (cached) {
      sock.write(cached);                    /* cached — serve locally */
      log(`served ${path} from cache (${cached.length} B)`);
      return;
    }
    if (!espSocket) { sock.end(errPage(502, 'Bad Gateway')); return; }
    if (reqQueue.length >= MAX_QUEUE) { sock.end(errPage(503, 'Service Unavailable')); return; }

    const cacheable = method === 'GET' && (/^\/lib\//.test(path) || path === '/data/boots');
    /* OTA uploads legitimately take 30-60s (slow tunnel + flash write) while
     * the ESP is silent — exempt them from the stall watchdog so the socket
     * isn't torn down mid-update. A real ESP death still fires close/error. */
    const noStall = method === 'POST' && path === '/update';
    reqQueue.push({ sock, buf, method, sent: false,
                    path, cacheable, noStall,
                    cacheTtlMs: cacheable && !/^\/lib\//.test(path) ? TTL_CACHE_MS : 0 });
    log(`req ${method} ${path} -> esp (queue=${reqQueue.length})`);

    flushToEsp();
    armPrefetch();
  });

  sock.on('error', () => {});
  sock.on('close', () => {
    browserBufs.delete(sock);
    clearTimeout(sock._idle);
    reqQueue = reqQueue.filter((r) => r.sock !== sock);
  });
});

/* ---------------------------------- boot ---------------------------------- */

tunnelServer.listen(TUNNEL_PORT, '0.0.0.0', () =>
  log(`tunnel relay up: ESP tunnel 0.0.0.0:${TUNNEL_PORT}, public HTTP 0.0.0.0:${HTTP_PORT}`));
httpServer.listen(HTTP_PORT, '0.0.0.0', () =>
  log(`public HTTP listening on 0.0.0.0:${HTTP_PORT}`));