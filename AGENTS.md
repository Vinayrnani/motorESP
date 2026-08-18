# AGENTS.md — motorESP (Submersible Pump Controller)

Single-file-hosted Arduino firmware for an ESP8266 submersible pump controller with PZEM 004T power metering, motor protections, flash data logging, web UI, and OTA updates. All HTML/CSS/JS lives inside `web_ui.h` as PROGMEM string literals — edit raw HTML embedded in C.

## Build & Deploy

```bash
# Compile — MUST run from the sketch root; folder name must equal main .ino name (motorESP)
rm -rf build/.cache   # stale-cache bug: source edits sometimes not picked up — always rm first
./bin/arduino-cli compile -b esp8266:esp8266:nodemcu -j 0 --build-path build/.cache --output-dir build motorESP.ino

# Deploy OTA (find IP via ping of motorESP.local, POST firmware.bin to /update)
./deploy.sh

# Flash USB (Termux/OTG)
./flash.sh

# Full release (bump → compile → commit → tag → GitHub Release → OTA)
./rel.sh [VERSION]      # auto-increment patch if no VERSION arg; includes OTA deploy
./rel-nd.sh [VERSION]   # same without OTA deploy
```

- `rel.sh` bumps `updates.h` + `version.txt`, compiles, creates GitHub Release with `firmware.bin`, then deploys OTA.
- `firmware.bin` is `.gitignore`d but release scripts `git add` it explicitly.
- `version.txt` must match `FIRMWARE_VERSION` in `updates.h`.

## Hardware Conventions

| Component | Pin | Active | Note |
|-----------|-----|--------|------|
| START1 relay | D1 (GPIO5) | LOW = CLOSE | CH1 NO, pulse 500ms |
| STOP relay | D2 (GPIO4) | LOW = CLOSE | CH2 NC, breaks coil, pulse 500ms |
| START2 relay | D4 (GPIO2) | LOW = CLOSE | CH3 NO, in SERIES with CH1 |
| SPARE relay | D7 (GPIO13) | LOW = CLOSE | CH4 unused |
| PZEM TX | D5 (GPIO14) | — | SoftwareSerial 9600 |
| PZEM RX | D6 (GPIO12) | — | SoftwareSerial 9600 |

**Relays are active LOW.** `digitalWrite(pin, HIGH)` = relay OFF (open), `LOW` = ON (closed). CH1+CH3 both must close for START. CH2 is NC and sits in series with the contactor coil — its pulse opens the circuit = STOP. All relays pulse 500ms, never latched by firmware. Getting polarity wrong can weld contactor contacts or leave the pump running.

Network: mDNS `motorESP.local`, static IP ends in `.72`. AP fallback SSID `motorESP` (after 20s) with DNS captive portal, rescans every 15s.

## Reverse Tunnel (CGNAT traversal)

The ESP is behind ISP CGNAT; remote access goes through an outbound raw-TCP
tunnel to a VM relay (no port forwarding):

```
Browser ──► VM :8280 (relay) ──► VM :9000 (tunnel) ◄── ESP dials OUT
```

- `tunnel_client.h` defines `TunnelWebServer` (subclass of `ESP8266WebServer`):
  when the server is idle (HC_NONE) the persistent `tunnelClient` is injected
  as the current client — ALL routes/OTA work unchanged. **Loop order matters:**
  `server.handleClient()` (LAN priority) then `server.handleTunnelClient()`.
- `TUNNEL_HOST`/`TUNNEL_PORT` in `config.h`; empty host = tunnel disabled.
  Numeric IP only, no DNS. Reconnect backoff 3s→30s (doubles on fail).
- Keepalive is lwIP TCP-level `tunnelClient.keepAlive(30,10,3)` — no app frames.
- Globals `tunnelClient`/`tunnelEnabled`/`tunnelState` are defined in
  motorESP.ino (extern in header) — sat_manager.cpp also includes the header;
  do NOT define them in the header (duplicate symbols).
- `/status` includes `"tunnel":"connected|connecting|wait_wifi|disabled"`.
- Relay (VM): `relay/tunnel_relay.js` (Node, zero deps) — requests are HTTP-
  framed and FIFO-queued onto the single ESP stream; complete responses are
  routed back to the owning browser (Content-Length/chunked/bodyless).
  502 when no ESP; queue survives nothing — requests fail 502 on ESP drop.
- systemd unit + install steps in `relay/README.md`. Ports: 9000 (tunnel,
  must be public), 8280 (HTTP). Local test: `relay/test/fake_esp.js` + curl
  against :8280. VM public IP: 68.233.98.190.
- **VM iptables gotcha**: this VM's INPUT chain ends with catch-all `REJECT
  icmp-host-prohibited` (Docker ports bypass it via FORWARD). The relay runs
  as a plain process, so explicit ACCEPT rules for 9000/8280 are required or
  the ESP's dial-in shows `No route to host` (fixed 2026-08-18, persisted via
  `netfilter-persistent save`). Cloud ingress rules alone do not suffice;
  the edge owning the public IP must forward 9000/8280 → 10.0.0.63.

## WiFi — Async State Machine (not blocking)

`wifi_manager.h` uses 5 states — no blocking `delay()` in init:

```
WF_TRY_SAVED → (10s timeout) → WF_TRY_DEFAULT → (10s timeout) → WF_AP
    ↕                               ↕                                  ↕
WF_CONNECTED ← (auto-reconnect) → WF_RECONNECTING (15s grace)   scan every 15s for saved/default SSID
```

- Boot order: `EEPROM.begin(512)` → `loadWifiCredentials()` → `initWiFi()` (non-blocking). Must stay in this order.
- `initWiFi()` returns immediately; `handleWiFi()` drives state from `loop()`.
- Priority: saved EEPROM creds → compile-time defaults (`config.h`) → AP fallback.
- `MDNS.begin()` deferred to first WiFi connection in loop (not setup).
- Saving WiFi creds via web (`/settings/api?wifiSsid=X&wifiPassword=Y`) writes EEPROM at addr 200 — does NOT reconnect. User must reboot.
- WiFi is independent of pump state: pump keeps running on WiFi loss, silent reconnect, flash logging unaffected.

## EEPROM Layout

| Region | Address | Magic | Size |
|--------|---------|-------|------|
| SAT drift | 15-23 | — | 9 bytes |
| DeviceSettings | 40 | `0xA2` | struct (~48B, all times in SECONDS) |
| WiFi credentials | 200 | `0xAC` | `WifiSettings` (98 bytes) |

- `loadWifiCredentials()` reads addr 200 — if magic invalid or SSID empty, keeps compile-time defaults.
- `saveWifiCredentials()` writes addr 200.
- `SETTINGS_MAGIC_VAL` = `0xA2` — bump it whenever DeviceSettings layout/semantics change (old EEPROM data then rejected → defaults).
- `loadSettings()` fully range-validates every field; any out-of-range → `initConfigDefaults()`.
- **Trip persistence (Option B, debounced)**: `saveActiveTrips()` writes EEPROM ONLY when `activeTrips` value changes (first trip of a sequence); same trip across retries/reboots → no rewrite. On boot, active trip → pump starts in TRIPPED, blocked until manual reset.
- Flash logging uses **separate** circular buffer at `0x200000` (256 sectors × 4096 bytes) — EEPROM untouched by logging.

## Pump State Machine (motorESP.ino)

```
OFF → ST_STARTING (start pulse 500ms) → verify (1s, fresh readPZEM()) → RUNNING (current ≥ 2A)
RUNNING/STARTING → any protection → ST_TRIPPED (all relays released)
ST_TRIPPED → auto-retry timer (300s, ≤3) or manual reset → OFF
RUNNING → ST_STOPPING (stop pulse) → OFF
```

- Relays: `relaysOff()` all HIGH; `fireStartPulse()` CH1+CH3 LOW; `fireStopPulse()` CH2 LOW.
- **Interlock for START** (startPump): no active trip/permanent lockout, not in start-fail block, not in voltage lockout, min-off elapsed, PZEM valid (or mock), voltage < VOLT_CRITICAL. Manual start bypasses power-restored flag; AUTO/OFF mode never restarts after power restoration.
- **External manual start**: pump OFF + PZEM current ≥ 2A → auto-state RUNNING (physical GREEN equivalent), clears powerRestored.
- **External manual stop**: pump RUNNING + current < 0.5A for 1.5s → OFF.
- **Min run 30s / min off 60s** enforced (web start/stop blocked outside window).
- Two-stage OC: during first 5s after start uses 50A instant; thereafter 12A with 5s accumulation.
- Dry-run armed 60s after start; current < 4A AND power < 500W for 15s → trip.
- Voltage: over 250V / under 190V for 3s → trip + `voltageLockUntil = now + 300s`.
- PZEM fault: read invalid while RUNNING (and not mock) → immediate trip (fail-safe).
- Fast-fault: trip recurs within 10s → fastFaultCount++; 3 fast faults → `permanentLockout` (manual reset only).
- Logging: `handleLogging()` each loop; 11-byte entries; intervals running 10s / OFF 60s; force-log on state change.

## Logging Format (logging.h/.cpp)

`LogEntry` packed 11 bytes: timeSec u32 · voltage u8 (offset from 200V) · current u16 (A×10) · energyDelta u8 (Wh since last entry) · pf u8 (×100; 0xFE/0xFF = meta markers) · states u8 · bootId u8.

- `LOGS_PER_SECTOR` = 372 (auto from struct size), 256 sectors at `0x200000`, 95,232 total entries.
- Meta markers: pf=0xFE sector pointer, 0xFF SAT correction (real PF 0-100 so both impossible).
- Energy stored as delta → survives outages + PZEM 9999kWh wrap; total accumulates in RAM for /status.
- State bits: 1 RUNNING · 2 OC · 4 DRYRUN · 8 OVERVOLT · 16 UNDERVOLT · 32 AUTO · 64 PZEM · 128 STARTFAIL.
- Brokers/browsers decode hex; skip entries with pf ≥ 0xFE.

## PZEM 004T (pzem_sensor.h)

- Modbus-RTU 9600 on SoftwareSerial D5(TX)/D6(RX), V4.0 100A uses same protocol as V3.0 (mandulaj).
- Read command {0xF8, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x64, 0x64} → 25-byte response, big-endian decode.
- voltage = be16×0.1; current = be32×0.001; power = be32×0.1; energy = be32 (Wh); freq = be16×0.1; pf = be16×0.01.
- 3 retries × 500ms timeout; fail → `valid=false`.
- Mock: `useMockPZEM` + globals `mockVoltage/mockCurrent/mockPower/mockEnergy/mockFrequency/mockPF` defined in motorESP.ino; profiles `setMockRunning/setMockOff/setMockDryRun/setMockOvercurrent`.
- All functions `static` in header (included by both logging.cpp and motorESP.ino — no duplicate symbols).

## Verification

1. **Compile** — must succeed with zero errors (`rm -rf build/.cache` first).
2. **Browser** — `http://motorESP.local/`, Control page loads, `/status` returns JSON.
3. **Manual Playwright tests** at `test/playwright/test_*.js` — `node test_xxx.js` (device must be reachable).
4. **Mock mode** (no hardware): `/settings/api?enable=1&mockProfile=running|dryrun|oc|off` or `/settings/api?mock=1&mockVoltage=258`.
5. **Acceptance matrix**: each protection trips with correct bit (2/4/8/16/64/128); trip survives /reboot; auto-retry schedules; permanent lockout after 3 fast faults; min-run/min-off blocks; critical-voltage start block; external manual start/stops; all pages HTTP 200; `/data/api` valid JSON.

## Embedded Assets (no internet needed)

5 gzipped libraries compiled into firmware via `embedded_assets.h`:

| Asset | Path |
|-------|------|
| Chart.js 4.4.7 | `/lib/chartjs/chart.umd.min.js` |
| Dexie.js 3.2.2 | `/lib/dexie/dexie.min.js` |
| Hammer.js | `/lib/hammerjs/hammer.min.js` |
| chartjs-plugin-zoom | `/lib/chartjs-plugin-zoom/chartjs-plugin-zoom.min.js` |
| Bootstrap 5 CSS | `/lib/bootstrap/css/bootstrap.min.css` |

- Served with `Content-Encoding: gzip` and `Cache-Control: public, max-age=31536000, immutable`.
- Registered as routes in `setup()` via `EMBEDDED_ASSETS` table.
- Web UI (`web_ui.h`) loads these paths — they resolve locally, not from CDN.

## Web Endpoints

| Path | Method | Purpose |
|------|--------|---------|
| `/` | GET | Control page (ON/OFF, mode, reset, V/A/W) |
| `/dashboard` | GET | Dashboard page (charts, numerics, polling 1-5s) |
| `/settings` | GET | Settings page |
| `/data` | GET | Data page (paginated table) |
| `/status` | GET | JSON: V/A/W/kWh/Hz/PF, pump state, trips, uptime, version, mock |
| `/data/api` | GET | JSON logs (`count`/`boot`/`time` pagination) |
| `/control` | GET | `action=start\|stop\|reset\|mode` |
| `/settings/api` | GET/POST | Read/write config, mock, WiFi creds, `action=newBatch` |
| `/timestamps` | GET/PUT | SAT boot table sync |
| `/ota/check` | GET | Version vs GitHub release |
| `/ota/apply` | POST | Download + flash firmware.bin from GitHub |
| `/reboot` | GET | `ESP.restart()` |
| `/settings/clear` | GET | Erase flash logs, reset boot ID, reboot |
| `/update` | POST | ESP8266HTTPUpdateServer (used by `deploy.sh`) |
| `/api/sector_hex` | GET/POST | Flash hex editor (`?sector=N`) |

## Connectivity (when device unreachable)

1. `http://motorESP.local`
2. `ping -c 1 motorESP.local` or `arp-scan -l`
3. `http://192.168.X.72` (X from DHCP subnet)
4. If all fail, report unreachable — no automated retries.

## Gotchas

- `config.h` has actual WiFi credentials — don't commit changes to it.
- `firmware.bin` is gitignored but release scripts stage it explicitly.
- **Stale build cache**: if a source edit doesn't appear in the binary, `rm -rf build/.cache` before compiling.
- `s.trim()` returns void in ESP8266 String — use `substring(0, length-1)`.
- `"const char*" + macro` is invalid C++ — wrap in `String(...)` first.
- All DeviceSettings time fields are SECONDS on the wire and in EEPROM; internal globals are milliseconds (`OC_DELAY` etc.).
- `wifiSsid`/`wifiPassword` globals in `wifi_manager.h` are runtime-writable — EEPROM loads over defaults on boot if valid.
- Clearing WiFi creds (empty SSID via web) resets to compile-time defaults in `config.h`.
- `EEPROM.begin(512)` must happen before `loadWifiCredentials()` and `initWiFi()`.
- mDNS blocked on mobile hotspots — device unreachable via `.local` when connected through phone hotspot.
- Firmware IROM ~59% used (~1MB IROM); no external Arduino libraries beyond ESP8266 core + SoftwareSerial.
- OTA deploy: device reboots ~15–25s after "Update Success!"; bootId may stay constant across reboots (recoverBootIdFromFlash behavior — harmless).
- Cloudflare trycloudflare tunnels rotate; 502 = tunnel dead, ask user for a new URL. Device reachable ONLY via tunnel URL in field tests.
- No CI, no linter, no formatter, no automated test runner.

## Repo Layout

```
motorESP.ino        # Main sketch: state machine, protections, web handlers, EEPROM
config.h            # Pins, compile-time defaults, extern declarations
pzem_sensor.h       # PZEM 004T Modbus + mock profiles
wifi_manager.h      # Async WiFi + captive portal
logging.h/.cpp      # Flash circular buffer + boot sessions
sat_manager.h/.cpp  # SAT timing
updates.h           # OTA from GitHub releases
web_ui.h            # 4 pages of HTML/CSS/JS (PROGMEM)
embedded_assets.h   # 5 gzipped libs
sector_viewer.h     # Flash hex editor
ntp_sync.h          # NTP sync
tunnel_client.h     # Reverse tunnel (TunnelWebServer + reconnect manager)
relay/              # VM-side relay: tunnel_relay.js, systemd unit, fake ESP
docs/               # Architecture docs (flash saving, logging, SAT)
test/playwright/    # Manual smoke tests
archive/            # Legacy eggubator files (reference only)
```

## Settings API Field Ranges (seconds where applicable)

- ocRunning 5-50A · ocStartInstant 20-100A · ocDelay 1-30s
- dryRunCurrent 1-10A · dryRunPower 100-2000W · dryRunDelay 1-300s · dryRunActivation 0-3600s
- voltOver 200-280V · voltUnder 150-230V · voltWarn 240-280V · voltCritical 250-300V · voltageDelay 1-60s · voltageLockout 0-3600s
- startSuccessCurrent 0.5-5A · startVerifyDelay 1-10s · startFailBlock 1-600s
- minRun 10-300s · minOff 10-600s
- autoRetryDelay 60-3600s · maxRetries 1-10 · maxFastFaults 1-10 · fastFaultWindow 5-60s
- logIntervalRunning 5-60s · logIntervalOff 30-600s · pzemReadRunning 1-5s · pzemReadOff 1-10s