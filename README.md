# Egg Incubator Controller

An ESP8266-based automatic egg incubator controller with web interface, temperature/humidity control, Data logging, and OTA updates.

## Hardware Setup

### Components
- **ESP8266 NodeMCU** - Main controller
- **DHT22** - Temperature & humidity sensor
- **Relay Module** (3-channel)
  - Heater relay
  - Atomizer/ultrasonic mist maker relay
  - Fan relay
- **Servo SG90** - Egg turner
- **Power Supply** - 5V for ESP8266, 12V for heater/atomizer

### Pin Mapping
| Pin | Device | Function |
|-----|--------|----------|
| D1 | Relay 1 | Heater |
| D2 | Relay 2 | Atomizer (humidity) |
| D3 | Relay 3 | Fan |
| D4 | DHT22 | Temperature/Humidity sensor |
| D5 | Servo | Egg turning motor |

All pin assignments are in `config.h`.

## Features

### Temperature Control
- Target: 37.5°C (±0.3°C hysteresis)
- Automatic heater on/off
- Overheat protection: fan ON when temp > 38°C

### Humidity Control (Pulsating Mode)
- Target: 55% RH (incubation), 65% RH (lockdown)
- **3 seconds ON → 10 seconds OFF** cycle (configurable)
- Repeats until target humidity reached
- When humidity is high (> target + 5%), atomizer stays OFF

### Fan Control (Smart Timing)
- ON when heater is ON
- Continues **3 seconds after heater turns OFF**
- ON when atomizer is ON (spreads humidity)
- Continues **3 seconds after atomizer turns OFF**
- OFF only when both temperature and humidity are stable
- ON when temperature > 38°C (overheat protection)

### Egg Turner
- Automatic rotation every 2 hours (configurable)
- Smooth ease-in-out movement over 10 seconds (configurable)
- Angle adjustment (-40 to +40) via settings page
- Disabled during lockdown stage (day 18+)

### Data Logging
- Logs to **serial flash memory** at 0x200000 (not EEPROM)
- Stores **131,072 entries** across 256 circular sectors
- Each entry: timestamp, temp, humidity, device states (5-byte overhead)
- Auto-purges oldest when circular buffer wraps
- Boot sessions tracked with meta entries for absolute time recovery
- Correction logs (invisible) handle time drift across reboots

### Web Interface
- Live temperature & humidity display with device controls
- **Interactive Chart.js charts** with zoom, pan, and decimation
- **SAT (Synchronised Absolute Timestamp)** syncs boot history across reboots
- **Dexie.js (IndexedDB)** stores historical data in browser
- Device status & settings page
- OTA firmware update
- Environment stability analysis (last 24h + entire incubation)

### SAT (Synchronised Absolute Timestamp)
- Tracks boot sessions across power cycles
- Recovers sector pointers from flash meta entries (no EEPROM wear)
- Recovers servo position from last log entry
- Synchronises incubation timeline between ESP and browser
- Auto-adjusts for time drift > 5 seconds

### Flash Durability
- **No per-boot EEPROM writes** — boot ID and sector pointers recovered entirely from flash
- `recoverBootIdFromFlash()` walks flash backwards on boot to find last boot ID
- `clearLogs()` uses a `temp=255` meta flag to signal boot ID reset on next boot
- EEPROM used only for settings changes and SAT drift corrections (infrequent)

## Network

- Connects to WiFi via DHCP first
- Sets static IP ending with **`.72`** (auto-detects subnet/gateway from DHCP)
- mDNS hostname: `eggubator.local`
- Falls back to DHCP if static IP fails

### Connectivity Workflow
If the device is not reachable, follow this sequence exactly:
1.  **Attempt mDNS**: Try opening `http://eggubator.local` in your browser.
2.  **Network Discovery**: If mDNS fails, use network tools (e.g., `ping -c 1 eggubator.local`, or `arp-scan -l`) to find the device.
3.  **Static IP**: If discovery fails, attempt connection directly via the configured static IP (e.g., `http://192.168.X.72`).
4.  **Stop & Report**: If none of the above work, the device is unreachable. Inform the user and stop; do not attempt further automated retries.

## Web Interface Endpoints

| Endpoint | Description |
|----------|-------------|
| `/` | Main web dashboard |
| `/status` | JSON status (temp, humidity, sectors, boot info) |
| `/data` | JSON sensor data with log pagination |
| `/settings` | Device settings page |
| `/settings/api` | Settings API (control, simulation, calibration) |
| `/control?device=X&mode=Y` | Control devices (heater/atomizer/fan/servo) |
| `/timestamps` | SAT boot timestamp sync (GET + PUT) |
| `/ota/check` | Check for updates |
| `/ota/update` | Trigger OTA update |
| `/reboot` | Reboot device |
| `/settings/clear` | Clear all log data and reset boot ID |

## OTA Updates

### Manual Update
Upload firmware via web interface at `http://<IP>/update`

### Auto Update
1. Host `firmware.bin` at your server
2. Host `version.txt` with version string (e.g., "1.3.13")
3. Update URLs in `updates.h`:
```cpp
#define FIRMWARE_URL "http://your-server/firmware.bin"
#define VERSION_URL "http://your-server/version.txt"
```

## File Structure

```
eggubator/
├── eggubator.ino        # Main sketch
├── config.h             # Configuration constants (pins, thresholds)
├── dht_sensor.h         # DHT22 sensor + physics simulation
├── wifi_manager.h       # WiFi connection with static IP
├── logging.h            # Flash logging + boot session tracking
├── logging.cpp          # Log read/write, sector management
├── sat_manager.h        # SAT timing system
├── sat_manager.cpp      # Timestamp sync, boot table, elapsed time
├── web_ui.h             # Full web dashboard HTML/CSS/JS (Chart.js + Dexie)
├── updates.h            # OTA updates + boot recovery
├── firmware.bin         # Compiled firmware binary
├── deploy.sh            # OTA deployment via mDNS
├── .gitignore
└── test/
    └── playwright/      # Playwright test artifacts (screenshots, logs, scripts)
```

## Build & Deploy

### Compile
```bash
arduino-cli compile -b esp8266:esp8266:nodemcu eggubator.ino
```

### Deploy via OTA
```bash
# Uses mDNS to find device and deploy via OTA update endpoint
./deploy.sh
```

### Flash via USB
```bash
esptool.py --chip esp8266 --port /dev/ttyUSB0 --baud 115200 write_flash -z \
  --flash_size=4MB --flash_mode=dio --flash_freq=40m \
  0x00000 firmware.bin
```

### Update WiFi Credentials
Edit `config.h`:
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

## Version History

| Version | Changes |
|---------|---------|
| **1.3.13** | Chart fixes (boot time drift, SAT sync order), favicon, code cleanup |
| **1.3.12** | Flash-based sector recovery (no EEPROM), servo position recovery |
| **1.3.11** | SAT architecture, startSector telemetry, boot session tracking |
| **1.3.0** | Modular SAT timing, invisible correction logs |
| **1.2.2** | Recovery system, uptime display, fixed degree symbol |
| **1.2.1** | Modular code structure, embedded DHT22 (no library) |
| **1.2.0** | Pulsating humidity (3s/10s), fan timing (5s), charts |
| **1.1.0** | Basic control with web interface, OTA |
