// ============================================
// tunnel_client.h — Reverse TCP tunnel (CGNAT traversal)
// ============================================
// The ESP dials OUT to the relay on the VM (TUNNEL_HOST:TUNNEL_PORT).
// TunnelWebServer subclasses ESP8266WebServer: when the web server is idle
// (HC_NONE), the persistent tunnel WiFiClient is injected as the current
// client, so ALL existing routes/handlers/OTA work over the tunnel with
// zero changes. LAN access keeps working — LAN clients get priority because
// server.handleClient() runs first.
//
// No branding/framing on the wire: raw HTTP over plain TCP. CGNAT mapping
// is kept alive by lwIP TCP keepalives (never visible to the relay).

#ifndef TUNNEL_CLIENT_H
#define TUNNEL_CLIENT_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "config.h"

// Reconnect backoff bounds
#define TUNNEL_RECONNECT_MIN_MS 3000ul
#define TUNNEL_RECONNECT_MAX_MS 30000ul

// lwIP TCP keepalive: probe after 30s idle, every 10s, drop after 3 fails —
// keeps the CGNAT mapping alive without app-level frames.
#define TUNNEL_TCP_KEEPALIVE_IDLE_SEC 30
#define TUNNEL_TCP_KEEPALIVE_INTV_SEC 10
#define TUNNEL_TCP_KEEPALIVE_CNT      3

extern WiFiClient tunnelClient;
extern bool tunnelEnabled;

enum TunnelState {
  TUNNEL_DISABLED,      // TUNNEL_HOST empty/invalid in config.h
  TUNNEL_WAIT_WIFI,     // WiFi not up (STA) yet
  TUNNEL_CONNECTING,    // connect() in progress / failed, backing off
  TUNNEL_CONNECTED      // persistent tunnel socket open
};

extern TunnelState tunnelState;

inline const char* tunnelStateName() {
  switch (tunnelState) {
    case TUNNEL_DISABLED:   return "disabled";
    case TUNNEL_WAIT_WIFI:  return "wait_wifi";
    case TUNNEL_CONNECTING: return "connecting";
    case TUNNEL_CONNECTED:  return "connected";
  }
  return "unknown";
}

// Diagnostics (file-scope statics visible to motorESP.ino's handleStatus)
static uint32_t sTunnelAvailHits = 0;   // times tunnel bytes were observed
static uint32_t sTunnelServed    = 0;   // complete responses served via tunnel

// ------------------------------------------------------------
// Web server subclass: serves the tunnel socket when idle.
// ------------------------------------------------------------
class TunnelWebServer : public ESP8266WebServer {
public:
  using ESP8266WebServer::ESP8266WebServer;

int  currentStatus()       { return (int)_currentStatus; }
  bool lanHasClientData()    { return _server.hasClientData(); }
  bool lanHasMaxPending()    { return _server.hasMaxPendingClients(); }

  // MUST be called AFTER handleClient() every loop: LAN clients win races
  // at HC_NONE; the tunnel is only injected while the server is idle.
  void handleTunnelClient() {
    // Nothing to do unless the tunnel has real bytes waiting.
    if (!tunnelClient.connected() || tunnelClient.available() == 0) return;

    sTunnelAvailHits++;

    if (_currentStatus == HC_NONE) {
      // Server idle -> inject the tunnel as the current client and let the
      // stock state machine parse/serve the request (all handlers work
      // unchanged, OTA included).
      _currentClient = tunnelClient;
      _currentClient.setTimeout(3000);   // parse wait bound while tunnelled
      _currentStatus = HC_WAIT_READ;
      _statusChange = millis();
      ESP8266WebServer::handleClient();
      if (_currentStatus == HC_WAIT_CLOSE) sTunnelServed++;
      return;
    }

    // A LAN client holds the server (e.g. the cloudflared daemon keeps a
    // persistent connection parked; a polling control page re-arms the 5s
    // idle timer every 3s and would starve the tunnel forever). The tunnel
    // socket is NOT in the WiFiServer listen queue, so the stock code can
    // never see its bytes. Expire the parked client's wait NOW: the stock
    // handleClient() performs its own safe teardown (no direct stop()).
    _statusChange = millis() - HTTP_MAX_DATA_WAIT - 1001;
    ESP8266WebServer::handleClient();
  }
};

// ------------------------------------------------------------
// Tunnel connection manager (non-blocking, called from loop())
// ------------------------------------------------------------
inline void tunnelSetup() {
  static bool done = false;
  if (done) return;
  done = true;

  IPAddress ip;
  tunnelEnabled = (strlen(TUNNEL_HOST) > 0) && ip.fromString(TUNNEL_HOST);
  if (!tunnelEnabled) {
    tunnelState = TUNNEL_DISABLED;
    Serial.println(F("[tunnel] disabled — TUNNEL_HOST empty/invalid in config.h"));
  } else {
    tunnelState = TUNNEL_WAIT_WIFI;
    Serial.printf_P(PSTR("[tunnel] enabled -> %s:%u\n"), TUNNEL_HOST, (unsigned)TUNNEL_PORT);
  }
}

inline void handleTunnelManager() {
  static bool configured = false;
  static IPAddress host;
  static unsigned long backoff = TUNNEL_RECONNECT_MIN_MS;
  static unsigned long nextTry = 0;

  if (!configured) {
    configured = true;
    host.fromString(TUNNEL_HOST);
    if (!tunnelEnabled) return;
  }
  if (!tunnelEnabled) return;

  bool wifiUp = (WiFi.getMode() & WIFI_STA) && WiFi.status() == WL_CONNECTED;
  if (!wifiUp) {
    if (tunnelClient.connected()) tunnelClient.stop();
    if (tunnelState != TUNNEL_WAIT_WIFI) {
      tunnelState = TUNNEL_WAIT_WIFI;
      nextTry = 0;
    }
    return;
  }

  if (tunnelClient.connected()) {
    if (tunnelState != TUNNEL_CONNECTED) tunnelState = TUNNEL_CONNECTED;
    nextTry = 0;
    return;
  }

  unsigned long now = millis();
  if (now < nextTry) return;
  nextTry = now;

  tunnelState = TUNNEL_CONNECTING;
  bool ok = tunnelClient.connect(host, TUNNEL_PORT);
  if (ok) {
    tunnelClient.setNoDelay(true);
    tunnelClient.keepAlive(TUNNEL_TCP_KEEPALIVE_IDLE_SEC,
                           TUNNEL_TCP_KEEPALIVE_INTV_SEC,
                           TUNNEL_TCP_KEEPALIVE_CNT);
    tunnelClient.setTimeout(3000);
    backoff = TUNNEL_RECONNECT_MIN_MS;
    tunnelState = TUNNEL_CONNECTED;
    Serial.printf_P(PSTR("[tunnel] connected to %s:%u\n"), TUNNEL_HOST, (unsigned)TUNNEL_PORT);
  } else {
    tunnelClient.stop();
    backoff = (backoff * 2 > TUNNEL_RECONNECT_MAX_MS) ? TUNNEL_RECONNECT_MAX_MS : backoff * 2;
    tunnelState = TUNNEL_CONNECTING;
    Serial.printf_P(PSTR("[tunnel] connect failed — retry in %lus\n"), backoff / 1000ul);
  }
  nextTry = now + backoff;
}

#endif