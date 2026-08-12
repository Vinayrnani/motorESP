// ============================================
// SUBMERSIBLE PUMP CONTROLLER - motorESP
// ============================================
// PZEM-004T power monitoring + contactor control
// Framework retained from eggubator (WiFi manager, SAT, flash logging, OTA)

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>

#include "config.h"
#include "pzem_sensor.h"
#include "wifi_manager.h"
#include "logging.h"

#include "updates.h"
#include "web_ui.h"
#include "sector_viewer.h"
#include "embedded_assets.h"
#include "sat_manager.h"
#include "ntp_sync.h"

// ============================================
// MOCK MODE GLOBALS (defined here — single TU)
// ============================================
bool useMockPZEM = false;
float mockVoltage = 0.0f;
float mockCurrent = 0.0f;
float mockPower = 0.0f;
float mockEnergy = 0.0f;
float mockFrequency = 50.0f;
float mockPF = 0.0f;

// ============================================
// PUMP MODES
// ============================================
#define PUMP_MODE_OFF    0
#define PUMP_MODE_MANUAL 1
#define PUMP_MODE_AUTO   2

// Trip behavior bit flags (in tripBehavior): 1 = AUTO-RETRY, 0 = LOCKOUT
#define TB_OC_RETRY         0x01
#define TB_DRYRUN_RETRY     0x02
#define TB_OVERVOLT_RETRY   0x04
#define TB_UNDERVOLT_RETRY  0x08
#define TB_PZEM_RETRY       0x10
#define TB_STARTFAIL_RETRY  0x20

// ============================================
// CONFIGURABLE DEFAULTS (all web-modifiable)
// ============================================
float OC_RUNNING = 12.0f;
float OC_START_INSTANT = 50.0f;
unsigned long OC_DELAY = 5000;

float DRYRUN_CURRENT = 4.0f;
float DRYRUN_POWER = 500.0f;
unsigned long DRYRUN_DELAY = 15000;
unsigned long DRYRUN_ACTIVATION = 60000;

float VOLT_OVER_RUN = 250.0f;
float VOLT_UNDER_RUN = 190.0f;
float VOLT_WARN = 250.0f;
float VOLT_CRITICAL = 280.0f;
unsigned long VOLTAGE_DELAY = 3000;
unsigned long VOLTAGE_LOCKOUT = 300000;

float START_SUCCESS_CURRENT = 2.0f;
unsigned long START_VERIFY_DELAY = 1000;
unsigned long START_FAIL_BLOCK = 30000;

unsigned long MIN_RUN_TIME = 30000;
unsigned long MIN_OFF_TIME = 60000;

unsigned long AUTORETRY_DELAY = 300000;
uint8_t MAX_RETRIES = 3;
uint8_t MAX_FAST_FAULTS = 3;
unsigned long FAST_FAULT_WINDOW = 10000;

unsigned long PZEM_READ_INTERVAL_RUNNING = 1000;
unsigned long PZEM_READ_INTERVAL_OFF = 5000;

unsigned long LOG_INTERVAL_RUNNING = 10000;
unsigned long LOG_INTERVAL_OFF = 60000;

// ============================================
// EEPROM LAYOUT
// ============================================
#define EEPROM_SETTINGS_MAGIC 40
#define SETTINGS_MAGIC_VAL 0xA2

#define EEPROM_WIFI_ADDR 200
#define WIFI_MAGIC_VAL 0xAC

struct WifiSettings {
  uint8_t magic;
  char ssid[33];
  char password[65];
};

struct DeviceSettings {
  uint8_t magic;
  uint8_t pumpMode;                  // 0=OFF,1=MANUAL,2=AUTO
  uint8_t activeTrips;               // persistent trip bitmask
  uint8_t tripBehavior;              // per-protection lockout/retry flags
  uint16_t overcurrentThreshold;     // A*10
  uint16_t dryRunCurrentThreshold;   // A*10
  uint16_t dryRunPowerThreshold;     // W
  uint16_t overVoltageThreshold;     // V
  uint16_t underVoltageThreshold;    // V
  uint16_t preStartWarnVoltage;      // V
  uint16_t preStartCriticalVoltage;  // V
  uint16_t pzemReadInterval;         // seconds
  uint16_t logInterval;              // seconds
  uint16_t logIntervalOff;           // seconds
  uint16_t autoRetryDelay;           // seconds
  uint8_t maxRetries;
  uint8_t retryCount;
  uint16_t minRunTime;               // seconds
  uint16_t minOffTime;               // seconds
  uint16_t ocDelay;                  // seconds
  uint16_t dryRunDelay;              // seconds
  uint16_t voltageDelay;             // seconds
  uint8_t ocStartInstant;            // A
  uint32_t batchStartUnix;
  uint8_t fastFaultCount;
};

// ============================================
// PUMP STATE MACHINE
// ============================================
enum PumpState { ST_OFF, ST_STARTING, ST_RUNNING, ST_STOPPING, ST_TRIPPED };
PumpState pumpState = ST_OFF;
uint8_t pumpMode = PUMP_MODE_MANUAL;
uint8_t activeTrips = 0;            // persistent trip bitmask
uint8_t tripBehavior = 0;           // all LOCKOUT by default
bool permanentLockout = false;      // 3 fast faults -> requires reset
bool powerRestored = true;          // boot always requires manual start

PZEMData pzem = {0};

unsigned long lastPzemRead = 0;
unsigned long stateEnterTime = 0;
unsigned long runStartTime = 0;
unsigned long lastStopTime = 0;
unsigned long lastTripTime = 0;
unsigned long autoRetryAt = 0;
unsigned long voltageLockUntil = 0;
unsigned long startFailBlockUntil = 0;

uint8_t retryCount = 0;
uint8_t fastFaultCount = 0;

// Pulse / verification timing
unsigned long pulseStart = 0;
bool startPulsing = false;
bool stopPulsing = false;
unsigned long verifyAt = 0;

// Debounce accumulators
unsigned long ocViolSince = 0;
unsigned long dryViolSince = 0;
unsigned long voltViolSince = 0;
uint8_t voltViolType = 0;   // 0 none, 1 over, 2 under
unsigned long extStopSince = 0;

// ============================================
// WEB SERVER
// ============================================
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

uint32_t batchStartUnix = 0;

// ============================================
// RELAY CONTROL (ACTIVE LOW)
// ============================================
void relaysOff() {
  digitalWrite(RELAY_START1, HIGH);
  digitalWrite(RELAY_START2, HIGH);
  digitalWrite(RELAY_STOP,  HIGH);
  digitalWrite(RELAY_SPARE,  HIGH);
}

void fireStartPulse() {
  digitalWrite(RELAY_START1, LOW);
  digitalWrite(RELAY_START2, LOW);
  pulseStart = millis();
  startPulsing = true;
}

void fireStopPulse() {
  digitalWrite(RELAY_STOP, LOW);
  pulseStart = millis();
  stopPulsing = true;
}

// ============================================
// TRIP BEHAVIOR HELPERS
// ============================================
static uint8_t behaviorBitFor(uint8_t tripVal) {
  switch (tripVal) {
    case STATE_TRIP_OVERCURRENT: return TB_OC_RETRY;
    case STATE_TRIP_DRYRUN:      return TB_DRYRUN_RETRY;
    case STATE_TRIP_OVERVOLT:    return TB_OVERVOLT_RETRY;
    case STATE_TRIP_UNDERVOLT:   return TB_UNDERVOLT_RETRY;
    case STATE_PZEM_FAULT:       return TB_PZEM_RETRY;
    case STATE_START_FAIL:       return TB_STARTFAIL_RETRY;
  }
  return 0;
}

// ============================================
// EEPROM SETTINGS PERSISTENCE
// ============================================
void saveSettings() {
  DeviceSettings settings;
  settings.magic = SETTINGS_MAGIC_VAL;
  settings.pumpMode = pumpMode;
  settings.activeTrips = activeTrips;
  settings.tripBehavior = tripBehavior;
  settings.overcurrentThreshold = (uint16_t)(OC_RUNNING * 10.0f + 0.5f);
  settings.dryRunCurrentThreshold = (uint16_t)(DRYRUN_CURRENT * 10.0f + 0.5f);
  settings.dryRunPowerThreshold = (uint16_t)DRYRUN_POWER;
  settings.overVoltageThreshold = (uint16_t)VOLT_OVER_RUN;
  settings.underVoltageThreshold = (uint16_t)VOLT_UNDER_RUN;
  settings.preStartWarnVoltage = (uint16_t)VOLT_WARN;
  settings.preStartCriticalVoltage = (uint16_t)VOLT_CRITICAL;
  settings.pzemReadInterval = (uint16_t)(PZEM_READ_INTERVAL_RUNNING / 1000);
  settings.logInterval = (uint16_t)(LOG_INTERVAL_RUNNING / 1000);
  settings.logIntervalOff = (uint16_t)(LOG_INTERVAL_OFF / 1000);
  settings.autoRetryDelay = (uint16_t)(AUTORETRY_DELAY / 1000);
  settings.maxRetries = MAX_RETRIES;
  settings.retryCount = retryCount;
  settings.minRunTime = (uint16_t)(MIN_RUN_TIME / 1000);
  settings.minOffTime = (uint16_t)(MIN_OFF_TIME / 1000);
  settings.ocDelay = (uint16_t)(OC_DELAY / 1000);
  settings.dryRunDelay = (uint16_t)(DRYRUN_DELAY / 1000);
  settings.voltageDelay = (uint16_t)(VOLTAGE_DELAY / 1000);
  settings.ocStartInstant = (uint8_t)OC_START_INSTANT;
  settings.batchStartUnix = batchStartUnix;
  settings.fastFaultCount = fastFaultCount;
  EEPROM.put(EEPROM_SETTINGS_MAGIC, settings);
  EEPROM.commit();
}

void saveActiveTrips() {
  // OPTION B (debounced): write only when trip type changes
  DeviceSettings settings;
  EEPROM.get(EEPROM_SETTINGS_MAGIC, settings);
  if (settings.magic != SETTINGS_MAGIC_VAL) { saveSettings(); return; }
  if (settings.activeTrips != activeTrips) {
    settings.activeTrips = activeTrips;
    EEPROM.put(EEPROM_SETTINGS_MAGIC, settings);
    EEPROM.commit();
  }
}

void loadSettings() {
  DeviceSettings settings;
  EEPROM.get(EEPROM_SETTINGS_MAGIC, settings);
  bool sane = (settings.magic == SETTINGS_MAGIC_VAL);
  if (sane) {
    sane = (settings.overcurrentThreshold >= 30 && settings.overcurrentThreshold <= 300)
        && (settings.dryRunCurrentThreshold >= 10 && settings.dryRunCurrentThreshold <= 150)
        && (settings.dryRunPowerThreshold >= 50 && settings.dryRunPowerThreshold <= 3000)
        && (settings.overVoltageThreshold >= 200 && settings.overVoltageThreshold <= 300)
        && (settings.underVoltageThreshold >= 150 && settings.underVoltageThreshold <= 240)
        && (settings.preStartWarnVoltage >= 200 && settings.preStartWarnVoltage <= 300)
        && (settings.preStartCriticalVoltage >= 210 && settings.preStartCriticalVoltage <= 320)
        && (settings.pzemReadInterval >= 1 && settings.pzemReadInterval <= 60)
        && (settings.logInterval >= 1 && settings.logInterval <= 600)
        && (settings.logIntervalOff >= 1 && settings.logIntervalOff <= 600)
        && (settings.autoRetryDelay >= 30 && settings.autoRetryDelay <= 7200)
        && (settings.maxRetries >= 1 && settings.maxRetries <= 10)
        && (settings.minRunTime >= 5 && settings.minRunTime <= 600)
        && (settings.minOffTime >= 5 && settings.minOffTime <= 1200)
        && (settings.ocDelay >= 1 && settings.ocDelay <= 60)
        && (settings.dryRunDelay >= 1 && settings.dryRunDelay <= 300)
        && (settings.voltageDelay >= 1 && settings.voltageDelay <= 60)
        && (settings.ocStartInstant >= 10 && settings.ocStartInstant <= 200);
  }
  if (sane) {
    pumpMode = (settings.pumpMode <= PUMP_MODE_AUTO) ? settings.pumpMode : PUMP_MODE_MANUAL;
    activeTrips = settings.activeTrips;
    tripBehavior = settings.tripBehavior;
    OC_RUNNING = settings.overcurrentThreshold / 10.0f;
    DRYRUN_CURRENT = settings.dryRunCurrentThreshold / 10.0f;
    DRYRUN_POWER = settings.dryRunPowerThreshold;
    VOLT_OVER_RUN = settings.overVoltageThreshold;
    VOLT_UNDER_RUN = settings.underVoltageThreshold;
    VOLT_WARN = settings.preStartWarnVoltage;
    VOLT_CRITICAL = settings.preStartCriticalVoltage;
    PZEM_READ_INTERVAL_RUNNING = (unsigned long)settings.pzemReadInterval * 1000;
    LOG_INTERVAL_RUNNING = (unsigned long)settings.logInterval * 1000;
    LOG_INTERVAL_OFF = (unsigned long)settings.logIntervalOff * 1000;
    AUTORETRY_DELAY = (unsigned long)settings.autoRetryDelay * 1000;
    MAX_RETRIES = settings.maxRetries;
    retryCount = settings.retryCount;
    MIN_RUN_TIME = (unsigned long)settings.minRunTime * 1000;
    MIN_OFF_TIME = (unsigned long)settings.minOffTime * 1000;
    OC_DELAY = (unsigned long)settings.ocDelay * 1000;
    DRYRUN_DELAY = (unsigned long)settings.dryRunDelay * 1000;
    VOLTAGE_DELAY = (unsigned long)settings.voltageDelay * 1000;
    OC_START_INSTANT = settings.ocStartInstant;
    batchStartUnix = settings.batchStartUnix;
    fastFaultCount = settings.fastFaultCount;
  } else {
    initConfigDefaults();
  }
}

void initConfigDefaults() {
  OC_RUNNING = 12.0f;
  OC_START_INSTANT = 50.0f;
  OC_DELAY = 5000;
  DRYRUN_CURRENT = 4.0f;
  DRYRUN_POWER = 500.0f;
  DRYRUN_DELAY = 15000;
  DRYRUN_ACTIVATION = 60000;
  VOLT_OVER_RUN = 250.0f;
  VOLT_UNDER_RUN = 190.0f;
  VOLT_WARN = 250.0f;
  VOLT_CRITICAL = 280.0f;
  VOLTAGE_DELAY = 3000;
  VOLTAGE_LOCKOUT = 300000;
  START_SUCCESS_CURRENT = 2.0f;
  START_VERIFY_DELAY = 1000;
  START_FAIL_BLOCK = 30000;
  MIN_RUN_TIME = 30000;
  MIN_OFF_TIME = 60000;
  AUTORETRY_DELAY = 300000;
  MAX_RETRIES = 3;
  MAX_FAST_FAULTS = 3;
  FAST_FAULT_WINDOW = 10000;
  PZEM_READ_INTERVAL_RUNNING = 1000;
  PZEM_READ_INTERVAL_OFF = 5000;
  LOG_INTERVAL_RUNNING = 10000;
  LOG_INTERVAL_OFF = 60000;
  pumpMode = PUMP_MODE_MANUAL;
  tripBehavior = 0;
  retryCount = 0;
  fastFaultCount = 0;
  saveSettings();
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
// PUMP START / STOP / TRIP
// ============================================
bool startPump(bool manual) {
  unsigned long now = millis();
  if (activeTrips || permanentLockout) return false;
  if (now < startFailBlockUntil) return false;
  if (now < voltageLockUntil) return false;
  if (powerRestored && !manual) return false;
  if ((now - lastStopTime) < MIN_OFF_TIME) return false;
  if (!useMockPZEM && !pzem.valid) return false;
  if (pzem.valid && pzem.voltage >= VOLT_CRITICAL) return false;

  if (manual) powerRestored = false;
  pumpState = ST_STARTING;
  stateEnterTime = now;
  verifyAt = 0;
  ocViolSince = dryViolSince = voltViolSince = 0;
  voltViolType = 0;
  fireStartPulse();
  return true;
}

void requestStop() {
  unsigned long now = millis();
  if (pumpState != ST_RUNNING && pumpState != ST_STARTING) return;
  if (pumpState == ST_RUNNING && (now - runStartTime) < MIN_RUN_TIME) return;
  pumpState = ST_STOPPING;
  stateEnterTime = now;
  fireStopPulse();
}

void tripPump(uint8_t tripVal) {
  unsigned long now = millis();
  bool newTripType = (activeTrips & tripVal) == 0;

  relaysOff();
  pumpState = ST_TRIPPED;
  stateEnterTime = now;
  activeTrips |= tripVal;
  if (newTripType) saveActiveTrips();  // Option B debounced write

  // Fast fault tracking: recurred within FAST_FAULT_WINDOW of previous trip
  if (lastTripTime != 0 && (now - lastTripTime) < FAST_FAULT_WINDOW) {
    fastFaultCount++;
  } else {
    fastFaultCount = 1;
  }
  if (fastFaultCount >= MAX_FAST_FAULTS && MAX_FAST_FAULTS > 0) {
    permanentLockout = true;
    autoRetryAt = 0;
  }
  lastTripTime = now;

  // Auto-retry scheduling
  bool retryConfigured = (behaviorBitFor(tripVal) & tripBehavior) != 0;
  if (retryConfigured && !permanentLockout && retryCount < MAX_RETRIES) {
    autoRetryAt = now + AUTORETRY_DELAY;
  } else {
    autoRetryAt = 0;
  }

  saveActiveTrips();
}

void resetTrips() {
  relaysOff();
  pumpState = ST_OFF;
  stateEnterTime = millis();
  activeTrips = 0;
  retryCount = 0;
  fastFaultCount = 0;
  permanentLockout = false;
  autoRetryAt = 0;
  voltageLockUntil = 0;
  startFailBlockUntil = 0;
  saveActiveTrips();
}

// ============================================
// STATE MACHINE (called from loop, every iteration)
// ============================================
void runStateMachine() {
  unsigned long now = millis();

  switch (pumpState) {
    case ST_OFF: {
      // Manual (external) start detection: physical GREEN press bypasses ESP relays
      if (!activeTrips && !permanentLockout &&
          pzem.valid && pzem.current >= START_SUCCESS_CURRENT &&
          (now - lastStopTime) > 1500) {
        powerRestored = false;
        pumpState = ST_RUNNING;
        runStartTime = now;
        stateEnterTime = now;
        ocViolSince = dryViolSince = voltViolSince = 0;
        voltViolType = 0;
      }
      break;
    }

    case ST_STARTING: {
      if (startPulsing && (now - pulseStart) >= PULSE_DURATION) {
        relaysOff();
        startPulsing = false;
        verifyAt = now + START_VERIFY_DELAY;
      }
      // Instantaneous OC during start inrush (before verify)
      if (pzem.valid && pzem.current >= OC_START_INSTANT) {
        tripPump(STATE_TRIP_OVERCURRENT);
        break;
      }
      if (!startPulsing && now >= verifyAt) {
        readPZEM();
        if (pzem.valid && pzem.current >= START_SUCCESS_CURRENT) {
          pumpState = ST_RUNNING;
          runStartTime = now;
          stateEnterTime = now;
          ocViolSince = dryViolSince = voltViolSince = 0;
          voltViolType = 0;
        } else {
          // Start failure — block retries for START_FAIL_BLOCK
          startFailBlockUntil = now + START_FAIL_BLOCK;
          tripPump(STATE_START_FAIL);
        }
      }
      break;
    }

    case ST_RUNNING: {
      // Successful run clears fast-fault / retry history
      if (fastFaultCount > 0 && (now - runStartTime) > FAST_FAULT_WINDOW) fastFaultCount = 0;
      if ((now - runStartTime) > FAST_FAULT_WINDOW) retryCount = 0;

      if (!pzem.valid && !useMockPZEM) {
        // PZEM fault — fail-safe trip OFF
        tripPump(STATE_PZEM_FAULT);
        break;
      }

      // External STOP detection: current collapses while running
      if (pzem.valid && pzem.current < 0.5f) {
        if (extStopSince == 0) extStopSince = now;
        if ((now - extStopSince) >= 1500) {
          extStopSince = 0;
          pumpState = ST_OFF;
          stateEnterTime = now;
          lastStopTime = now;
          relaysOff();
        }
      } else {
        extStopSince = 0;
      }
      if (pumpState != ST_RUNNING) break;

      // Overcurrent (two-stage)
      bool inStartWindow = (now - runStartTime) < OC_DELAY;
      float ocThreshold = inStartWindow ? OC_START_INSTANT : OC_RUNNING;
      if (pzem.valid && pzem.current >= ocThreshold) {
        if (inStartWindow) {
          tripPump(STATE_TRIP_OVERCURRENT);
          break;
        }
        if (ocViolSince == 0) ocViolSince = now;
        if ((now - ocViolSince) >= OC_DELAY) {
          tripPump(STATE_TRIP_OVERCURRENT);
          break;
        }
      } else {
        ocViolSince = 0;
      }

      // Dry-run (only after activation delay)
      if ((now - runStartTime) >= DRYRUN_ACTIVATION &&
          pzem.valid && pzem.current < DRYRUN_CURRENT && pzem.power < DRYRUN_POWER) {
        if (dryViolSince == 0) dryViolSince = now;
        if ((now - dryViolSince) >= DRYRUN_DELAY) {
          tripPump(STATE_TRIP_DRYRUN);
          break;
        }
      } else {
        dryViolSince = 0;
      }

      // Voltage (running): over / under with delay, then lockout
      uint8_t thisViolType = 0;
      if (pzem.valid && pzem.voltage > VOLT_OVER_RUN) thisViolType = 1;
      else if (pzem.valid && pzem.voltage < VOLT_UNDER_RUN) thisViolType = 2;

      if (thisViolType != 0) {
        if (thisViolType != voltViolType) {
          voltViolType = thisViolType;
          voltViolSince = now;
        }
        if ((now - voltViolSince) >= VOLTAGE_DELAY) {
          voltageLockUntil = now + VOLTAGE_LOCKOUT;
          tripPump(thisViolType == 1 ? STATE_TRIP_OVERVOLT : STATE_TRIP_UNDERVOLT);
          break;
        }
      } else {
        voltViolType = 0;
        voltViolSince = 0;
      }
      break;
    }

    case ST_STOPPING: {
      if (stopPulsing && (now - pulseStart) >= PULSE_DURATION) {
        relaysOff();
        stopPulsing = false;
        pumpState = ST_OFF;
        stateEnterTime = now;
        lastStopTime = now;
      }
      break;
    }

    case ST_TRIPPED: {
      // Auto-retry after configured delay
      if (autoRetryAt != 0 && now >= autoRetryAt &&
          !permanentLockout && retryCount < MAX_RETRIES) {
        retryCount++;
        saveActiveTrips();
        if (startPump(false)) {
          autoRetryAt = 0;
        } else {
          autoRetryAt = now + AUTORETRY_DELAY;
        }
      }
      break;
    }
  }
}

// ============================================
// LOGGING
// ============================================
uint8_t currentStatesByte() {
  uint8_t states = 0;
  if (pumpState == ST_RUNNING || pumpState == ST_STARTING) states |= STATE_PUMP_RUNNING;
  if (pumpMode == PUMP_MODE_AUTO) states |= STATE_AUTO_MODE;
  states |= activeTrips;
  return states;
}

void handleLogging() {
  unsigned long interval = (pumpState == ST_RUNNING) ? LOG_INTERVAL_RUNNING : LOG_INTERVAL_OFF;
  logPumpData(pzem, currentStatesByte(), interval);
}

// ============================================
// WEB HANDLERS
// ============================================
void handleSectorViewerPage() {
  server.send(200, "text/html; charset=utf-8", SECTOR_VIEWER_HTML);
}

void handleSectorHex() {
  if (server.method() == HTTP_POST) {
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
    memset(buffer, 0xFF, FLASH_SECTOR_SIZE);
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
    uint32_t flashAddr = FLASH_LOG_START + (sector * FLASH_SECTOR_SIZE);
    uint32_t sectorId = flashAddr / FLASH_SECTOR_SIZE;
    ESP.flashEraseSector(sectorId);
    ESP.flashWrite(flashAddr, (uint32_t*)buffer, FLASH_SECTOR_SIZE);
    free(buffer);
    server.send(200, "text/plain", "OK");
    return;
  }

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

void handleDashboardPage() {
  server.send(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void handleSettingsPage() {
  server.send(200, "text/html; charset=utf-8", SETTINGS_HTML);
}

void handleDataPage() {
  server.send(200, "text/html; charset=utf-8", DATA_HTML);
}

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

String statesName(uint8_t trips) {
  String s;
  if (trips & STATE_TRIP_OVERCURRENT) s += "Overcurrent ";
  if (trips & STATE_TRIP_DRYRUN) s += "DryRun ";
  if (trips & STATE_TRIP_OVERVOLT) s += "OverVoltage ";
  if (trips & STATE_TRIP_UNDERVOLT) s += "UnderVoltage ";
  if (trips & STATE_PZEM_FAULT) s += "PZEMFault ";
  if (trips & STATE_START_FAIL) s += "StartFail ";
  if (permanentLockout) s += "PERMANENT ";
  if (s.length() == 0) return "NONE";
  return s.substring(0, s.length() - 1);
}

String pumpStateName() {
  switch (pumpState) {
    case ST_OFF:      return "OFF";
    case ST_STARTING: return "STARTING";
    case ST_RUNNING:  return "RUNNING";
    case ST_STOPPING: return "STOPPING";
    case ST_TRIPPED:  return "TRIPPED";
  }
  return "OFF";
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

  String voltageStatus = "NORMAL";
  if (pzem.valid) {
    if (pzem.voltage >= VOLT_CRITICAL) voltageStatus = "CRITICAL";
    else if (pzem.voltage >= VOLT_WARN) voltageStatus = "WARNING";
  }

  String autoRetryIn = "0";
  if (pumpState == ST_TRIPPED && autoRetryAt != 0 && !permanentLockout) {
    autoRetryIn = String(autoRetryAt > millis() ? (autoRetryAt - millis()) / 1000 : 0);
  }

  String json = "{\"voltage\":" + String(pzem.voltage) +
                ",\"current\":" + String(pzem.current) +
                ",\"power\":" + String(pzem.power) +
                ",\"energyKwh\":" + String(pzem.energy / 1000.0f, 2) +
                ",\"frequency\":" + String(pzem.frequency) +
                ",\"pf\":" + String(pzem.pf) +
                ",\"pzemValid\":" + String(pzem.valid ? 1 : 0) +
                ",\"pumpState\":\"" + pumpStateName() + "\"" +
                ",\"pumpStateRaw\":" + String((int)pumpState) +
                ",\"pumpMode\":" + String(pumpMode) +
                ",\"trips\":" + String(activeTrips) +
                ",\"tripNames\":\"" + statesName(activeTrips) + "\"" +
                ",\"permanentLockout\":" + String(permanentLockout ? 1 : 0) +
                ",\"powerRestored\":" + String(powerRestored ? 1 : 0) +
                ",\"retryCount\":" + String(retryCount) +
                ",\"fastFaultCount\":" + String(fastFaultCount) +
                ",\"maxRetries\":" + String(MAX_RETRIES) +
                ",\"autoRetryIn\":" + autoRetryIn +
                ",\"voltageStatus\":\"" + voltageStatus + "\"" +
                ",\"startFailBlock\":" + String(startFailBlockUntil > millis() ? (startFailBlockUntil - millis()) / 1000 : 0) +
                ",\"version\":\"" + FIRMWARE_VERSION + "\"" +
                ",\"uptime\":\"" + uptimeStr + "\"" +
                ",\"mock\":" + String(useMockPZEM ? 1 : 0) +
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
                ",\"logsInCurrentBoot\":" + String(logsInCurrentBoot) +
                ",\"totalLogs\":" + String(getTotalLogs()) + "}";

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", json);
}

void handleData() {
  String json = String("{\"version\":\"") + FIRMWARE_VERSION + "\"";

  uint8_t sinceBootId = 0;
  if (server.hasArg("boot")) sinceBootId = (uint8_t)server.arg("boot").toInt();
  uint32_t sinceTimeSec = 0;
  if (server.hasArg("time")) sinceTimeSec = (uint32_t)server.arg("time").toInt();

  int count = 200;
  if (server.hasArg("count")) {
    count = server.arg("count").toInt();
    if (count > 200) count = 200;
    if (count < 1) count = 1;
  }

  String logHex = "";
  int sentCount = getLogHex(logHex, count, sinceBootId, sinceTimeSec);

  json += ",\"totalLogs\":" + String(getTotalLogs()) +
          ",\"sentCount\":" + String(sentCount) +
          ",\"logs\":\"" + logHex + "\"}";

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", json);
}

void handleControl() {
  if (!server.hasArg("action")) {
    server.send(400, "text/plain", "Missing action");
    return;
  }
  String action = server.arg("action");
  if (action == "start") {
    if (activeTrips || permanentLockout) {
      server.send(200, "text/plain", "BLOCKED: active trip — reset first");
    } else if (startPump(true)) {
      server.send(200, "text/plain", "Starting");
    } else {
      server.send(200, "text/plain", "BLOCKED: interlock");
    }
  } else if (action == "stop") {
    if (pumpState == ST_RUNNING && (millis() - runStartTime) < MIN_RUN_TIME) {
      server.send(200, "text/plain", "BLOCKED: min run time active");
    } else {
      requestStop();
      server.send(200, "text/plain", "Stopping");
    }
  } else if (action == "reset") {
    resetTrips();
    server.send(200, "text/plain", "Trips reset");
  } else if (action == "mode") {
    if (server.hasArg("mode")) {
      int m = server.arg("mode").toInt();
      if (m >= PUMP_MODE_OFF && m <= PUMP_MODE_AUTO) {
        pumpMode = (uint8_t)m;
        if (pumpMode == PUMP_MODE_OFF) requestStop();
        saveSettings();
        server.send(200, "text/plain", "Mode set");
      } else {
        server.send(400, "text/plain", "Invalid mode");
      }
    } else {
      server.send(400, "text/plain", "Missing mode");
    }
  } else {
    server.send(400, "text/plain", "Invalid action");
  }
}

void handleOtaCheck() {
  String current = FIRMWARE_VERSION;
  String latest = "";

  int cMaj = 0, cMin = 0, cPat = 0;
  int cFirstDot = current.indexOf('.');
  int cLastDot = current.lastIndexOf('.');
  if (cFirstDot > 0 && cLastDot > cFirstDot) {
    cMaj = current.substring(0, cFirstDot).toInt();
    cMin = current.substring(cFirstDot + 1, cLastDot).toInt();
    cPat = current.substring(cLastDot + 1).toInt();
  }

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
        if (tagEnd != -1) latest = payload.substring(tagStart, tagEnd);
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
  if (success) return;  // ESP.restart() inside
  updateInProgress = false;
  String errorMsg = "Update failed";
  int lastErr = ESPhttpUpdate.getLastError();
  if (lastErr != 0) errorMsg += ": " + ESPhttpUpdate.getLastErrorString();
  server.send(200, "application/json", "{\"status\":\"error\",\"message\":\"" + errorMsg + "\"}");
}

String settingsJson() {
  String json = "{\"mock\":" + String(useMockPZEM ? "true" : "false") +
                ",\"pumpMode\":" + String(pumpMode) +
                ",\"tripBehavior\":" + String(tripBehavior) +
                ",\"activeTrips\":" + String(activeTrips) +
                ",\"ocRunning\":" + String(OC_RUNNING, 1) +
                ",\"ocStartInstant\":" + String(OC_START_INSTANT, 0) +
                ",\"ocDelay\":" + String(OC_DELAY / 1000) +
                ",\"dryRunCurrent\":" + String(DRYRUN_CURRENT, 1) +
                ",\"dryRunPower\":" + String(DRYRUN_POWER, 0) +
                ",\"dryRunDelay\":" + String(DRYRUN_DELAY / 1000) +
                ",\"dryRunActivation\":" + String(DRYRUN_ACTIVATION / 1000) +
                ",\"voltOver\":" + String(VOLT_OVER_RUN, 0) +
                ",\"voltUnder\":" + String(VOLT_UNDER_RUN, 0) +
                ",\"voltWarn\":" + String(VOLT_WARN, 0) +
                ",\"voltCritical\":" + String(VOLT_CRITICAL, 0) +
                ",\"voltageDelay\":" + String(VOLTAGE_DELAY / 1000) +
                ",\"voltageLockout\":" + String(VOLTAGE_LOCKOUT / 1000) +
                ",\"startSuccessCurrent\":" + String(START_SUCCESS_CURRENT, 1) +
                ",\"startVerifyDelay\":" + String(START_VERIFY_DELAY / 1000) +
                ",\"startFailBlock\":" + String(START_FAIL_BLOCK / 1000) +
                ",\"minRun\":" + String(MIN_RUN_TIME / 1000) +
                ",\"minOff\":" + String(MIN_OFF_TIME / 1000) +
                ",\"autoRetryDelay\":" + String(AUTORETRY_DELAY / 1000) +
                ",\"maxRetries\":" + String(MAX_RETRIES) +
                ",\"maxFastFaults\":" + String(MAX_FAST_FAULTS) +
                ",\"fastFaultWindow\":" + String(FAST_FAULT_WINDOW / 1000) +
                ",\"pzemReadRunning\":" + String(PZEM_READ_INTERVAL_RUNNING / 1000) +
                ",\"pzemReadOff\":" + String(PZEM_READ_INTERVAL_OFF / 1000) +
                ",\"logIntervalRunning\":" + String(LOG_INTERVAL_RUNNING / 1000) +
                ",\"logIntervalOff\":" + String(LOG_INTERVAL_OFF / 1000) +
                ",\"batchStartUnix\":" + String(batchStartUnix) +
                ",\"elapsedSeconds\":" + String(getElapsedSeconds()) +
                ",\"currentDay\":" + String(getCurrentDay()) +
                ",\"wifiSsid\":\"" + String(wifiSsid) + "\"" +
                ",\"wifiPassword\":\"" + String(wifiPassword) + "\"}";
  return json;
}

void handleSettingsApi() {
  if (server.method() == HTTP_POST || server.args() > 0) {
    // Handle settings updates (POST-style)
    if (server.hasArg("pumpMode")) {
      int m = server.arg("pumpMode").toInt();
      if (m >= PUMP_MODE_OFF && m <= PUMP_MODE_AUTO) {
        pumpMode = (uint8_t)m;
        if (pumpMode == PUMP_MODE_OFF) requestStop();
      }
    }
    if (server.hasArg("ocRunning")) OC_RUNNING = constrain(server.arg("ocRunning").toFloat(), 1.0f, 30.0f);
    if (server.hasArg("ocStartInstant")) OC_START_INSTANT = constrain(server.arg("ocStartInstant").toInt(), 1, 100);
    if (server.hasArg("ocDelay")) OC_DELAY = (unsigned long)constrain(server.arg("ocDelay").toInt(), 1, 30) * 1000;
    if (server.hasArg("dryRunCurrent")) DRYRUN_CURRENT = constrain(server.arg("dryRunCurrent").toFloat(), 0.5f, 20.0f);
    if (server.hasArg("dryRunPower")) DRYRUN_POWER = constrain(server.arg("dryRunPower").toFloat(), 10.0f, 5000.0f);
    if (server.hasArg("dryRunDelay")) DRYRUN_DELAY = (unsigned long)constrain(server.arg("dryRunDelay").toInt(), 1, 300) * 1000;
    if (server.hasArg("dryRunActivation")) DRYRUN_ACTIVATION = (unsigned long)constrain(server.arg("dryRunActivation").toInt(), 0, 3600) * 1000;
    if (server.hasArg("voltOver")) VOLT_OVER_RUN = constrain(server.arg("voltOver").toFloat(), 200.0f, 300.0f);
    if (server.hasArg("voltUnder")) VOLT_UNDER_RUN = constrain(server.arg("voltUnder").toFloat(), 100.0f, 240.0f);
    if (server.hasArg("voltWarn")) VOLT_WARN = constrain(server.arg("voltWarn").toFloat(), 200.0f, 300.0f);
    if (server.hasArg("voltCritical")) VOLT_CRITICAL = constrain(server.arg("voltCritical").toFloat(), 200.0f, 300.0f);
    if (server.hasArg("voltageDelay")) VOLTAGE_DELAY = (unsigned long)constrain(server.arg("voltageDelay").toInt(), 1, 60) * 1000;
    if (server.hasArg("voltageLockout")) VOLTAGE_LOCKOUT = (unsigned long)constrain(server.arg("voltageLockout").toInt(), 0, 3600) * 1000;
    if (server.hasArg("startSuccessCurrent")) START_SUCCESS_CURRENT = constrain(server.arg("startSuccessCurrent").toFloat(), 0.5f, 10.0f);
    if (server.hasArg("startVerifyDelay")) START_VERIFY_DELAY = (unsigned long)constrain(server.arg("startVerifyDelay").toInt(), 1, 10) * 1000;
    if (server.hasArg("startFailBlock")) START_FAIL_BLOCK = (unsigned long)constrain(server.arg("startFailBlock").toInt(), 1, 600) * 1000;
    if (server.hasArg("minRun")) MIN_RUN_TIME = (unsigned long)constrain(server.arg("minRun").toInt(), 10, 300) * 1000;
    if (server.hasArg("minOff")) MIN_OFF_TIME = (unsigned long)constrain(server.arg("minOff").toInt(), 10, 600) * 1000;
    if (server.hasArg("autoRetryDelay")) AUTORETRY_DELAY = (unsigned long)constrain(server.arg("autoRetryDelay").toInt(), 60, 3600) * 1000;
    if (server.hasArg("maxRetries")) MAX_RETRIES = constrain(server.arg("maxRetries").toInt(), 1, 10);
    if (server.hasArg("tripBehavior")) tripBehavior = (uint8_t)server.arg("tripBehavior").toInt();

    if (server.hasArg("logIntervalRunning")) LOG_INTERVAL_RUNNING = (unsigned long)constrain(server.arg("logIntervalRunning").toInt(), 5, 60) * 1000;
    if (server.hasArg("logIntervalOff")) LOG_INTERVAL_OFF = (unsigned long)constrain(server.arg("logIntervalOff").toInt(), 30, 600) * 1000;
    if (server.hasArg("pzemReadRunning")) PZEM_READ_INTERVAL_RUNNING = (unsigned long)constrain(server.arg("pzemReadRunning").toInt(), 1, 5) * 1000;

    if (server.hasArg("mock")) {
      bool enable = (server.arg("mock") == "1");
      setMockPZEM(enable);
      if (!enable) setMockValues(0, 0, 0, mockEnergy, 50.0f, 0);
    }
    if (server.hasArg("mockProfile")) {
      server.arg("mockProfile") == "running" ? setMockRunning() :
      server.arg("mockProfile") == "dryrun"  ? setMockDryRun() :
      server.arg("mockProfile") == "oc"      ? setMockOvercurrent() : setMockOff();
      setMockPZEM(true);
    }
    if (server.hasArg("mockVoltage")) { mockVoltage = server.arg("mockVoltage").toFloat(); setMockPZEM(true); }
    if (server.hasArg("mockCurrent")) { mockCurrent = server.arg("mockCurrent").toFloat(); setMockPZEM(true); }
    if (server.hasArg("mockPower")) { mockPower = server.arg("mockPower").toFloat(); setMockPZEM(true); }

    if (server.hasArg("wifiSsid")) {
      String ssid = server.arg("wifiSsid");
      String password = server.arg("wifiPassword");
      if (ssid.length() == 0) {
        strncpy(wifiSsid, DEFAULT_WIFI_SSID, 32); wifiSsid[32] = '\0';
        strncpy(wifiPassword, DEFAULT_WIFI_PASSWORD, 64); wifiPassword[64] = '\0';
        WifiSettings clearSettings; clearSettings.magic = 0;
        EEPROM.put(EEPROM_WIFI_ADDR, clearSettings); EEPROM.commit();
        server.send(200, "text/plain", "WiFi credentials cleared");
        return;
      } else if (ssid.length() > 32 || password.length() > 64) {
        server.send(400, "text/plain", "Credentials too long");
        return;
      } else {
        strncpy(wifiSsid, ssid.c_str(), 32); wifiSsid[32] = '\0';
        strncpy(wifiPassword, password.c_str(), 64); wifiPassword[64] = '\0';
        saveWifiCredentials(wifiSsid, wifiPassword);
        server.send(200, "text/plain", "WiFi credentials saved");
        return;
      }
    }

    if (server.hasArg("action")) {
      String action = server.arg("action");
      if (action == "newBatch" && server.hasArg("timestamp")) {
        batchStartUnix = (uint32_t)server.arg("timestamp").toInt();
        clearLogs();
        saveSettings();
        server.send(200, "text/plain", "New batch started");
        delay(100);
        ESP.restart();
        return;
      } else if (action == "resetTrips") {
        resetTrips();
        server.send(200, "text/plain", "Trips reset");
        return;
      }
    }

    saveSettings();
    server.send(200, "application/json", settingsJson());
    return;
  }

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "application/json", settingsJson());
}

void handleReboot() {
  server.send(200, "text/plain", "Rebooting...");
  delay(500);
  ESP.restart();
}

void handleClearFlash() {
  clearLogs();
  prepareBootTable();
  server.send(200, "text/plain", "Flash cleared, boot ID reset, rebooting...");
  delay(500);
  ESP.restart();
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_START1, OUTPUT);
  pinMode(RELAY_STOP,   OUTPUT);
  pinMode(RELAY_START2, OUTPUT);
  pinMode(RELAY_SPARE,  OUTPUT);
  relaysOff();  // Safe boot: CH1/CH3 open (start circuit broken), CH2 NC closed

  EEPROM.begin(512);
  loadWifiCredentials();

  initWiFi();  // non-blocking

  server.on("/", handleRoot);
  server.on("/dashboard", handleDashboardPage);
  server.on("/settings", handleSettingsPage);
  server.on("/data", handleDataPage);
  server.on("/sector_viewer", handleSectorViewerPage);
  server.on("/api/sector_hex", handleSectorHex);
  server.on("/status", handleStatus);
  server.on("/data/api", handleData);
  server.on("/control", handleControl);
  server.on("/settings/clear", handleClearFlash);
  server.on("/ota/check", handleOtaCheck);
  server.on("/ota/apply", HTTP_POST, handleOtaApply);
  server.on("/settings/api", handleSettingsApi);
  server.on("/reboot", handleReboot);
  server.on("/timestamps", handleTimestamps);
  for (size_t i = 0; i < EMBEDDED_ASSETS_COUNT; i++) {
    server.on(EMBEDDED_ASSETS[i].path, handleLibAsset);
  }
  httpUpdater.setup(&server);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://motorESP.local/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  initSectorPointers();
  currentBootId = recoverBootIdFromFlash();
  prepareBootTable();
  initLogging(currentBootId);
  loadSettings();

  initPZEM();

  // On boot, active trip (if persisted) blocks starts until manual reset
  if (activeTrips) {
    pumpState = ST_TRIPPED;
    powerRestored = true;
  }
}

// ============================================
// LOOP
// ============================================
void loop() {
  handleWiFi();
  handleNtpSync();
  server.handleClient();
  static bool mdnsStarted = false;
  if (!mdnsStarted && WiFi.getMode() != WIFI_OFF) {
    mdnsStarted = MDNS.begin("motorESP");
  }
  if (mdnsStarted) MDNS.update();

  unsigned long now = millis();
  unsigned long readInterval = (pumpState == ST_RUNNING || pumpState == ST_STARTING)
                                   ? PZEM_READ_INTERVAL_RUNNING : PZEM_READ_INTERVAL_OFF;
  if (now - lastPzemRead >= readInterval) {
    pzem = readPZEM();
    lastPzemRead = now;
  }

  runStateMachine();
  handleLogging();
}