# motorESP - Submersible Pump Automation Controller
# Requirements Specification (Consolidated)
# Date: 2026-08-11
# Based on: eggubator framework (Vinayrnani/eggubator)
# Status: APPROVED — All review issues resolved

---

## 1. PROJECT OVERVIEW

Transform the eggubator ESP8266 framework into a submersible pump automation
controller with PZEM 004T 100A power meter for monitoring, protection, and
control of a single-phase submersible pump.

### Design Principles
- Keep eggubator core framework (WiFi, SAT, Flash Logging, Web UI, OTA)
- Replace all incubator logic with pump control + protection
- Maintain existing physical push-button control panel functionality
- Safety-first: protection overrides everything
- Configurable thresholds and timing via web UI

---

## 2. HARDWARE SPECIFICATION

### 2.1 Components

| Component | Specification | Purpose |
|-----------|---------------|---------|
| MCU | ESP8266 NodeMCU | Main controller |
| Power Meter | PZEM 004T 100A | Voltage, current, power, energy monitoring |
| Relay Module | Standard 5V 240VAC 10A, 4-channel | Pump start/stop + spares |
| Power Supply | HLK-PM01 (240VAC → 5V DC), 1A min | Powers ESP + PZEM |
| Contactor | Existing panel contactor, 240VAC coil, AUX latching | Switches motor power |
| Push Buttons | Existing GREEN (NO) start, RED (NC) stop | Manual control |
| Capacitors | Existing starting + running capacitors | Motor phase shift |

### 2.2 Pump Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Motor Power | 1.5 HP (1119W) | |
| Voltage | 240VAC Single Phase | |
| Running Current | 9.6A | Measured |
| Starting Current | 28-58A | Estimated 3-6x running |
| Power Factor | ~0.5-0.7 | Typical single-phase motor |

### 2.3 Site Voltage Data (MEASURED — Critical for Protection Logic)

| Condition | Voltage | Notes |
|-----------|---------|-------|
| No load (pump OFF) | 290V | Open circuit, no wiring drop |
| Under load (pump ON) | 240V | 50V drop through wiring |
| Voltage at transformer | Likely set high | Explains 290V no-load |

**Implication:** Overvoltage threshold must account for 290V no-load. PZEM max readable = 260V (anything above reads as 260V). Pre-start voltage check uses WARNING + CRITICAL levels, not trip.

### 2.4 Pin Mapping (REVISED — GPIO0 removed)

| ESP Pin | GPIO | Function | Connection | Notes |
|---------|------|----------|------------|-------|
| D1 | GPIO5 | Relay CH1 (NO) | START #1 — series with CH3 | Safe pin |
| D2 | GPIO4 | Relay CH2 (NC) | STOP — breaks coil circuit | Safe pin |
| D4 | GPIO2 | Relay CH3 (NO) | START #2 — series with CH1 | Built-in LED blinks |
| D7 | GPIO13 | Relay CH4 | Spare | Safe pin |
| D5 | GPIO14 | SoftwareSerial TX | PZEM RX | |
| D6 | GPIO12 | SoftwareSerial RX | PZEM TX | |

**GPIO0 (D3) NOT USED** — Boot strap pin, must be HIGH at boot. Risk of entering UART download mode if LOW.

### 2.5 Relay Control Logic

**Both START relays (CH1 + CH3) wired in series for safety:**
- Both must close to energize contactor coil
- Single relay failure cannot start pump accidentally

**STOP relay (CH2) NC in series with coil circuit:**
- NC contact CLOSED = normal state = coil circuit complete
- NC contact OPEN = stop = coil de-energized

**All relays PULSE type (500ms):**
- Contactor self-latches via AUX contact
- Continuous relay activation unnecessary and wasteful

| Action | Relays | Pulse Duration | What Happens |
|--------|--------|----------------|--------------|
| START | CH1 + CH3 close | 500ms | Contactor energizes, AUX latches |
| STOP | CH2 opens | 500ms | Contactor de-energizes |
| IDLE | All default | — | Manual buttons work normally |

### 2.6 Relay Module Compatibility Note

⚠️ Standard 5V relay modules may not trigger reliably with 3.3V ESP GPIO.
- **Risk:** Relay doesn't close → pump won't start
- **Mitigation:** Test before deploy. If issues, switch to JD-VCC separable modules.

---

## 3. PZEM 004T WIRING (BEFORE CONTACTOR — Always Powered)

### 3.1 Measurement Configuration

The PZEM voltage input (L&N) connects to the **supply side** (before contactor),
so it **always measures mains voltage** — even when pump is OFF.

| PZEM Terminal | Connection | Notes |
|---------------|------------|-------|
| L (Line) | Supply side R terminal (before contactor) | Always live |
| N (Neutral) | Supply N bus | Always connected |
| CT | Clamps around LIVE wire to contactor | Current measurement |
| TX | ESP D6 (GPIO12) | SoftwareSerial RX |
| RX | ESP D5 (GPIO14) | SoftwareSerial TX |
| VCC | ESP 5V (from HLK-PM01) | Always powered |
| GND | Common GND | Always connected |

### 3.2 Measurements

| Measurement | Range | Resolution | Logged |
|-------------|-------|------------|--------|
| Voltage | 80-260VAC | 0.1V | Yes |
| Current | 0.02-100A | 0.01A | Yes |
| Active Power | 0-26kW | 0.1W | Calculated (V×I×PF) |
| Energy | 0-9999 kWh | 1 Wh | Delta per entry |
| Frequency | 45-65Hz | 0.1Hz | Via /status only |
| Power Factor | 0-1.00 | 0.01 | Yes |

### 3.3 PZEM V4.0 Specifications

- **Protocol:** Modbus-RTU, 9600 baud (identical to V3.0)
- **Registers:** Same as V3.0 — use `PZEM004Tv30` library
- **Energy register:** 0-9999 kWh range, wraps at 9999 kWh
- **Energy retention:** Likely non-volatile (retains across power loss)
- **Energy reset:** Dedicated command available (`resetEnergy()`)
- **Max readable voltage:** 260V (clamps anything higher, e.g. 290V reads as 260V)

### 3.4 PZEM Operational Notes

- PZEM always powered → energy register persists across pump cycles
- Max readable voltage = 260V (clamps higher values)
- Current = 0 when pump OFF (no current through CT)
- Power = 0 when pump OFF
- **500ms timeout per read, 3 retries** before declaring PZEM fault
- Persistent PZEM failure → **fail-safe trip OFF**
- **Library:** `PZEM004Tv30` by mandulaj (works with V4.0)

### 3.4 CT Clamp Direction

- CT clamp has direction (P1/P2 marking)
- If installed backwards: current/power factor sign inverted
- **Software fix:** Take absolute value of current in code
- **Setup check:** "If power factor negative, reverse CT direction"

---

## 4. PROTECTION SYSTEM

### 4.1 Overcurrent Protection (Two-Stage)

| Stage | Threshold | Timing | Catches |
|-------|-----------|--------|---------|
| Stage 1 | >50A | Instantaneous (first 3s of start) | Short circuit, motor jam during start |
| Stage 2 | >12A | 5s continuous (configurable 1-60s) | Running overload, bearing failure |

**Stage 2 timer resets if current drops below threshold before delay expires.**

### 4.2 Dry-Run Protection

| Parameter | Default | Range | Notes |
|-----------|---------|-------|-------|
| Current threshold | 4A | 1-20A | |
| Power threshold | 500W | 100-2000W | Combined with current |
| Delay | 15s | 5-60s | Both must be below for full delay |
| Activation delay | 60s after start | 30-120s | Don't check during start surge |

**Logic:** Current < 4A **AND** Power < 500W for continuous 15s → DRY-RUN TRIP

**Why both current AND power:** Prevents false positives from:
- Partially closed valve (low current but normal voltage)
- Voltage sag (higher current, lower power)

### 4.3 Voltage Protection (Three-Zone)

#### Zone A: Pre-Start (Pump OFF, 290V no-load possible)

| Level | Threshold | Action |
|-------|-----------|--------|
| Normal | 190-250V | Pump can start |
| WARNING | 250-280V | Yellow alert on web UI, pump CAN start |
| CRITICAL | >280V | Red alert on web UI, pump BLOCKED |

- Warning threshold: **configurable, default 250V**
- Critical threshold: **configurable, default 280V, must be > warning**
- Critical effectively capped at 260V (PZEM max readable)

#### Zone B: Running (Pump ON, ~240V under load)

| Protection | Threshold | Delay | Action |
|------------|-----------|-------|--------|
| Over-voltage | >250V | 3s | Trip OFF |
| Under-voltage | <190V | 3s | Trip OFF |

#### Zone C: Voltage Trip Lockout

- After voltage trip, block restart for 5 minutes
- Display "VOLTAGE FAULT — START BLOCKED" on web UI
- Prevents start-stop cycling on persistent grid issues

### 4.4 PZEM Fault Protection

- 500ms timeout per read attempt
- 3 consecutive retries before declaring fault
- Persistent fault → **fail-safe trip OFF**
- Display "PZEM FAULT" on web UI

### 4.5 Start Failure Detection

- After 500ms start pulse, wait 1 second
- Check PZEM current:
  - Current > 2A → contactor latched ✓
  - Current < 2A → contactor didn't latch → START FAILURE
- Block retry for 30s after start failure
- Display "START FAILURE — CHECK CONTACTOR" on web UI
- Log failure event

### 4.6 Timing Protection

| Parameter | Default | Range | Notes |
|-----------|---------|-------|-------|
| Min run time | 30s | 10-300s | Once started, must run this long |
| Min off time | 60s | 10-600s | Once stopped, must wait before restart |

Prevents rapid cycling that damages motor and contactor.

### 4.7 Trip Behavior (Per Protection, Configurable)

| Mode | Behavior |
|------|----------|
| **LOCKOUT** | Pump stays OFF until manual reset via web UI |
| **AUTO-RETRY** | Wait configurable delay, attempt restart, max N retries |

**Auto-retry with fault persistence:**
- Monitor 10s after each retry
- If fault recurs within 10s → count as "fast fault"
- 3 fast faults = permanent lockout (requires manual reset)
- Display "PERMANENT FAULT — MANUAL RESET REQUIRED"

| Parameter | Default | Range |
|-----------|---------|-------|
| Auto-retry delay | 300s | 60-3600s |
| Max retries | 3 | 1-10 |
| Fast fault window | 10s | Fixed |

### 4.8 Trip State Persistence

- All active trips saved to EEPROM **debounced** (Option B)
- EEPROM written only when the trip type changes (first trip in a sequence)
- Same trip type persisting across retries/reboots → no rewrite
- Minimizes flash wear while guaranteeing fault survives power loss
- On boot, EEPROM checked for persisted trip state
- If trip active → block pump start until manual reset
- Prevents pump restart after ESP reboot

### 4.9 Power Restoration Safety

- After power restoration, **ALWAYS require manual start**
- "POWER RESTORED — MANUAL START REQUIRED" on web UI
- Block automatic start regardless of mode
- Physical buttons always work

### 4.10 Protection Priority

```
Protection Trip > OFF Mode > Manual/Auto
```

- Protection ALWAYS overrides manual buttons and web commands
- Even if someone holds GREEN button, protection trip stops pump

---

## 5. PUMP CONTROL MODES

### 5.1 Mode Selection

| Mode | Description |
|------|-------------|
| **OFF** | Pump disabled (kill switch) |
| **MANUAL** | User turns pump ON/OFF via web UI |
| **AUTO** | Time-of-day schedule (placeholder — no RTC) |

### 5.2 Manual Control

- Web UI: ON/OFF buttons
- Physical buttons: Always work (GREEN/RED)
- ESP relay pulses: 500ms for start/stop

### 5.3 Automatic Control (Placeholder)

- Time-of-day scheduling (future feature)
- Uses SAT elapsed time as reference (no RTC)
- Schedule stored in EEPROM
- Format: ON time, OFF time, repeat daily

### 5.4 Pump State Machine

```
OFF → STARTING (pulse active) → RUNNING → STOPPING (pulse) → OFF
                   ↓                                ↓
               TRIPPED ←──────────────────────────┘
```

- STARTING: 500ms pulse, then verify contactor latch via PZEM
- RUNNING: Normal operation, protection active
- STOPPING: 500ms NC break, then idle
- TRIPPED: All relays safe, wait for reset or auto-retry

---

## 6. FLASH LOGGING

### 6.1 Log Entry Format (11 bytes)

```cpp
struct __attribute__((packed)) LogEntry {
  uint32_t timeSec;     // 4 bytes - seconds since boot
  uint8_t  voltage;     // 1 byte  - offset from 200V (40=240V, 90=290V)
  uint16_t current;     // 2 bytes - A × 10 (96 = 9.6A, 580 = 58.0A)
  uint8_t  energyDelta; // 1 byte  - Wh since last entry (0-255)
  uint8_t  pf;          // 1 byte  - PF × 100 (65 = 0.65), 0xFF = meta marker
  uint8_t  states;      // 1 byte  - bit flags (see below)
  uint8_t  bootId;      // 1 byte  - boot session ID
};
```

### 6.2 State Bits

| Bit | State |
|-----|-------|
| 0 | Pump running |
| 1 | Overcurrent trip |
| 2 | Dry-run trip |
| 3 | Over-voltage trip |
| 4 | Under-voltage trip |
| 5 | Auto mode active |
| 6 | PZEM fault |
| 7 | Start failure |

### 6.3 Meta Markers

Two marker types (matching eggubator's `logging.cpp` port):

| Marker | Value | Purpose |
|--------|-------|---------|
| `META_SECTOR_POINTER` | `pf = 0xFE` | First entry of each sector (written on erase) |
| `META_CORRECTION` | `pf = 0xFF` | SAT timing correction (invisible to browser) |

- Real PF ranges 0-100 (0.00-1.00), so both 0xFE (254) and 0xFF (255) are physically impossible → safe markers
- All flash code uses `entry.pf != 0xFE && entry.pf != 0xFF` to identify valid data entries
- Browser skips entries with `pf >= 0xFE` when parsing logs
- Voltage field preserved for data (not used as marker)

### 6.4 Storage Capacity

| Parameter | Value |
|-----------|-------|
| Sector size | 4096 bytes |
| Entries per sector | 372 (4096 / 11) |
| Total sectors | 256 |
| Total entries | 95,232 |
| Flash address | 0x200000 |

### 6.5 Data Encoding

| Field | Encoding | Range |
|-------|----------|-------|
| Voltage | offset from 200V | 200-455V (real: 240-290V) |
| Current | A × 10 | 0-6553.5A |
| Energy | Wh delta since last entry | 0-255 Wh |
| Power | **Calculated:** V × I × PF | Not stored (saves 2 bytes) |
| PF | PF × 100 | 0.00-1.00 |

### 6.6 Logging Intervals

| Condition | Interval | Range |
|-----------|----------|-------|
| Pump running | 10s | 5-60s |
| Pump OFF | 60s | 30-600s |

**Always log on state change** (start, stop, trip, mode change).

### 6.7 Energy Tracking (Delta Approach)

- Each entry stores **Wh since last entry**, not cumulative total
- Sum of all deltas = total energy consumed
- **Survives power outages:** deltas preserved in flash even if PZEM register resets
- **Survives PZEM wrap:** PZEM register wraps at 9999 kWh, deltas unaffected
- **Periodic totals:** sum any range of entries (daily, weekly, monthly)
- Running total maintained in RAM for fast /status display

---

## 7. WEB UI

### 7.1 Design Philosophy

- **Lightweight pages** for long-distance loading (slow connections)
- **Separate pages** for dashboard and controls
- **Minimal JavaScript** — no heavy frameworks on device side
- **Embedded assets** — Chart.js, Dexie.js served from ESP flash (no CDN)
- Load charts only on dashboard page

### 7.2 Pages (Route Structure)

| Page | URL | Content | Weight Target |
|------|-----|---------|---------------|
| Control | `/` | ON/OFF buttons, mode, status, quick stats | <20KB |
| Dashboard | `/dashboard` | Large numerics, charts, protection state | <50KB |
| Settings | `/settings` | Thresholds, intervals, trip behavior | <30KB |
| Data | `/data` | Historical data table | <20KB |

### 7.3 Control Page (`/`) — Primary Interface

- Big ON / OFF buttons
- Mode selector: OFF / MANUAL / AUTO
- Current status: Running, Stopped, Tripped (with reason)
- Trip reset button (when tripped)
- Power restored indicator
- Quick stats: V, A, W (small display)

### 7.4 Dashboard Page (`/dashboard`) — Detailed View

- Large numeric displays: Voltage, Current, Power, Energy, Frequency, PF
- Status badge: Running / Stopped / Tripped / Warning (color-coded)
- Protection state indicators
- Voltage status: Normal / Warning / Critical
- Chart.js line graphs: power, voltage, current (last 100 points)
- Auto-refresh via `/status` JSON (configurable 1-5s polling)

### 7.5 Settings Page (`/settings`) — Configuration

- Overcurrent threshold (A)
- Dry-run current threshold (A)
- Dry-run power threshold (W)
- Over-voltage running threshold (V)
- Under-voltage running threshold (V)
- Pre-start warning voltage (V)
- Pre-start critical voltage (V)
- PZEM read interval (running/OFF)
- Log interval (running/OFF)
- Polling interval (1-5s)
- Trip behavior per protection (lockout / auto-retry)
- Auto-retry delay
- Max retries
- Min run time
- Min off time

### 7.6 Data Page (`/data`) — History

- Paginated table of recent log entries
- "Load more" fetches older entries via /data?boot=X&time=Y&count=50

### 7.7 Client-Side Storage (Dexie.js)

- Dexie.js (IndexedDB) stores all fetched log entries client-side
- Survives browser refresh / reboot
- Renders charts from local data
- SAT sync on page load (GET/PUT /timestamps)
- Same pattern as eggubator

### 7.8 SAT Sync Flow (On Page Load)

1. GET /timestamps → ESP boot table
2. Load Dexie history from IndexedDB
3. PUT /timestamps → merge & correct timeline
4. GET /data?boot=X&time=Y&count=200 → fetch logs
5. Decode 11-byte hex → store in Dexie → render charts

### 7.9 Polling

- Dashboard auto-refreshes via GET /status
- Polling interval: configurable 1-5 seconds (default 5s)
- Simple HTTP polling (not SSE)

### 7.10 Web Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Control page (primary) |
| `/dashboard` | GET | Dashboard with charts |
| `/status` | GET | JSON: V, A, W, kWh, Hz, PF, pump state, protection state, heap, uptime |
| `/data` | GET | JSON: logged data with pagination |
| `/control?action=start` | GET | Start pump |
| `/control?action=stop` | GET | Stop pump |
| `/control?action=reset` | GET | Reset protection trip |
| `/settings` | GET | Settings page |
| `/settings/api` | GET/POST | Settings API (read/write all config) |
| `/timestamps` | GET/PUT | SAT boot sync |
| `/ota/check` | GET | Check firmware version vs GitHub |
| `/ota/apply` | POST | Trigger OTA update |
| `/reboot` | GET | Reboot device |
| `/clear` | GET | Clear flash logs, reset boot ID |
| `/update` | POST | OTA upload endpoint (ESP8266HTTPUpdateServer) |

---

## 8. FRAMEWORK COMPONENTS (from eggubator)

### 8.1 Keep and Modify

| File | Changes |
|------|---------|
| `wifi_manager.h` | Rename AP_SSID to "motorESP", hostname to "motorESP" |
| `sat_manager.h/.cpp` | Keep as-is (generic boot tracking) |
| `logging.h/.cpp` | New 11-byte LogEntry, meta marker = 0xFF (PF field), delta energy |
| `web_ui.h` | Complete rewrite for pump dashboard |
| `embedded_assets.h` | Keep same (Chart.js, Dexie.js, etc.) |
| `updates.h` | Update URLs to motorESP repo |
| `deploy.sh` | Update mDNS name to "motorESP" |
| `ntp_sync.h` | Keep as-is |

### 8.2 Remove

| File | Reason |
|------|--------|
| `dht_sensor.h` | Replaced by PZEM sensor |
| `config.h` | Redesigned for pump pins/thresholds |
| `eggubator.ino` | Replaced by motorESP.ino |

### 8.3 New Files

| File | Purpose |
|------|---------|
| `pzem_sensor.h` | PZEM 004T interface with mock mode |
| `motorESP.ino` | Main sketch |

### 8.4 Keep Unchanged

| File | Purpose |
|------|---------|
| `rel.sh` | Release script |
| `flash.sh` | USB flash script |
| `sector_viewer.h` | Flash hex editor debug tool |
| `embedded_assets.h` | Gzipped Chart.js, Dexie.js, etc. |

---

## 9. EEPROM LAYOUT

| Region | Address | Magic | Size | Purpose |
|--------|---------|-------|------|---------|
| SAT drift | 15-23 | — | 9 bytes | SAT timing anchor |
| DeviceSettings | 40 | 0xAB | ~36 bytes | Pump settings, thresholds, trip state |
| WiFi credentials | 200 | 0xAC | 98 bytes | WiFi SSID/password |

### DeviceSettings Struct

```cpp
struct DeviceSettings {
  uint8_t magic;                    // 0xAB
  uint8_t pumpMode;                 // 0=OFF, 1=MANUAL, 2=AUTO
  uint8_t activeTrips;              // bit flags for persisted trips
  uint8_t tripBehavior;             // bit flags: per-protection lockout/retry
  uint16_t overcurrentThreshold;    // A * 10 (120 = 12.0A)
  uint16_t dryRunCurrentThreshold;  // A * 10 (40 = 4.0A)
  uint16_t dryRunPowerThreshold;    // W (500 = 500W)
  uint16_t overVoltageThreshold;    // V (250)
  uint16_t underVoltageThreshold;   // V (190)
  uint16_t preStartWarnVoltage;     // V (250)
  uint16_t preStartCriticalVoltage; // V (280)
  uint16_t pzemReadInterval;        // ms (1000)
  uint16_t logInterval;             // ms (10000)
  uint16_t logIntervalOff;          // ms (60000)
  uint16_t autoRetryDelay;          // seconds (300)
  uint8_t maxRetries;               // (3)
  uint8_t retryCount;               // persisted retry count
  uint16_t minRunTime;              // seconds (30)
  uint16_t minOffTime;              // seconds (60)
  uint16_t ocDelay;                 // ms (5000)
  uint16_t dryRunDelay;             // ms (15000)
  uint16_t voltageDelay;            // ms (3000)
  uint8_t ocStartInstant;           // A (50)
  uint32_t batchStartUnix;          // SAT anchor
  uint8_t fastFaultCount;           // consecutive fast faults
  // Total: ~40 bytes
};
```

---

## 10. NETWORK CONFIGURATION

| Parameter | Value |
|-----------|-------|
| mDNS hostname | motorESP.local |
| Static IP | Ends in .72 (auto-detect subnet from DHCP) |
| AP mode SSID | motorESP |
| AP fallback | After 20s of failed WiFi connection |

### 10.1 WiFi Reconnection Behavior

- **Pump state independent of WiFi** — if WiFi drops, pump continues running
- ESP reconnects to WiFi silently when available
- Flash logging continues during WiFi outage
- Only web UI affected during outage (can't view/control remotely)
- No fail-safe stop on comms loss (pump state preserved by contactor AUX latch)

---

## 11. BUILD & DEPLOY

### 11.1 Compile

```bash
arduino-cli compile -b esp8266:esp8266:nodemcu motorESP.ino
```

### 11.2 Deploy OTA

```bash
./deploy.sh    # compile + find IP via mDNS + OTA upload
```

### 11.3 Release

```bash
./rel.sh [VERSION]    # bump + compile + commit + tag + GitHub Release + OTA
```

### 11.4 Flash USB

```bash
./flash.sh    # auto-detect /dev/ttyUSB* + esptool.py
```

---

## 12. SAFETY CONSIDERATIONS

1. **Protection always overrides** — no software disable for protection
2. **Relay defaults are safe** — CH1/CH3 open (no start), CH2 NC closed (allows stop)
3. **Physical buttons always work** — ESP failure doesn't prevent manual control
4. **Two start relays in series** — single failure cannot accidentally start pump
5. **Contactor coil circuit** — ESP relay CH2 NC breaks coil on trip
6. **PZEM isolation** — PZEM module has built-in optocoupler/isolation
7. **Voltage always monitored** — PZEM before contactor enables pre-start check
8. **GPIO0 not used** — eliminates boot failure risk from relay loading
9. **Trip state persisted** — power cycle doesn't clear protection state
10. **Power restoration** — always requires manual start

---

## 13. TESTING CHECKLIST

Before deployment, verify:

- [ ] ESP boots reliably with relay module connected (100 power cycles)
- [ ] Relay triggers reliably with 3.3V logic input
- [ ] Contactor latches and ESP detects latch via PZEM
- [ ] Overcurrent Stage 1 trips at >50A during start
- [ ] Overcurrent Stage 2 trips at >12A after 5s delay
- [ ] Dry-run detection works (current AND power below threshold)
- [ ] Voltage warning displays at >250V pre-start
- [ ] Voltage critical blocks start at >280V pre-start
- [ ] Voltage running trip at >250V or <190V after 3s
- [ ] PZEM fault triggers fail-safe trip
- [ ] Start failure detection works (no current after pulse)
- [ ] Min run time enforced (30s)
- [ ] Min off time enforced (60s)
- [ ] Auto-retry works (transient fault → success)
- [ ] Fast fault detection works (3 fast faults = lockout)
- [ ] Max retries lockout works
- [ ] Trip state persists across reboot
- [ ] Power restoration requires manual start
- [ ] Manual buttons work regardless of ESP state
- [ ] Protection trip overrides manual buttons
- [ ] WiFi stable during motor starting and running
- [ ] Flash logging works (11-byte entries, 372/sector, verify with sector viewer)
- [ ] OTA update works
- [ ] All settings persist across reboots
- [ ] Mock mode works (all endpoints, state transitions)

---

## 14. IMPLEMENTATION ORDER

1. `config.h` — Pin mapping + protection thresholds
2. `pzem_sensor.h` — PZEM interface with mock mode
3. `logging.h` — New 11-byte LogEntry struct (delta energy, 0xFF meta marker)
4. `logging.cpp` — Adapt for new struct (meta markers, hex output, 11-byte entries)
5. `motorESP.ino` — Main sketch (setup, loop, state machine, protection, web handlers)
6. `web_ui.h` — Lightweight dashboard + control + settings + data pages
7. `wifi_manager.h` — Rename to motorESP
8. `updates.h` — Update GitHub URLs
9. `deploy.sh` — Update mDNS name
10. Test compile + fix errors
11. Mock mode functional test

---

## 15. NOTES

- Single-phase motor: R-Y-B = Starting-Running-Common windings (NOT 3-phase)
- 9.6A running current at 240V = ~2300VA apparent power
- With PF ~0.5: real power ~1150W ≈ 1.5HP
- Contactor AUX contact handles latching — ESP only pulses
- PZEM before contactor = always monitors voltage (290V no-load, 240V loaded)
- PZEM max readable = 260V (clamps anything higher)
- PZEM V4.0 energy register wraps at 9999 kWh
- PZEM V4.0 uses PZEM004Tv30 library (same protocol as V3.0)
- 50V voltage drop from no-load to loaded indicates wiring resistance
- Energy stored as delta (Wh since last entry) — survives power outages and PZEM wraparound
- Power calculated in browser: P = V × I × PF
- 11-byte log entry (down from 18): voltage 1B, current 2B, energy delta 1B, pf 1B
- Meta markers: pf = 0xFE (sector pointer), pf = 0xFF (SAT correction)
- Voltage field preserved for data (NOT used as meta marker)
- Control page at `/`, Dashboard at `/dashboard`
- Dexie.js client-side storage (same pattern as eggubator)
- Polling interval configurable 1-5s
- WiFi independent of pump state (pump runs during WiFi outage)
- SAT used for log timestamping and runtime hours display
- Web UI shows runtime in hours (not days like eggubator)
- Manual start detection: ESP detects via PZEM current, updates state automatically
- EEPROM trip persistence uses debounced writes (Option B) — write only on trip type change
- SAT used for log timestamping and runtime hours display
- Web UI shows runtime in hours (not days like eggubator)

---

*End of Consolidated Requirements — Ready for GO*
