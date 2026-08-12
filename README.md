# motorESP — Submersible Pump Controller

ESP8266-based submersible pump automation controller with PZEM 004T power metering, multi-layer motor protection, flash data logging, web UI, and OTA updates. Derived from the eggubator framework (WiFi manager, SAT, flash logging, embedded-asset web UI kept; all incubator logic replaced).

## Hardware

### Components
- **ESP8266 NodeMCU** — main controller
- **PZEM 004T V4.0 (100A)** — voltage / current / power / energy / PF / frequency (Modbus-RTU 9600)
- **4-channel 5V relay module** (active LOW) — START1, STOP, START2, SPARE
- **240VAC coil contactor** with AUX self-latch — carries the pump load
- **GREEN (NO) / RED (NC) push buttons** — physical start / stop
- **HLK-PM01** — 5V supply (1A min) for ESP + relays

### Pin Mapping
| Pin | Device | Function |
|-----|--------|----------|
| D1 (GPIO5) | Relay CH1 (NO) | START1 — start pulse |
| D2 (GPIO4) | Relay CH2 (NC) | STOP — breaks coil (stop pulse) |
| D4 (GPIO2) | Relay CH3 (NO) | START2 — start pulse (series with CH1) |
| D7 (GPIO13) | Relay CH4 | SPARE |
| D5 (GPIO14) | PZEM TX → D5 | Modbus TX |
| D6 (GPIO12) | PZEM RX ← D6 | Modbus RX |

**All relays are PULSE type, 500ms.** CH1 + CH3 are wired in **series** for START (both must close). CH2 (NC) sits in series with the contactor coil for STOP. Relays are **active LOW**: `digitalWrite(pin, HIGH)` = relay OFF, `LOW` = ON. Getting this wrong can overheat or damage hardware.

PZEM: voltage terminals L&N BEFORE the contactor (always powered → pre-start voltage check possible); CT clamps the live wire to the contactor. Max readable voltage 260V (site runs 240V loaded / 290V no-load — high voltage only drives warnings, never measurement destruction).

## Features

### Motor Protection
| Protection | Trigger | Delay | Result |
|------------|---------|-------|--------|
| Overcurrent (start) | current ≥ 50A | instant | Trip + lockout/retry |
| Overcurrent (running) | current ≥ 12A | 5s | Trip |
| Dry-run | current < 4A AND power < 500W | 15s | Trip (armed 60s after start) |
| Overvoltage (running) | voltage > 250V | 3s | Trip + 5min voltage lockout |
| Undervoltage (running) | voltage < 190V | 3s | Trip + 5min voltage lockout |
| PZEM fault | read timeout 500ms × 3 retries | fail-safe | Trip OFF |
| Start failure | current < 2A at verify (1s after pulse) | — | Trip + 30s retry block |

Pre-start: voltage ≥ 280V (critical) blocks start; ≥ 250V (warning) shows warning.

### Trip Behavior
- Per-protection **LOCKOUT** or **AUTO-RETRY** (configurable)
- AUTO-RETRY: 300s default delay, max 3 retries; fault recurring within 10s = fast fault; 3 fast faults = **PERMANENT LOCKOUT** (manual reset required)
- **Trip persistence**: trip state saved to EEPROM only when the trip type changes (debounced) — survives power loss; on boot an active trip blocks pump start until manual reset
- **Power restoration**: always requires manual start ("POWER RESTORED — MANUAL START REQUIRED")
- **Manual start detection**: physical GREEN button start is detected via PZEM current (> 2A) and the ESP auto-updates its state to RUNNING
- **Safety**: 3 independent stop paths (RED button, CH2 NC relay, ESP software); protection trip overrides OFF/MANUAL/AUTO modes

### Modes
- **OFF** — kill
- **MANUAL** — web ON/OFF buttons
- **AUTO** — time-of-day schedule (placeholder, SAT elapsed time)

### Data Logging
- 11-byte packed entries in **flash circular buffer** at 0x200000 (256 sectors × 4096B = 95,232 entries)
- Entry: timeSec(4) + voltage offset from 200V(1) + current A×10(2) + energy delta Wh(1) + PF×100(1) + states(1) + bootId(1)
- Energy stored as delta → survives power loss and PZEM 9999kWh wrap
- Meta markers in PF byte: 0xFE = sector pointer, 0xFF = SAT correction
- State bits: 1 RUNNING, 2 OC, 4 DRYRUN, 8 OVERVOLT, 16 UNDERVOLT, 32 AUTO, 64 PZEM, 128 STARTFAIL
- Log intervals: 10s running (5–60s), 60s OFF (30–600s); always logs on state change

### Web Interface
- **Control** `/` — big ON/OFF buttons, mode selector, trip reset, quick V/A/W stats, power-restored banner
- **Dashboard** `/dashboard` — large numerics (V/A/W/kWh/Hz/PF), status badge, voltage status (Normal/Warning/Critical), Chart.js line charts (power/voltage/current), poll interval 1–5s
- **Settings** `/settings` — all thresholds (OC, dry-run, voltage zones, start logic, auto-retry, logging intervals, PZEM read interval)
- **Data** `/data` — paginated log table (boot, time, V, I, Wh, PF, state) with LOAD MORE
- Dexie.js (IndexedDB) client-side history, SAT sync via `/timestamps`
- 5 embedded gzipped assets (Chart.js, Dexie, Hammer, chartjs-zoom, Bootstrap) — no CDN

## Network

- mDNS `motorESP.local`, static IP ending `.72`
- AP fallback SSID `motorESP` after 20s (with DNS captive portal), rescans every 15s
- WiFi independent of pump state — pump keeps running on WiFi loss; silent reconnect; flash logging unaffected

## EEPROM Layout (512 bytes)

| Region | Address | Magic | Content |
|--------|---------|-------|---------|
| SAT drift | 15–23 | — | last known boot ID + start unix |
| DeviceSettings | 40 | `0xA2` | pump thresholds, mode, trip state (~48B) |
| WiFi credentials | 200 | `0xAC` | ssid[33] + password[65] |

All time settings stored in **seconds** (magic 0xA2). Settings with invalid magic or out-of-range values reset to defaults.

## Web Endpoints

| Path | Method | Purpose |
|------|--------|---------|
| `/` | GET | Control page |
| `/dashboard` | GET | Dashboard page |
| `/settings` | GET | Settings page |
| `/data` | GET | Data page |
| `/status` | GET | JSON: V, A, W, kWh, Hz, PF, pump state, protection state, heap, uptime |
| `/data/api` | GET | JSON log payload (pagination via `boot`, `time`, `count`) |
| `/control` | GET | `action=start\|stop\|reset\|mode` (mode=N) |
| `/settings/api` | GET/POST | Read/write config, mock enable + profiles, WiFi creds, `action=newBatch` |
| `/timestamps` | GET/PUT | SAT boot table sync |
| `/ota/check` | GET | Compare version vs GitHub release |
| `/ota/apply` | POST | Download + flash firmware.bin from GitHub |
| `/reboot` | GET | `ESP.restart()` |
| `/settings/clear` | GET | Erase flash logs, reset boot ID, reboot |
| `/update` | POST | ESP8266HTTPUpdateServer (used by `deploy.sh`) |
| `/api/sector_hex` | GET/POST | Flash sector hex editor (`sector=X`) |
| `/lib/*` | GET | Embedded gzipped assets (immutable cache) |

### Mock Mode (no hardware)
```
/settings/api?enable=1&mockProfile=running|off|dryrun|oc
/settings/api?mock=1&mockVoltage=258     # voltage protection testing
```

## Build & Deploy

```bash
# Compile (must run from sketch root — folder name must match .ino name)
rm -rf build/.cache
./bin/arduino-cli compile -b esp8266:esp8266:nodemcu -j 0 --build-path build/.cache --output-dir build motorESP.ino

# Deploy OTA (finds device, POSTs to /update)
./deploy.sh

# Flash USB (Termux/OTG)
./flash.sh

# Full release (bump → compile → commit → tag → GitHub Release → OTA)
./rel.sh [VERSION]        # with OTA deploy
./rel-nd.sh [VERSION]     # without OTA deploy
```

`rel.sh` bumps `updates.h` + `version.txt`, compiles, creates GitHub Release with `firmware.bin`, deploys OTA. `version.txt` must match `FIRMWARE_VERSION` in `updates.h`.

## File Structure

```
motorESP.ino        # Main sketch: state machine, protections, web handlers, EEPROM
config.h            # Pins, compile-time defaults, extern declarations
pzem_sensor.h       # PZEM 004T Modbus interface + mock profiles
wifi_manager.h      # Async WiFi state machine + DNS captive portal
logging.h/.cpp      # Flash circular buffer, 11-byte entries, boot sessions
sat_manager.h/.cpp  # Boot session tracking, absolute time recovery
updates.h           # OTA check + download from GitHub releases
web_ui.h            # All HTML/CSS/JS as PROGMEM strings (4 pages)
embedded_assets.h   # 5 gzipped JS/CSS libs compiled in (no CDN)
sector_viewer.h     # Flash hex editor tool
ntp_sync.h          # NTP sync (when internet available)
docs/               # Architecture references (flash saving, logging, SAT)
test/playwright/    # Manual Playwright/HTTP smoke tests
archive/            # Legacy eggubator firmware files
```

## Read More

- `REQUIREMENTS.md` — full requirements, safety analysis, test checklist
- `REVIEW.md` — architecture review record