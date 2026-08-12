// ============================================
// EGG INCUBATOR CONTROLLER - Modular Version
// ============================================

// Include header files
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>

#include "config.h"
#include "dht_sensor.h"
#include "wifi_manager.h"
#include "logging.h"

#include "updates.h"
#include "web_ui.h"
#include "sector_viewer.h"
#include "embedded_assets.h"
#include "sat_manager.h"
#include "ntp_sync.h"

extern bool useMockSensor;
extern bool autoSimMode;
extern float mockTemp;
extern float mockHum;
extern void updateAutoSim(bool heater, bool atomizer, bool fan);


#define KILL_OFF 0
#define AUTO 1

// Configurable timing (can be changed via web)
unsigned long LOG_INTERVAL = 90000;
unsigned long EGG_TURN_INTERVAL = 7200000;
unsigned long EGG_TURN_DURATION = 2000; // ms per step (each step = 6°)
unsigned long PULSE_ON_TIME = 3000;
unsigned long PULSE_OFF_TIME = 10000;

// Target temperature and humidity (can be changed via web/stage selection)
float TARGET_TEMP = 37.5;    // Default 37.5°C
float TARGET_HUMIDITY = 55.0; // Default 55.0%

// Global variables
float currentTemp = 0;
float currentHumidity = 0;
bool heaterState = false;
bool atomizerState = false;
bool fanState = false;
bool servoEnabled = true;
int servoPosition = 0; // Current servo step (0-31, each step = 6°)
int heaterMode = AUTO;
bool stageLockdown = false;  // false = incubation (1-18), true = lockdown (19-21)
int atomizerMode = AUTO;
int fanMode = AUTO;
int servoMode = AUTO;
unsigned long lastReadTime = 0;
unsigned long lastServoTurn = 0;
  uint8_t currentServoStep = 15; // Step 15 = 90° (center position)
bool sweeping = false;
bool isMovingTowardsMax = true;
uint8_t sweepTargetStep = 22; // Target step during sweep
unsigned long lastStepTime = 0;
bool movingInStep = false;
int stepStartAngle = 0;
int stepTargetAngle = 0;
unsigned long stepMoveStart = 0;
int8_t angleAdjustMin = 0;
int8_t angleAdjustMax = 0;

Servo myServo;

uint32_t batchStartUnix = 0;

// Control state variables
unsigned long atomizerPulseStart = 0;
bool atomizerPulsing = false;
bool atomizerInOffPhase = false;
unsigned long atomizerOffStart = 0;
unsigned long heaterLastChanged = 0;
bool heaterWasOn = false;
unsigned long atomizerLastChanged = 0;
bool atomizerWasOn = false;
bool pendingHeaterOn = false;
bool pendingAtomizerOn = false;
unsigned long fanPreRunStart = 0;
#define FAN_PRE_RUN_TIME 1500

// Web server
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;


// EEPROM addresses for settings
#define EEPROM_SETTINGS_MAGIC 40
#define SETTINGS_MAGIC_VAL 0xAB

struct DeviceSettings {
  uint8_t magic;
  bool stageLockdown;
  unsigned long logInterval;
  unsigned long turnInterval;
  unsigned long pulseOnTime;
  unsigned long pulseOffTime;
  uint32_t batchStartUnix;
  uint8_t turnDurationDs;
  int8_t angleAdjustMin;
  int8_t angleAdjustMax;
};

// EEPROM address for WiFi credentials (separate from DeviceSettings)
#define EEPROM_WIFI_ADDR 200
#define WIFI_MAGIC_VAL 0xAC

struct WifiSettings {
  uint8_t magic;
  char ssid[33];
  char password[65];
};

// ============================================
// AUTO CONTROL LOGIC
// ============================================

void getServoEndpoints(uint8_t &minStep, uint8_t &maxStep) {
  minStep = constrain(7 + (angleAdjustMin / 6), 0, 31);
  maxStep = constrain(22 + (angleAdjustMax / 6), 0, 31);
}

void saveSettings() {
  DeviceSettings settings;
  EEPROM.get(EEPROM_SETTINGS_MAGIC, settings);
  
  bool changed = false;
  if (settings.magic != SETTINGS_MAGIC_VAL) { settings.magic = SETTINGS_MAGIC_VAL; changed = true; }
  if (settings.stageLockdown != stageLockdown) { settings.stageLockdown = stageLockdown; changed = true; }
  if (settings.logInterval != LOG_INTERVAL) { settings.logInterval = LOG_INTERVAL; changed = true; }
  if (EGG_TURN_INTERVAL != 20000 && settings.turnInterval != EGG_TURN_INTERVAL) { settings.turnInterval = EGG_TURN_INTERVAL; changed = true; }
  if (settings.turnDurationDs != (EGG_TURN_DURATION / 100)) { settings.turnDurationDs = (EGG_TURN_DURATION / 100); changed = true; }
  if (settings.angleAdjustMin != angleAdjustMin) { settings.angleAdjustMin = angleAdjustMin; changed = true; }
  if (settings.angleAdjustMax != angleAdjustMax) { settings.angleAdjustMax = angleAdjustMax; changed = true; }
  if (settings.pulseOnTime != PULSE_ON_TIME) { settings.pulseOnTime = PULSE_ON_TIME; changed = true; }
  if (settings.pulseOffTime != PULSE_OFF_TIME) { settings.pulseOffTime = PULSE_OFF_TIME; changed = true; }
  if (settings.batchStartUnix != batchStartUnix) { settings.batchStartUnix = batchStartUnix; changed = true; }
  
  if (changed) {
    EEPROM.put(EEPROM_SETTINGS_MAGIC, settings);
    EEPROM.commit();
  }
}

void loadSettings() {
  DeviceSettings settings;
  EEPROM.get(EEPROM_SETTINGS_MAGIC, settings);
  
  if (settings.magic == SETTINGS_MAGIC_VAL) {
    stageLockdown = settings.stageLockdown;
    LOG_INTERVAL = settings.logInterval;
    EGG_TURN_INTERVAL = (settings.turnInterval >= 3600000) ? settings.turnInterval : 7200000;
    EGG_TURN_DURATION = settings.turnDurationDs > 0 && settings.turnDurationDs <= 40 ? settings.turnDurationDs * 100 : 2000;
    angleAdjustMin = settings.angleAdjustMin;
    angleAdjustMax = settings.angleAdjustMax;
    batchStartUnix = settings.batchStartUnix;
    
    if (settings.pulseOnTime == 2000 || settings.pulseOnTime == 3000 || settings.pulseOnTime == 4000 || settings.pulseOnTime == 5000) {
      PULSE_ON_TIME = settings.pulseOnTime;
    } else {
      PULSE_ON_TIME = 3000;
    }
    if (settings.pulseOffTime >= 5000 && settings.pulseOffTime <= 30000 && settings.pulseOffTime % 5000 == 0) {
      PULSE_OFF_TIME = settings.pulseOffTime;
    } else {
      PULSE_OFF_TIME = 10000;
    }
    
    stageLockdown = (getCurrentDay() >= 17); // 0-indexed Day 18 is index 17

    if (stageLockdown) {
      TARGET_TEMP = 37.5;
      TARGET_HUMIDITY = 65.0;
      servoEnabled = false;
    } else {
      TARGET_TEMP = 37.5;
      TARGET_HUMIDITY = 55.0;
      servoEnabled = true;
    }
  } else {
    batchStartUnix = 0;
    saveSettings();
  }
}

void loadWifiCredentials() {
  WifiSettings settings;
  EEPROM.get(EEPROM_WIFI_ADDR, settings);
  if (settings.magic == WIFI_MAGIC_VAL && settings.ssid[0] != '\0') {
    strncpy(wifiSsid, settings.ssid, 32);
    wifiSsid[32] = '\0';
    strncpy(wifiPassword, settings.password, 64);
    wifiPassword[64] = '\0';
  }
  // else: keep defaults (already set in wifi_manager.h globals)
}

void saveWifiCredentials(const char* ssid, const char* password) {
  WifiSettings settings;
  settings.magic = WIFI_MAGIC_VAL;
  strncpy(settings.ssid, ssid, 32);
  settings.ssid[32] = '\0';
  strncpy(settings.password, password, 64);
  settings.password[64] = '\0';
  EEPROM.put(EEPROM_WIFI_ADDR, settings);
  EEPROM.commit();
}

// ============================================
// WEB SERVER HANDLERS
// ============================================
void handleSectorViewerPage() {
  server.send(200, "text/html; charset=utf-8", SECTOR_VIEWER_HTML);
}

void handleSectorHex() {
  if (server.method() == HTTP_POST) {
    // Write sector
    if (!server.hasArg("sector")) {
      server.send(400, "text/plain", "Missing sector parameter");
      return;
    }
    int sector = server.arg("sector").toInt();
    if (sector < 0 || sector >= FLASH_NUM_SECTORS) {
      server.send(400, "text/plain", "Invalid sector");
      return;
    }
    if (!server.hasArg("plain")) {
      server.send(400, "text/plain", "Missing body");
      return;
    }
    String hexData = server.arg("plain");
    int byteCount = hexData.length() / 2;
    if (byteCount > FLASH_SECTOR_SIZE) byteCount = FLASH_SECTOR_SIZE;

    uint8_t* buffer = (uint8_t*)malloc(FLASH_SECTOR_SIZE);
    if (!buffer) {
      server.send(500, "text/plain", "Out of memory");
      return;
    }
    // Fill with 0xFF first (erased flash state)
    memset(buffer, 0xFF, FLASH_SECTOR_SIZE);
    // Decode hex string into buffer
    for (int i = 0; i < byteCount; i++) {
      char hi = hexData[i * 2];
      char lo = hexData[i * 2 + 1];
      uint8_t b = 0;
      if (hi >= '0' && hi <= '9') b = (hi - '0') << 4;
      else if (hi >= 'a' && hi <= 'f') b = (hi - 'a' + 10) << 4;
      else if (hi >= 'A' && hi <= 'F') b = (hi - 'A' + 10) << 4;
      if (lo >= '0' && lo <= '9') b |= (lo - '0');
      else if (lo >= 'a' && lo <= 'f') b |= (lo - 'a' + 10);
      else if (lo >= 'A' && lo <= 'F') b |= (lo - 'A' + 10);
      buffer[i] = b;
    }
    // Erase sector then write
    uint32_t flashAddr = FLASH_LOG_START + (sector * FLASH_SECTOR_SIZE);
    uint32_t sectorId = flashAddr / FLASH_SECTOR_SIZE;
    ESP.flashEraseSector(sectorId);
    ESP.flashWrite(flashAddr, (uint32_t*)buffer, FLASH_SECTOR_SIZE);
    free(buffer);
    server.send(200, "text/plain", "OK");
    return;
  }

  // GET - read sector
  if (!server.hasArg("sector")) {
    server.send(400, "text/plain", "Missing sector parameter");
    return;
  }
  int sector = server.arg("sector").toInt();
  if (sector < 0 || sector >= FLASH_NUM_SECTORS) {
    server.send(400, "text/plain", "Invalid sector");
    return;
  }
  uint8_t* buffer = (uint8_t*)malloc(FLASH_SECTOR_SIZE);
  if (!buffer) {
    server.send(500, "text/plain", "Out of memory");
    return;
  }
  ESP.flashRead(FLASH_LOG_START + (sector * FLASH_SECTOR_SIZE), (uint32_t*)buffer, FLASH_SECTOR_SIZE);
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/octet-stream", (const char*)buffer, FLASH_SECTOR_SIZE);
  free(buffer);
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleSettingsPage() {
  server.send(200, "text/html; charset=utf-8", SETTINGS_HTML);
}

void handleDexiePage() {
  server.send(200, "text/html; charset=utf-8", DEXIE_HTML);
}

// Serve embedded gzip'd CDN assets from PROGMEM (no internet needed)
void handleLibAsset() {
  String path = server.uri();
  for (size_t i = 0; i < EMBEDDED_ASSETS_COUNT; i++) {
    if (path == EMBEDDED_ASSETS[i].path) {
      server.sendHeader("Content-Encoding", "gzip");
      server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
      server.send_P(200, EMBEDDED_ASSETS[i].mime_type,
                    (const char*)EMBEDDED_ASSETS[i].data,
                    EMBEDDED_ASSETS[i].len);
      return;
    }
  }
  server.send(404, "text/plain", "Not found");
}

void handleStatus() {
  unsigned long uptimeSec = millis() / 1000;
  int days = uptimeSec / 86400;
  int hours = (uptimeSec % 86400) / 3600;
  int mins = (uptimeSec % 3600) / 60;
  int secs = uptimeSec % 60;
  String uptimeStr = "";
  if (days > 0) uptimeStr += String(days) + "d ";
  uptimeStr += String(hours) + "h " + String(mins) + "m " + String(secs) + "s";
  
  String json = "{\"temperature\":" + String(currentTemp) +
                ",\"humidity\":" + String(currentHumidity) +
                ",\"heater\":" + String(heaterState ? 1 : 0) +
                ",\"atomizer\":" + String(atomizerState ? 1 : 0) +
                ",\"fan\":" + String(fanState ? 1 : 0) +
                ",\"servo\":" + String(servoPosition) +
                ",\"version\":\"" + FIRMWARE_VERSION + "\"" +
                ",\"uptime\":\"" + uptimeStr + "\"" +
                ",\"mock\":" + String(useMockSensor ? 1 : 0) +
                ",\"autosim\":" + String(autoSimMode ? 1 : 0) +
                ",\"stageLockdown\":" + String(stageLockdown ? 1 : 0) +
                ",\"targetTemp\":" + String(TARGET_TEMP) +
                ",\"targetHum\":" + String(TARGET_HUMIDITY) +
                ",\"heapFree\":" + String(ESP.getFreeHeap()) +
                ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
                ",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"uptimeSec\":" + String(uptimeSec) +
                ",\"bootId\":" + String(currentBootId) +
                ",\"currentSector\":" + String(currentSector) +
                ",\"startSector\":" + String(startSector) +
                ",\"batchStartUnix\":" + String(batchStartUnix) +
                ",\"elapsedSeconds\":" + String(getElapsedSeconds()) +
                ",\"currentDay\":" + String(getCurrentDay()) +
                ",\"logsInCurrentBoot\":" + String(logsInCurrentBoot) + "}";

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", json);
}

void handleData() {
  unsigned long uptimeSec = millis() / 1000;
  int days = uptimeSec / 86400;
  int hours = (uptimeSec % 86400) / 3600;
  int mins = (uptimeSec % 3600) / 60;
  int secs = uptimeSec % 60;
  String uptimeStr = "";
  if (days > 0) uptimeStr += String(days) + "d ";
  uptimeStr += String(hours) + "h " + String(mins) + "m " + String(secs) + "s";

  String json = "{\"temperature\":" + String(currentTemp) +
                ",\"humidity\":" + String(currentHumidity) +
                ",\"heater\":" + String(heaterState ? 1 : 0) +
                ",\"atomizer\":" + String(atomizerState ? 1 : 0) +
                ",\"fan\":" + String(fanState ? 1 : 0) +
                ",\"servo\":" + String(servoPosition) +
                ",\"version\":\"" + FIRMWARE_VERSION + "\"" +
                ",\"uptime\":\"" + uptimeStr + "\"" +
                ",\"mock\":" + String(useMockSensor ? 1 : 0) +
                ",\"autosim\":" + String(autoSimMode ? 1 : 0) +
                ",\"stageLockdown\":" + String(stageLockdown ? 1 : 0) +
                ",\"targetTemp\":" + String(TARGET_TEMP) +
                ",\"targetHum\":" + String(TARGET_HUMIDITY) +
                ",\"heapFree\":" + String(ESP.getFreeHeap()) +
                ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
                ",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"uptimeSec\":" + String(uptimeSec) +
                ",\"bootId\":" + String(currentBootId) +
                ",\"currentSector\":" + String(currentSector) +
                ",\"batchStartUnix\":" + String(batchStartUnix) +
                ",\"elapsedSeconds\":" + String(getElapsedSeconds()) +
                ",\"currentDay\":" + String(getCurrentDay()) +
                ",\"logsInCurrentBoot\":" + String(logsInCurrentBoot) +
                ",\"bootStartUnix\":" + String(batchStartUnix + getElapsedSeconds() - uptimeSec);

json += ",\"totalLogs\":" + String(getTotalLogs());

  // Parse pagination params
  uint8_t sinceBootId = 0;
  if (server.hasArg("boot")) {
    sinceBootId = (uint8_t)server.arg("boot").toInt();
  }
  uint32_t sinceTimeSec = 0;
  if (server.hasArg("time")) {
    sinceTimeSec = (uint32_t)server.arg("time").toInt();
  }
  
  int count = 200;
  if (server.hasArg("count")) {
    count = server.arg("count").toInt();
    if (count > 200) count = 200;
    if (count < 1) count = 1;
  }
  
  String logHex = "";
  int sentCount = getLogHex(logHex, count, sinceBootId, sinceTimeSec);
  
  json += ",\"sentCount\":" + String(sentCount) +
          ",\"logs\":\"" + logHex + "\"}";
  
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", json);
}

void handleControl() {
  if (server.hasArg("device") && server.hasArg("mode")) {
    String device = server.arg("device");
    String mode = server.arg("mode");
    bool isKillOff = (mode == "off");
    
    if (device == "heater") {
      heaterMode = isKillOff ? KILL_OFF : AUTO;
      if (isKillOff) {
        heaterState = false;
        pendingHeaterOn = false;
        digitalWrite(RELAY_HEATER, HIGH);
      }
    } else if (device == "atomizer") {
      atomizerMode = isKillOff ? KILL_OFF : AUTO;
      if (isKillOff) {
        atomizerState = false;
        pendingAtomizerOn = false;
        digitalWrite(RELAY_ATOMIZER, HIGH);
        atomizerPulsing = false;
        atomizerInOffPhase = false;
      }
    } else if (device == "fan") {
      fanMode = isKillOff ? KILL_OFF : AUTO;
      if (isKillOff) {
        fanState = false;
        digitalWrite(RELAY_FAN, HIGH);
      }
    } else if (device == "servo") {
      if (mode == "left") {
        servoEnabled = true;
        currentServoStep = constrain(7 + (angleAdjustMin / 6), 0, 31);
        int angle = constrain(currentServoStep * 6, 0, 180);
        int pulseWidth = map(angle, 0, 180, 544, 2450);
        myServo.attach(SERVO_PIN, 544, 2450, pulseWidth);
        myServo.write(angle);
        myServo.detach();
        sweeping = false;
        server.send(200, "text/plain", "Servo moved to left (" + String(angle) + "°)");
      } else if (mode == "right") {
        servoEnabled = true;
        currentServoStep = constrain(22 + (angleAdjustMax / 6), 0, 31);
        int angle = constrain(currentServoStep * 6, 0, 180);
        int pulseWidth = map(angle, 0, 180, 544, 2450);
        myServo.attach(SERVO_PIN, 544, 2450, pulseWidth);
        myServo.write(angle);
        myServo.detach();
        sweeping = false;
        server.send(200, "text/plain", "Servo moved to right (" + String(angle) + "°)");
      } else {
        servoMode = isKillOff ? KILL_OFF : AUTO;
        servoEnabled = !isKillOff;
        if (isKillOff) {
          servoPosition = 0;
          int angle = constrain(currentServoStep * 6, 0, 180);
          int pulseWidth = map(angle, 0, 180, 544, 2450);
          myServo.attach(SERVO_PIN, 544, 2450, pulseWidth);
          myServo.write(angle);
          myServo.detach();
          sweeping = false;
        }
        server.send(200, "text/plain", device + " mode set to " + (isKillOff ? "OFF" : "AUTO"));
      }
    }
  } else {
    server.send(200, "text/plain", "Invalid request");
  }
}

void handleOtaCheck() {
  String current = FIRMWARE_VERSION;
  String latest = "";
  
  // Parse current version once
  int cMaj = 0, cMin = 0, cPat = 0;
  int cFirstDot = current.indexOf('.');
  int cLastDot = current.lastIndexOf('.');
  if (cFirstDot > 0 && cLastDot > cFirstDot) {
    cMaj = current.substring(0, cFirstDot).toInt();
    cMin = current.substring(cFirstDot + 1, cLastDot).toInt();
    cPat = current.substring(cLastDot + 1).toInt();
  }
  
  // Single HTTPS call for version check
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (http.begin(client, VERSION_URL)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      
      int tagStart = payload.indexOf("\"tag_name\":\"");
      if (tagStart != -1) {
        tagStart += 12;
        int tagEnd = payload.indexOf('\"', tagStart);
        if (tagEnd != -1) {
          latest = payload.substring(tagStart, tagEnd);
        }
      }
    }
    http.end();
  }
  
  bool hasUpdate = false;
  if (latest.length() > 0) {
    String v = latest;
    if (v.startsWith("v")) v = v.substring(1);
    
    int vMaj = v.substring(0, v.indexOf('.')).toInt();
    int vMin = v.substring(v.indexOf('.') + 1, v.lastIndexOf('.')).toInt();
    int vPat = v.substring(v.lastIndexOf('.') + 1).toInt();
    
    if (vMaj > cMaj) hasUpdate = true;
    else if (vMaj == cMaj && vMin > cMin) hasUpdate = true;
    else if (vMaj == cMaj && vMin == cMin && vPat > cPat) hasUpdate = true;
  }
  
  String json = "{\"update\":" + String(hasUpdate ? "true" : "false") +
                ",\"currentVersion\":\"" + current +
                "\",\"latestVersion\":\"" + latest + "\"}";
  server.send(200, "application/json", json);
}

void handleOtaApply() {
  static bool updateInProgress = false;
  if (updateInProgress) {
    server.send(200, "application/json", "{\"status\":\"error\",\"message\":\"Update already in progress\"}");
    return;
  }
  
  updateInProgress = true;
  
  bool success = performUpdate();
  
  if (success) {
    // performUpdate() calls ESP.restart() — never reaches here
    return;
  }
  
  // Failure: reset flag so user can retry
  updateInProgress = false;
  
  String errorMsg = "Update failed";
  int lastErr = ESPhttpUpdate.getLastError();
  if (lastErr != 0) {
    errorMsg += ": " + ESPhttpUpdate.getLastErrorString();
  }
  
  String json = "{\"status\":\"error\",\"message\":\"" + errorMsg + "\"}";
  server.send(200, "application/json", json);
}

void handleSettingsApi() {
  if (server.hasArg("autosim")) {
    bool enable = (server.arg("autosim") == "1");
    setAutoSim(enable);
    server.send(200, "text/plain", enable ? "Auto simulation enabled" : "Auto simulation disabled");
  } else if (server.hasArg("enable")) {
    bool enable = (server.arg("enable") == "1");
    if (enable) setAutoSim(false);
    setMockSensor(enable);
    server.send(200, "text/plain", enable ? "Mock sensor enabled" : "Mock sensor disabled");
  } else if (server.hasArg("temp") && server.hasArg("hum")) {
    float t = server.arg("temp").toFloat();
    float h = server.arg("hum").toFloat();
    setAutoSim(false);
    setMockSensor(true);
    setMockValues(t, h);
    server.send(200, "text/plain", "Mock values set: " + String(t) + "C, " + String(h) + "%");
  } else if (server.hasArg("logInterval")) {
    unsigned long val = server.arg("logInterval").toInt();
    LOG_INTERVAL = val;
    saveSettings();
    server.send(200, "text/plain", "Log interval set to " + String(val/1000) + "s");
  } else if (server.hasArg("eggTurnInterval")) {
    unsigned long val = server.arg("eggTurnInterval").toInt();
    EGG_TURN_INTERVAL = val;
    // Only save if interval is >= 1 hour (3600000 ms)
    if (val >= 3600000) {
      saveSettings();
    }
    server.send(200, "text/plain", "Egg turner interval set to " + String(val/3600000) + " hours");
  } else if (server.hasArg("eggTurnDuration")) {
    float val = server.arg("eggTurnDuration").toFloat();
    if (val >= 0.5f && val <= 4.0f) {
      EGG_TURN_DURATION = (unsigned long)(val * 1000);
      saveSettings();
      server.send(200, "text/plain", "Egg sweep duration set to " + String(val, 1) + "s");
    } else {
      server.send(400, "text/plain", "Invalid duration (must be 0.5-4s)");
    }
  } else if (server.hasArg("angleAdjustMin")) {
    int8_t val = server.arg("angleAdjustMin").toInt();
    if (val >= -36 && val <= 36 && val % 6 == 0) {
      angleAdjustMin = val;
      saveSettings();
      server.send(200, "text/plain", "Min angle adjustment set to " + String(val));
    } else {
      server.send(400, "text/plain", "Invalid min adjustment (must be -36 to +36, multiple of 6)");
    }
  } else if (server.hasArg("angleAdjustMax")) {
    int8_t val = server.arg("angleAdjustMax").toInt();
    if (val >= -36 && val <= 36 && val % 6 == 0) {
      angleAdjustMax = val;
      saveSettings();
      server.send(200, "text/plain", "Max angle adjustment set to " + String(val));
    } else {
      server.send(400, "text/plain", "Invalid max adjustment (must be -36 to +36, multiple of 6)");
    }
  } else if (server.hasArg("pulseOnTime")) {
    unsigned long val = server.arg("pulseOnTime").toInt();
    PULSE_ON_TIME = val;
    saveSettings();
    server.send(200, "text/plain", "Atomizer pulse on time set to " + String(val/1000) + "s");
  } else if (server.hasArg("pulseOffTime")) {
    unsigned long val = server.arg("pulseOffTime").toInt();
    if (val >= 5000 && val <= 30000 && val % 5000 == 0) {
      PULSE_OFF_TIME = val;
      saveSettings();
      server.send(200, "text/plain", "Atomizer pulse off time set to " + String(val/1000) + "s");
    } else {
      server.send(400, "text/plain", "Invalid off time (must be 5-30s, multiple of 5)");
    }
  } else if (server.hasArg("wifiSsid")) {
    String ssid = server.arg("wifiSsid");
    String password = server.arg("wifiPassword");
    if (ssid.length() == 0) {
      // Clear credentials from EEPROM, reset to defaults
      WifiSettings clearSettings;
      clearSettings.magic = 0;
      EEPROM.put(EEPROM_WIFI_ADDR, clearSettings);
      EEPROM.commit();
      strncpy(wifiSsid, DEFAULT_WIFI_SSID, 32);
      wifiSsid[32] = '\0';
      strncpy(wifiPassword, DEFAULT_WIFI_PASSWORD, 64);
      wifiPassword[64] = '\0';
      server.send(200, "text/plain", "WiFi credentials cleared");
    } else if (ssid.length() > 32) {
      server.send(400, "text/plain", "SSID too long (max 32 chars)");
    } else if (password.length() > 64) {
      server.send(400, "text/plain", "Password too long (max 64 chars)");
    } else {
      strncpy(wifiSsid, ssid.c_str(), 32);
      wifiSsid[32] = '\0';
      strncpy(wifiPassword, password.c_str(), 64);
      wifiPassword[64] = '\0';
      saveWifiCredentials(wifiSsid, wifiPassword);
      server.send(200, "text/plain", "WiFi credentials saved");
    }
  } else if (server.hasArg("action")) {
    String action = server.arg("action");
    if (action == "newBatch" && server.hasArg("timestamp")) {
      batchStartUnix = (uint32_t)server.arg("timestamp").toInt();
      clearLogs();
      
      // Non-blocking move to 90 degrees (step 15)
      sweeping = true;
      sweepTargetStep = 15;
      
      saveSettings();
      server.send(200, "text/plain", "New batch started");
      delay(100);
      ESP.restart();
    } else if (action == "syncTime" && server.hasArg("timestamp")) {
      uint32_t currentUnix = (uint32_t)server.arg("timestamp").toInt();
      if (currentUnix > 0) {
        uint32_t bootUptime = getBootUptime();
        uint32_t browserCalculatedStartUnix = currentUnix - bootUptime;
        for (int i = 0; i < bootSessionCount; i++) {
          if (bootSessions[i].bootId == currentBootId) {
            int32_t drift = abs((int32_t)(browserCalculatedStartUnix - bootSessions[i].startUnix));
            if (drift > 5) {
              bootSessions[i].startUnix = browserCalculatedStartUnix;
              EEPROM.put(EEPROM_LAST_KNOWN_START_UNIX, browserCalculatedStartUnix);
              EEPROM.write(EEPROM_LAST_KNOWN_BOOT_ID, currentBootId);
              EEPROM.commit();
              server.send(200, "text/plain", "Time synced with drift=" + String(drift));
              return;
            }
            break;
          }
        }
      }
      server.send(200, "text/plain", "No sync needed");
    } else if (action == "adjustDay" && server.hasArg("dir")) {
      int dir = server.arg("dir").toInt();
      uint32_t currentElapsed = getElapsedSeconds();
      if (dir == 1) {
        if (batchStartUnix >= 86400) batchStartUnix -= 86400;
      } else if (dir == -1 && currentElapsed >= 86400) {
        batchStartUnix += 86400;
      }
      saveSettings();
      server.send(200, "text/plain", "Day adjusted");
    } else {
      server.send(400, "text/plain", "Invalid action");
    }
  } else {
    String json = "{\"enabled\":" + String(useMockSensor ? "true" : "false") + 
                  ",\"autosim\":" + String(autoSimMode ? "true" : "false") +
                  ",\"temp\":" + String(mockTemp) + 
                  ",\"hum\":" + String(mockHum) +
                  ",\"logInterval\":" + String(LOG_INTERVAL) +
                  ",\"eggTurnInterval\":" + String(EGG_TURN_INTERVAL) +
                  ",\"eggTurnDuration\":" + String((float)EGG_TURN_DURATION / 1000.0f, 1) +
                  ",\"angleAdjustMin\":" + String(angleAdjustMin) + ",\"angleAdjustMax\":" + String(angleAdjustMax) +
",\"pulseOnTime\":" + String(PULSE_ON_TIME) +
                   ",\"pulseOffTime\":" + String(PULSE_OFF_TIME) +
                   ",\"batchStartUnix\":" + String(batchStartUnix) +
                   ",\"elapsedSeconds\":" + String(getElapsedSeconds()) +
                   ",\"currentDay\":" + String(getCurrentDay()) +
                   ",\"stageLockdown\":" + String(stageLockdown ? "true" : "false") +
                   ",\"wifiSsid\":\"" + String(wifiSsid) + "\"" +
                   ",\"wifiPassword\":\"" + String(wifiPassword) + "\"}";
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.send(200, "application/json", json);
  }
}

// ============================================
// AUTO CONTROL LOGIC
// ============================================
void autoControl() {
  if (!isnan(currentTemp) && !isnan(currentHumidity)) {
    unsigned long now = millis();
    
    // Heater Control
    if (heaterMode == AUTO) {
      if (currentTemp <= TARGET_TEMP - TEMP_HYSTERESIS) {
        if (!heaterState && !pendingHeaterOn) {
          pendingHeaterOn = true;
          fanPreRunStart = millis();
        }
        if (pendingHeaterOn && (millis() - fanPreRunStart >= FAN_PRE_RUN_TIME)) {
          heaterState = true;
          digitalWrite(RELAY_HEATER, LOW);
          pendingHeaterOn = false;
        }
      } else if (currentTemp >= TARGET_TEMP) {
        heaterState = false;
        pendingHeaterOn = false;
        digitalWrite(RELAY_HEATER, HIGH);
      }
      
      if (heaterState != heaterWasOn) {
        heaterLastChanged = now;
        heaterWasOn = heaterState;
      }
    } else {
      if (heaterState || pendingHeaterOn) {
        heaterState = false;
        pendingHeaterOn = false;
        digitalWrite(RELAY_HEATER, HIGH);
        heaterWasOn = false;
      }
    }
    
    // Atomizer Control
    if (atomizerMode == AUTO) {
      unsigned long effectivePulseOn = PULSE_ON_TIME;
      unsigned long effectivePulseOff = PULSE_OFF_TIME;
      float humDelta = TARGET_HUMIDITY - currentHumidity;

      if (humDelta >= 25.0) {
        effectivePulseOn = PULSE_ON_TIME * 2;
        effectivePulseOff = PULSE_OFF_TIME / 2;
      } else if (humDelta >= 10.0) {
        effectivePulseOn = PULSE_ON_TIME * 2;
      }

      float humHysteresis = TARGET_HUMIDITY * 5.0f / 100.0f;
      if (currentHumidity < TARGET_HUMIDITY - humHysteresis || atomizerPulsing || atomizerInOffPhase || pendingAtomizerOn) {
        if (currentHumidity < TARGET_HUMIDITY && !atomizerPulsing && !atomizerInOffPhase && !pendingAtomizerOn) {
          pendingAtomizerOn = true;
          fanPreRunStart = millis();
        }
        if (pendingAtomizerOn && currentHumidity < TARGET_HUMIDITY && (millis() - fanPreRunStart >= FAN_PRE_RUN_TIME)) {
          atomizerState = true;
          digitalWrite(RELAY_ATOMIZER, LOW);
          atomizerPulseStart = millis();
          atomizerPulsing = true;
          pendingAtomizerOn = false;
        }
      } else if (currentHumidity >= TARGET_HUMIDITY) {
        atomizerState = false;
        pendingAtomizerOn = false;
        digitalWrite(RELAY_ATOMIZER, HIGH);
        atomizerPulsing = false;
        atomizerInOffPhase = false;
      }

      if (atomizerPulsing && (millis() - atomizerPulseStart >= effectivePulseOn)) {
        atomizerState = false;
        digitalWrite(RELAY_ATOMIZER, HIGH);
        atomizerPulsing = false;
        atomizerInOffPhase = true;
        atomizerOffStart = millis();
      } else if (atomizerInOffPhase && (millis() - atomizerOffStart >= effectivePulseOff)) {
        atomizerInOffPhase = false;
        if (currentHumidity < TARGET_HUMIDITY && !pendingAtomizerOn) {
          pendingAtomizerOn = true;
          fanPreRunStart = millis();
        }
      }
      
      if (atomizerState != atomizerWasOn) {
        atomizerLastChanged = now;
        atomizerWasOn = atomizerState;
      }
    } else {
      if (atomizerState || pendingAtomizerOn) {
        atomizerState = false;
        pendingAtomizerOn = false;
        digitalWrite(RELAY_ATOMIZER, HIGH);
        atomizerWasOn = false;
        atomizerPulsing = false;
        atomizerInOffPhase = false;
      }
    }
    
    // Fan Control
    if (fanMode == AUTO) {
      bool withinHeaterWindow = (!heaterState && (now - heaterLastChanged < FAN_EXTEND_TIME));
      bool withinAtomizerWindow = (!atomizerState && (now - atomizerLastChanged < FAN_EXTEND_TIME));
      
      if (pendingHeaterOn || pendingAtomizerOn || heaterState || withinHeaterWindow || atomizerState || atomizerInOffPhase || withinAtomizerWindow || 
          currentTemp > MAX_SAFE_TEMP) {
        fanState = true;
      } else {
        fanState = false;
      }
      digitalWrite(RELAY_FAN, fanState ? LOW : HIGH);
    } else {
      if (fanState) {
        fanState = false;
        digitalWrite(RELAY_FAN, HIGH);
      }
    }
  }
}

// ============================================
// SERVO PWM (Using ServoSmooth)
// ============================================

void servoInit() {
  int angle = constrain(currentServoStep * 6, 0, 180);
  myServo.attach(SERVO_PIN, 544, 2450, angle);  // <200 = angle mode in Servo::write()
  // No detach — servo holds position until first sweep
}

// ============================================
// EGG TURNER - Smooth step-based servo sweep
// ============================================
void rotateEggs() {
  // Disable egg turner during lockdown stage
  if (stageLockdown) {
    if (currentServoStep != 15) {
      sweeping = true;
      sweepTargetStep = 15;
    } else {
      servoEnabled = false;
      servoPosition = 0;
      return;
    }
  }

  // Calculate step endpoints
  uint8_t minStep, maxStep;
  getServoEndpoints(minStep, maxStep);

  // Start turning if interval elapsed
  if (!sweeping && !movingInStep && (millis() - lastServoTurn > EGG_TURN_INTERVAL)) {
    sweeping = true;
    lastStepTime = millis();
    lastServoTurn = millis();
    
    // Target the endpoint based on current direction
    if (isMovingTowardsMax) {
      sweepTargetStep = maxStep;
      // If we already hit max, flip direction
      if (currentServoStep >= maxStep) {
        sweepTargetStep = minStep;
        isMovingTowardsMax = false;
      }
    } else {
      sweepTargetStep = minStep;
      // If we already hit min, flip direction
      if (currentServoStep <= minStep) {
        sweepTargetStep = maxStep;
        isMovingTowardsMax = true;
      }
    }
    
    // Attach servo for the sweep at current position
    myServo.attach(SERVO_PIN, 544, 2450, currentServoStep * 6);
  }

  if (sweeping) {
    // Ensure servo is attached if interrupted sweep resumed
    if (!myServo.attached()) {
      myServo.attach(SERVO_PIN, 544, 2450, currentServoStep * 6);
    }
    
    unsigned long now = millis();
    
    if (!movingInStep) {
      // Check if sweep complete
      if (currentServoStep == sweepTargetStep) {
        delay(50);
        // Servo stays attached — holds position between sweep cycles to resist vibration
        sweeping = false;
        lastServoTurn = now;
      } else {
        // Start a smooth 6-degree movement over EGG_TURN_DURATION
        stepStartAngle = currentServoStep * 6;
        if (currentServoStep < sweepTargetStep) {
          currentServoStep++;
        } else {
          currentServoStep--;
        }
        stepTargetAngle = currentServoStep * 6;
        stepMoveStart = now;
        movingInStep = true;
      }
    }
    
    if (movingInStep) {
      unsigned long elapsed = now - stepMoveStart;
      float progress = (float)elapsed / (float)EGG_TURN_DURATION;
      
      if (progress >= 1.0f) {
        // Movement complete - set final position
        int angle = constrain(stepTargetAngle, 0, 180);
        myServo.write(angle);
        movingInStep = false;
        lastStepTime = now;
      } else {
        // Linear interpolation from start to target angle
        int angle = stepStartAngle + (int)((stepTargetAngle - stepStartAngle) * progress);
        angle = constrain(angle, 0, 180);
        myServo.write(angle);
      }
    }
  }
  
  servoPosition = currentServoStep;
  
  if (!servoEnabled || servoMode == KILL_OFF) {
    servoPosition = 0;
  }
}

// Rollback endpoints
void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(500);
  ESP.restart();
}

void handleClearFlash() {
  clearLogs();
  prepareBootTable();
  
  // Send response and reboot
  server.send(200, "text/plain", "Flash cleared, boot ID reset to 0, rebooting...");
  delay(500); // Allow time for TCP transmission
  ESP.restart();
}

// ============================================
// MAIN SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_HEATER, OUTPUT);
  pinMode(RELAY_ATOMIZER, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  digitalWrite(RELAY_HEATER, HIGH);
  digitalWrite(RELAY_ATOMIZER, HIGH);
  digitalWrite(RELAY_FAN, HIGH);

  // Hold servo pin LOW immediately to suppress SPI boot noise on GPIO14
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);

  EEPROM.begin(512);
  loadWifiCredentials();

  initWiFi();  // non-blocking, no delay loops


// Setup web server
  server.on("/", handleRoot);
  server.on("/sector_viewer", handleSectorViewerPage);
  server.on("/api/sector_hex", handleSectorHex);
  server.on("/settings", handleSettingsPage);
  server.on("/dexie", handleDexiePage);
  server.on("/status", handleStatus);
  server.on("/data", handleData);
  server.on("/control", handleControl);
  server.on("/settings/clear", handleClearFlash);
  server.on("/ota/check", handleOtaCheck);
  server.on("/ota/apply", HTTP_POST, handleOtaApply);
  server.on("/settings/api", handleSettingsApi);
  server.on("/reboot", handleReboot);
  server.on("/timestamps", handleTimestamps);
  // Register embedded asset routes (gzip'd CDN-free libraries)
  for (size_t i = 0; i < EMBEDDED_ASSETS_COUNT; i++) {
    server.on(EMBEDDED_ASSETS[i].path, handleLibAsset);
  }
  httpUpdater.setup(&server);
  // Catch-all: redirect captive portal probes (generate_204, connecttest.txt, etc.)
  // to the dashboard so the phone shows a captive portal notification instead of
  // keeping all traffic on cellular data. Uses the mDNS hostname so Android/Apple
  // captive portal browsers land on eggubator.local (not bare IP).
  server.onNotFound([]() {
    server.sendHeader("Location", "http://eggubator.local/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
  
  initSectorPointers();
  currentBootId = recoverBootIdFromFlash();
  prepareBootTable();
  initLogging(currentBootId);
  loadSettings();

  initDHT();

  uint8_t recoveredSteps[3] = {15, 15, 15};
  if (getLastServoPositions(recoveredSteps, 3)) {
    currentServoStep = recoveredSteps[0];
    // Simple direction detection: if pos0 > pos1 > pos2, moving towards min.
    // If pos0 < pos1 < pos2, moving towards max.
    if (recoveredSteps[0] > recoveredSteps[1] && recoveredSteps[1] > recoveredSteps[2]) {
      isMovingTowardsMax = false;
    } else if (recoveredSteps[0] < recoveredSteps[1] && recoveredSteps[1] < recoveredSteps[2]) {
      isMovingTowardsMax = true;
    }
    
    // Resume interrupted sweep
    uint8_t minStep, maxStep;
    getServoEndpoints(minStep, maxStep);
    if (currentServoStep > minStep && currentServoStep < maxStep) {
      sweeping = true;
      sweepTargetStep = isMovingTowardsMax ? maxStep : minStep;
    }
  }

  servoInit();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  handleWiFi();
  handleNtpSync();
  server.handleClient();
  // Start mDNS in STA or AP mode so eggubator.local resolves regardless
  static bool mdnsStarted = false;
  if (!mdnsStarted && WiFi.getMode() != WIFI_OFF) {
    mdnsStarted = MDNS.begin("EGGubator");
  }
  if (mdnsStarted) MDNS.update();

  unsigned long currentMillis = millis();

  {
    bool newStageLockdown = (getCurrentDay() >= 17); // 0-indexed Day 18 is index 17
    if (newStageLockdown != stageLockdown) {
      stageLockdown = newStageLockdown;
      if (stageLockdown) {
        TARGET_TEMP = 37.5;
        TARGET_HUMIDITY = 65.0;
        // servoEnabled = false; // Removed to allow rotateEggs to handle smooth move to 90deg
      } else {
        TARGET_TEMP = 37.5;
        TARGET_HUMIDITY = 55.0;
        servoEnabled = true;
      }
      saveSettings();
    }
  }

  if (currentMillis - lastReadTime > 2000) {
    if (autoSimMode) {
      updateAutoSim(heaterState, atomizerState, fanState);
    }
    float t = readDHT22();
    float h = readHumidity();

      if (!isnan(t) && !isnan(h) && t > 0 && h > 0) {
        currentTemp = t;
        currentHumidity = h;
        autoControl();
      }
      // Always log state (servo position, relays) even if DHT sensor unavailable
      logData(currentTemp, currentHumidity, heaterState, atomizerState, fanState, currentServoStep, LOG_INTERVAL);
    lastReadTime = millis();
  }

  if (servoEnabled) {
    rotateEggs();
  }


}
