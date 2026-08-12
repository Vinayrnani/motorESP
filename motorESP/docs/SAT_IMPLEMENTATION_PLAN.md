# SAT Implementation Plan

## Overview
Implement the SAT (Synchronised Absolute Timestamp) architecture per `SAT_ARCHITECTURE.md`. Replaces the naive `elapsedSeconds` counter with a boot-timestamp table synced between ESP and browser Dexie DB.

---

## Phase 1: ESP8266 Firmware (`eggubator.ino`)

### 1a — EEPROM constants + boot table structs
- Add EEPROM addresses for `LAST_KNOWN_BOOT_ID` (15) and `LAST_KNOWN_START_UNIX` (16-19)
- Add `BootTimestamp` struct, dynamic `bootTable` pointer, `bootTableCount`

### 1b — `prepareBootTable()`
- Scan flash sectors using existing `bootIndex[]` from logging.cpp
- For each boot: read last log entry's `timeSec` = boot duration
- Build `bootTable` dynamically via `malloc`
- Sort by `bootId`
- Read EEPROM anchor, calculate `startUnix` chain bidirectionally
- Add current boot entry (estimated)

### 1c — Derived time functions
- `getBootUptime()` → `millis() / 1000`
- `getElapsedSeconds()` → `bootTable[currentBootId].startUnix + getBootUptime() - startTimestamp`
- `getCurrentDay()` → `getElapsedSeconds() / 86400`
- Remove `elapsedSeconds++` from `loop()`
- Remove 3-hour EEPROM save of `elapsedSeconds`
- Remove `EEPROM_ELAPSED_ADDR` reads/writes

### 1d — `/timestamps` endpoints
- `GET /timestamps`: returns bootTable + currentBootId + currentStartUnix + bootUptimeSec
- `PUT /timestamps`: receives boot history, replaces bootTable, updates EEPROM anchor, calculates drift

### 1e — Register endpoints + update handlers
- Register `GET /timestamps` and `PUT /timestamps` in `setup()`
- `/status` & `/data`: use derived functions
- `/settings/api` actions: update `newBatch`, `syncTime` for SAT
- `saveSettings()`: remove `EEPROM_ELAPSED_ADDR` writes

---

## Phase 2: Browser/Dexie (`web_ui.h`)

### 2a — Dexie schema v3
- Add `bootTimestamps: 'bootId, startUnix, duration'` table

### 2b — SAT sync on dashboard load
1. `GET /timestamps` from ESP
2. Query Dexie `bootTimestamps` + derive history from logs
3. Merge (newer-wins conflict resolution)
4. `PUT /timestamps` reconciled timeline to ESP
5. Save to Dexie `bootTimestamps`

### 2c — Update `decodeLogs()`
- Use bootTimestamps for accurate absolute timestamps
- Fallback to estimation if bootId unknown

### 2d — Update `checkAutoReset()`
- Use SAT `/timestamps` endpoints instead of crude `syncTime` action

---

## Phase 3: Deploy & Test

### 3a — Compile
```bash
arduino-cli compile -b esp8266:esp8266:nodemcu eggubator.ino
```

### 3b — Deploy to ESP at http://10.240.149.72
```bash
./deploy.sh
```

### 3c — Test
- Verify boot table builds correctly
- Verify GET/PUT /timestamps
- Verify elapsed time is accurate
- Verify browser sync workflow
- Verify multi-boot scenarios
