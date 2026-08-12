#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <Arduino.h>

#define DEFAULT_WIFI_SSID "Sweet Home"
#define DEFAULT_WIFI_PASSWORD "dishoom1234"
#define AP_SSID "motorESP"

// Runtime-writable WiFi credentials (initialized to defaults, can be overridden from EEPROM)
char wifiSsid[33] = DEFAULT_WIFI_SSID;
char wifiPassword[65] = DEFAULT_WIFI_PASSWORD;

DNSServer dnsServer;

enum WifiState { WF_TRY_SAVED, WF_TRY_DEFAULT, WF_CONNECTED, WF_RECONNECTING, WF_AP };
WifiState wifiState = WF_TRY_SAVED;
unsigned long wifiStateTime = 0;

// Non-blocking: starts WiFi async, no delay loops
void initWiFi() {
  WiFi.hostname("motorESP");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);
  WiFi.setAutoReconnect(true);
  wifiState = WF_TRY_SAVED;
  wifiStateTime = millis();
}

// Call every loop() — drives state machine
void handleWiFi() {
  switch (wifiState) {
    case WF_TRY_SAVED:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WF_CONNECTED;
      } else if (millis() - wifiStateTime > 10000) {
        // Saved creds timed out, try compile-time defaults
        WiFi.mode(WIFI_STA);
        WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
        wifiState = WF_TRY_DEFAULT;
        wifiStateTime = millis();
      }
      break;

    case WF_TRY_DEFAULT:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WF_CONNECTED;
      } else if (millis() - wifiStateTime > 10000) {
        // Everything failed, fall back to AP mode
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID);
        dnsServer.start(53, "*", WiFi.softAPIP());
        wifiState = WF_AP;
        wifiStateTime = millis();
      }
      break;

    case WF_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        // Connection lost — auto-reconnect is already enabled, give it time
        wifiState = WF_RECONNECTING;
        wifiStateTime = millis();
      }
      break;

    case WF_RECONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WF_CONNECTED;
      } else if (millis() - wifiStateTime > 15000) {
        // Auto-reconnect timed out, retry from scratch with saved creds
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSsid, wifiPassword);
        wifiState = WF_TRY_SAVED;
        wifiStateTime = millis();
      }
      break;

    case WF_AP:
      dnsServer.processNextRequest();
      // Every 15s scan for known networks
      if (millis() - wifiStateTime >= 15000) {
        int n = WiFi.scanNetworks();
        bool savedFound = false;
        bool defaultFound = false;
        for (int i = 0; i < n; i++) {
          if (WiFi.SSID(i) == wifiSsid) savedFound = true;
          if (WiFi.SSID(i) == DEFAULT_WIFI_SSID) defaultFound = true;
        }
        WiFi.scanDelete();

        if (savedFound) {
          dnsServer.stop();
          WiFi.mode(WIFI_STA);
          WiFi.begin(wifiSsid, wifiPassword);
          wifiState = WF_TRY_SAVED;
          wifiStateTime = millis();
        } else if (defaultFound) {
          dnsServer.stop();
          WiFi.mode(WIFI_STA);
          WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
          wifiState = WF_TRY_DEFAULT;
          wifiStateTime = millis();
        } else {
          wifiStateTime = millis(); // reset timer for next scan
        }
      }
      break;
  }
}

bool isWiFiConnected() {
  return wifiState == WF_CONNECTED;
}

// Legacy compat — DNS processing is handled inside handleWiFi() now
void processDNS() {
  if (wifiState == WF_AP) dnsServer.processNextRequest();
}

#endif
