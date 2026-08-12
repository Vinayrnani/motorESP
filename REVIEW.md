# motorESP - Requirements Review
# Review Date: 2026-08-11
# Status: ALL ISSUES RESOLVED — See Section 5 for resolution mapping

---

## EXECUTIVE SUMMARY

All 17 issues from the deep 4-way review have been resolved and incorporated
into REQUIREMENTS.md. This document retains the original findings for audit
trail. Each issue's resolution is mapped in Section 5.

---

## 1. HARDWARE / PIN LEVEL ISSUES

### 1.1 GPIO0 (D3) Used for Relay — Boot Failure Risk
**Status:** RESOLVED in REQUIREMENTS.md Section 2.4
**Resolution:** CH3 moved from D3 (GPIO0) to D4 (GPIO2). GPIO0 not used.

### 1.2 Relay Module 3.3V Logic Compatibility
**Status:** RESOLVED in REQUIREMENTS.md Section 2.6
**Resolution:** Documented as risk. User must test before deploy. If issues, switch to JD-VCC modules.

### 1.3 Relay CH2 (NC) Breaking Inductive Load
**Status:** RESOLVED in REQUIREMENTS.md Section 2.5
**Resolution:** Documented. Contactor coil ~50-100mA at 240VAC. Add snubber recommended.

### 1.4 SoftwareSerial Pin Selection
**Status:** RESOLVED in REQUIREMENTS.md Section 2.4
**Resolution:** D5/D6 kept. 9600 baud reliable with SoftwareSerial on ESP8266.

### 1.5 PZEM 004T Power Supply Noise
**Status:** RESOLVED in REQUIREMENTS.md Section 3.3
**Resolution:** 100µF capacitor on PZEM VCC recommended. Power from HLK-PM01 5V.

---

## 2. PROTECTION LOGIC ISSUES

### 2.1 Overcurrent During Motor Starting
**Status:** RESOLVED in REQUIREMENTS.md Section 4.1
**Resolution:** Two-stage overcurrent: 50A instantaneous (start) + 12A delayed 5s (running).

### 2.2 Contactor Failed to Latch Detection
**Status:** RESOLVED in REQUIREMENTS.md Section 4.5
**Resolution:** Check PZEM current 1s after start pulse. < 2A = start failure.

### 2.3 Dry-Run Detection False Positives
**Status:** RESOLVED in REQUIREMENTS.md Section 4.2
**Resolution:** Uses current AND power. 15s delay. 60s activation delay after start.

### 2.4 Voltage Protection Only Works While Running
**Status:** RESOLVED in REQUIREMENTS.md Section 4.3
**Resolution:** PZEM moved BEFORE contactor. Always measures voltage. Pre-start warning/critical levels added.

### 2.5 No Minimum Run Time / Minimum Off Time
**Status:** RESOLVED in REQUIREMENTS.md Section 4.6
**Resolution:** Min run 30s, min off 60s. Both configurable.

### 2.6 Protection Trip State Not Persisted
**Status:** RESOLVED in REQUIREMENTS.md Section 4.8
**Resolution:** Trip state saved to EEPROM immediately. Checked on boot.

### 2.7 Auto-Retry Without Current Monitoring
**Status:** RESOLVED in REQUIREMENTS.md Section 4.7
**Resolution:** Fast fault detection — 3 faults within 10s = permanent lockout.

---

## 3. FRAMEWORK COMPATIBILITY ISSUES

### 3.1 LogEntry Struct Size Change Breaks Flash Layout
**Status:** RESOLVED in REQUIREMENTS.md Section 6.1-6.3
**Resolution:** 18-byte struct. Meta marker = voltage 0xFFFF. All code adapted.

### 3.2 DeviceSettings Struct Needs Expansion
**Status:** RESOLVED in REQUIREMENTS.md Section 9
**Resolution:** Redesigned as ~40-byte struct with all new fields.

### 3.3 SAT Architecture Assumptions
**Status:** RESOLVED in REQUIREMENTS.md Section 15
**Resolution:** Runtime hours displayed instead of days. SAT still used for timestamps.

### 3.4 Web UI Page Size vs Long-Distance Loading
**Status:** RESOLVED in REQUIREMENTS.md Section 7
**Resolution:** Separate lightweight pages. Charts only on dashboard.

### 3.5 PZEM Library Memory Usage
**Status:** RESOLVED in REQUIREMENTS.md Section 7.6
**Resolution:** Heap monitoring in status endpoint. Alert if low.

---

## 4. REAL-WORLD TESTING ISSUES

### 4.1 Power Restoration After Outage
**Status:** RESOLVED in REQUIREMENTS.md Section 4.9
**Resolution:** Always require manual start after power restoration.

### 4.2 EMI from Motor Affecting WiFi
**Status:** RESOLVED in REQUIREMENTS.md Section 13
**Resolution:** Documented. Test with motor running. Ferrite cores if needed.

### 4.3 PZEM CT Clamp Orientation
**Status:** RESOLVED in REQUIREMENTS.md Section 3.4
**Resolution:** Take absolute value of current in software. Setup check documented.

### 4.4 Contactor Coil Voltage
**Status:** RESOLVED in REQUIREMENTS.md Section 2.1
**Resolution:** Confirmed 240VAC. Relay contacts rated for 240VAC 10A.

### 4.5 PZEM Communication Timeout
**Status:** RESOLVED in REQUIREMENTS.md Section 3.3 + 4.4
**Resolution:** 500ms timeout, 3 retries, fail-safe trip OFF.

---

## 5. ISSUE-TO-RESOLUTION MAPPING

| Review Issue | REQUIREMENTS.md Section | Status |
|--------------|------------------------|--------|
| 1.1 GPIO0 boot risk | 2.4 Pin Mapping | RESOLVED |
| 1.2 3.3V relay compat | 2.6 Compatibility Note | RESOLVED |
| 1.3 NC inductive break | 2.5 Relay Logic | RESOLVED |
| 1.4 SoftwareSerial pins | 2.4 Pin Mapping | RESOLVED |
| 1.5 PZEM power noise | 3.3 PZEM Notes | RESOLVED |
| 2.1 Overcurrent start | 4.1 Two-Stage OC | RESOLVED |
| 2.2 Contactor latch fail | 4.5 Start Failure Det. | RESOLVED |
| 2.3 Dry-run false pos | 4.2 Dry-Run Logic | RESOLVED |
| 2.4 Voltage only running | 4.3 Three-Zone Voltage | RESOLVED |
| 2.5 Min run/off time | 4.6 Timing Protection | RESOLVED |
| 2.6 Trip not persisted | 4.8 Trip Persistence | RESOLVED |
| 2.7 Auto-retry monitor | 4.7 Fast Fault Detect | RESOLVED |
| 3.1 LogEntry struct | 6.1-6.3 Log Format | RESOLVED |
| 3.2 DeviceSettings | 9 EEPROM Layout | RESOLVED |
| 3.3 SAT assumptions | 15 Notes | RESOLVED |
| 3.4 Web UI size | 7 Web UI Pages | RESOLVED |
| 3.5 PZEM memory | 7.6 + 3.3 | RESOLVED |
| 4.1 Power restoration | 4.9 Power Restore Safe | RESOLVED |
| 4.2 EMI WiFi | 13 Safety | RESOLVED |
| 4.3 CT orientation | 3.4 CT Direction | RESOLVED |
| 4.4 Coil voltage | 2.1 Components | RESOLVED |
| 4.5 PZEM timeout | 3.3 + 4.4 | RESOLVED |

---

## 6. SITE-SPECIFIC DATA INCORPORATED

| Measurement | Value | Source |
|-------------|-------|--------|
| No-load voltage | 290V | User measured |
| Loaded voltage | 240V | User measured |
| Voltage drop | 50V | Calculated |
| Running current | 9.6A | User measured |
| Motor power | 1.5 | User specified |

## 6. ADDITIONAL DECISIONS LOG

| Decision | Status | REQUIREMENTS.md Section |
|----------|--------|------------------------|
| GPIO0 not used — CH3 moved to D4 | RESOLVED | 2.4 |
| PZEM before contactor (always monitors) | RESOLVED | 3.1 |
| 290V no-load voltage data incorporated | RESOLVED | 2.3 |
| Voltage protection: warning + critical levels | RESOLVED | 4.3 |
| Two-stage overcurrent (50A + 12A) | RESOLVED | 4.1 |
| Contactor latch verification via PZEM | RESOLVED | 4.5 |
| Power restoration requires manual start | RESOLVED | 4.9 |
| Trip state persisted to EEPROM | RESOLVED | 4.8 |
| Min run/off timing (30s/60s) | RESOLVED | 4.6 |
| Dry-run via current AND power | RESOLVED | 4.2 |
| Voltage trip lockout (5min) | RESOLVED | 4.3 |
| Auto-retry with fast-fault detection | RESOLVED | 4.7 |
| PZEM timeout (500ms, 3 retries, fail-safe) | RESOLVED | 3.3 + 4.4 |
| LogEntry 11-byte structure | RESOLVED | 6.1 |
| Delta energy approach | RESOLVED | 6.7 |
| DeviceSettings ~40-byte struct | RESOLVED | 9 |
| Runtime hours (not days) | RESOLVED | 15 |
| PF meta markers (0xFE + 0xFF) | RESOLVED | 6.3 |
| Control at `/`, Dashboard at `/dashboard` | RESOLVED | 7.2 |
| Dexie.js client-side storage | RESOLVED | 7.7 |
| Polling 1-5s configurable | RESOLVED | 7.9 |
| WiFi independent of pump state | RESOLVED | 10.1 |
| Manual start detection via PZEM | RESOLVED | 15 |
| EEPROM write strategy — Option B (debounced) | RESOLVED | 4.8 |

---

*All issues resolved. REQUIREMENTS.md is the single source of truth.*
*Ready for implementation — say GO to begin.*
