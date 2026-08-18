#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi Configuration
#define WIFI_SSID "Sweet Home"
#define WIFI_PASSWORD "dishoom1234"

// ============================================
// PIN MAPPING (REVISED — GPIO0 NOT USED)
// ============================================
// D1=GPIO5, D2=GPIO4, D4=GPIO2, D7=GPIO13 are all safe output pins
// D3=GPIO0 is a BOOT STRAP pin — must be HIGH at boot, NOT used for relay

#define RELAY_START1 D1    // CH1 NO — START #1 (series with CH3)
#define RELAY_STOP   D2    // CH2 NC — STOP (breaks contactor coil circuit)
#define RELAY_START2 D4    // CH3 NO — START #2 (series with CH1)
#define RELAY_SPARE  D7    // CH4 — spare output

#define PZEM_TX_PIN D5      // SoftwareSerial TX → PZEM RX (GPIO14)
#define PZEM_RX_PIN D6      // SoftwareSerial RX → PZEM TX (GPIO12)

// ============================================
// RELAY BEHAVIOR
// ============================================
// Standard 5V relay modules are ACTIVE LOW
// digitalWrite(pin, LOW) = relay ON (closes NO / opens NC)
// digitalWrite(pin, HIGH) = relay OFF (opens NO / closes NC)
// Pulse duration: 500ms (contactor self-latches via AUX)

#define PULSE_DURATION 500  // ms — both START and STOP pulses

// ============================================
// DEFAULT PROTECTION THRESHOLDS
// (all configurable via web UI)
// ============================================

// Overcurrent (two-stage)
extern float OC_RUNNING;        // 12.0A — running threshold
extern float OC_START_INSTANT;  // 50.0A — instantaneous during start (first 3s)
extern unsigned long OC_DELAY;  // 5000ms — delay for running overcurrent

// Dry-run (current AND power)
extern float DRYRUN_CURRENT;    // 4.0A
extern float DRYRUN_POWER;      // 500.0W
extern unsigned long DRYRUN_DELAY;      // 15000ms
extern unsigned long DRYRUN_ACTIVATION; // 60000ms — wait after pump start before checking

// Voltage (three-zone)
extern float VOLT_OVER_RUN;     // 250V — running over-voltage
extern float VOLT_UNDER_RUN;    // 190V — running under-voltage
extern float VOLT_WARN;         // 250V — pre-start warning
extern float VOLT_CRITICAL;     // 280V — pre-start critical (blocks start)
extern unsigned long VOLTAGE_DELAY;     // 3000ms — running voltage trip delay
extern unsigned long VOLTAGE_LOCKOUT;   // 300000ms — 5min lockout after voltage trip

// Start failure detection
extern float START_SUCCESS_CURRENT;     // 2.0A — min current for successful start
extern unsigned long START_VERIFY_DELAY; // 1000ms — wait after pulse before checking
extern unsigned long START_FAIL_BLOCK;   // 30000ms — block retry after start failure

// Timing protection
extern unsigned long MIN_RUN_TIME;      // 30000ms
extern unsigned long MIN_OFF_TIME;      // 60000ms

// Auto-retry
extern unsigned long AUTORETRY_DELAY;   // 300000ms (5 minutes)
extern uint8_t MAX_RETRIES;             // 3
extern uint8_t MAX_FAST_FAULTS;         // 3 — consecutive fast faults = permanent lockout
extern unsigned long FAST_FAULT_WINDOW; // 10000ms — fault within this = fast fault

// PZEM communication
extern unsigned long PZEM_READ_INTERVAL_RUNNING;  // 1000ms
extern unsigned long PZEM_READ_INTERVAL_OFF;      // 5000ms
#define PZEM_TIMEOUT_MS 500     // per read attempt
#define PZEM_MAX_RETRIES 3      // before declaring fault
#define PZEM_BAUD 9600

// Logging intervals
extern unsigned long LOG_INTERVAL_RUNNING;  // 10000ms
extern unsigned long LOG_INTERVAL_OFF;      // 60000ms

// Network
#define AP_SSID "motorESP"
#define MDNS_HOSTNAME "motorESP"

// Reverse tunnel (CGNAT traversal): ESP dials OUT to the relay on the VM,
// public HTTP arrives over that persistent socket. Numeric IP only — the
// ESP resolves no DNS for the tunnel. Empty string disables the tunnel.
#define TUNNEL_HOST "68.233.98.190"   // VM public IP (relay: ESP tunnel :9000)
#define TUNNEL_PORT 9000

// OTA
#define FIRMWARE_VERSION "1.0.0"

// ============================================
// HELPER: Default initializers
// ============================================
void initConfigDefaults();

#endif
