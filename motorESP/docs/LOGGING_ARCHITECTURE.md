# ESP8266 Flash-Based Circular Logging (Zero EEPROM)

A reusable pattern for sensor data logging on ESP8266 using a circular buffer in flash memory. All state is recovered from flash content — no per-boot EEPROM writes.

> **Why it matters:** ESP8266 EEPROM emulation uses a single flash sector at `0x3FB000` rated for ~100K erase cycles. Each `EEPROM.commit()` erases and rewrites that sector. If your project writes EEPROM on every boot (boot counter, recovery tracking, etc.) and reboots frequently, you'll exhaust the EEPROM sector in months. This pattern eliminates all boot-time EEPROM writes.

---

## Flash Memory Layout

Reserve a contiguous flash region for your circular log buffer:

```cpp
#define LOG_FLASH_START   0x200000   // Start address (must be sector-aligned)
#define SECTOR_SIZE       4096       // ESP8266 flash sector size
#define NUM_SECTORS       256        // Total sectors in the circular buffer
#define ENTRIES_PER_SECTOR (SECTOR_SIZE / sizeof(LogEntry))
```

- **256 sectors × 4 KB = 1 MB** circular buffer (adjust NUM_SECTORS for your flash size)
- Each sector gets a **meta entry** at offset 0, then real data entries
- No EEPROM stores sector positions — everything is recovered by scanning flash

### Log Entry Structure

Keep entries small and packed. Example 8-byte data entry:

```cpp
struct __attribute__((packed)) LogEntry {
  uint32_t timestamp;   // Relative timestamp (e.g., millis()/1000 at write time)
  uint8_t  sensor1;     // Primary sensor value
  uint8_t  sensor2;     // Secondary sensor value (can double as marker)
  uint8_t  states;      // Bitfield for device states
  uint8_t  sessionId;   // Session/boot identifier (0-255)
};
```

### Marker Values

Reserve specific `sensor2` (or whatever field is convenient) values to distinguish data from metadata:

| Value | Meaning |
|-------|---------|
| `0xFE` (254) | **Sector meta marker** — first entry of each sector carries sector number and session ID |
| `0xFF` (255) | **Special entry** — correction logs, system events, etc. (skipped in normal reads) |
| 0–100 | Real sensor data |

---

## Core Pattern 1: Sector Recovery From Flash

On every boot, find the current write position by scanning for the first erased sector:

```cpp
uint16_t currentSector = 0;
uint16_t startSector = 0;   // Oldest valid sector
uint16_t currentOffset = 0; // Next write position within currentSector

void recoverSectorPointers() {
  // Step 1: Find current sector — scan for first erased (0xFF) sector
  bool allErased = true;
  for (int s = 0; s < NUM_SECTORS; s++) {
    uint8_t firstByte;
    ESP.flashRead(LOG_FLASH_START + (s * SECTOR_SIZE), &firstByte, 1);
    if (firstByte != 0xFF) { allErased = false; break; }
  }

  if (allErased) {
    currentSector = 0;  // Fresh flash — start at sector 0
    startSector = 0;
    currentOffset = 0;
    return;
  }

  // Find first empty sector; currentSector is the one before it
  for (int s = 0; s < NUM_SECTORS; s++) {
    uint8_t firstByte;
    ESP.flashRead(LOG_FLASH_START + (s * SECTOR_SIZE), &firstByte, 1);
    if (firstByte == 0xFF) {
      currentSector = (s + NUM_SECTORS - 1) % NUM_SECTORS;
      break;
    }
    if (s == NUM_SECTORS - 1) currentSector = 0;  // Wrapped around
  }

  // Step 2: Recover startSector from meta entry at currentSector[0]
  LogEntry meta;
  ESP.flashRead(LOG_FLASH_START + (currentSector * SECTOR_SIZE),
                (uint32_t*)&meta, sizeof(LogEntry));
  if (meta.sensor2 == META_MARKER) {
    startSector = meta.timestamp;  // timestamp field stores startSector
  } else {
    // Fall back to previous sector's meta
    uint16_t prev = (currentSector + NUM_SECTORS - 1) % NUM_SECTORS;
    ESP.flashRead(LOG_FLASH_START + (prev * SECTOR_SIZE),
                  (uint32_t*)&meta, sizeof(LogEntry));
    startSector = (meta.sensor2 == META_MARKER) ? meta.timestamp : currentSector;
  }

  // Step 3: Scan current sector to find first empty slot
  currentOffset = 0;
  for (int i = 0; i < ENTRIES_PER_SECTOR; i++) {
    uint32_t addr = LOG_FLASH_START + (currentSector * SECTOR_SIZE) + (i * sizeof(LogEntry));
    uint32_t ts;
    ESP.flashRead(addr, &ts, 4);
    if (ts == 0xFFFFFFFF) { currentOffset = i; break; }
  }
}
```

**Cost:** ~512 flash reads (~5 ms), **zero EEPROM reads or writes**.

---

## Core Pattern 2: Session/Sequence ID Recovery From Flash

If your project tracks a session counter (boot number, batch ID, run number, etc.), recover it by walking backwards through existing entries:

```cpp
uint8_t recoverSessionId() {
  int s = currentSector;
  int o = currentOffset;

  // Start from the entry before currentOffset
  if (o == 0) {
    o = ENTRIES_PER_SECTOR - 1;
    s = (s + NUM_SECTORS - 1) % NUM_SECTORS;
  } else {
    o--;
  }

  for (int scanned = 0; scanned < 200; scanned++) {
    // If we've exhausted the current sector, check its meta for a reset flag
    // (See "Reset Flag Protocol" below)
    if (/* crossed sector boundary */) {
      checkMetaForReset();
    }

    uint32_t addr = LOG_FLASH_START + (s * SECTOR_SIZE) + (o * sizeof(LogEntry));
    LogEntry entry;
    ESP.flashRead(addr, (uint32_t*)&entry, sizeof(LogEntry));

    if (entry.timestamp != 0xFFFFFFFF && entry.sensor2 <= 100) {
      return entry.sessionId + 1;  // Next session ID
    }

    // Walk backwards
    if (o == 0) { o = ENTRIES_PER_SECTOR - 1; s = (s - 1 + NUM_SECTORS) % NUM_SECTORS; }
    else { o--; }
  }

  return 0;  // Nothing found — fresh flash
}
```

**Edge cases covered:**
- **Fresh flash** (all `0xFF`): walks backwards, finds nothing → returns 0
- **Normal reboot**: finds last entry, returns its `sessionId + 1`
- **After clear+immediate reboot**: hits the reset flag → returns 0
- **After clear that ran for a while**: finds new entries with continuing IDs

---

## Core Pattern 3: Reset Flag Protocol

When you clear the log (start a "new batch"), you want the session ID to reset to 0 on the *next* boot. Signal this by writing a meta entry with a non-zero "reset" value in an otherwise-unused field:

```cpp
void clearLogs() {
  // Advance to a fresh sector
  currentSector = (currentSector + 1) % NUM_SECTORS;
  startSector = currentSector;

  // Erase the new sector (and the next one, to guarantee an empty sentinel)
  ESP.flashEraseSector((LOG_FLASH_START + (currentSector * SECTOR_SIZE)) / SECTOR_SIZE);
  uint16_t nextSector = (currentSector + 1) % NUM_SECTORS;
  ESP.flashEraseSector((LOG_FLASH_START + (nextSector * SECTOR_SIZE)) / SECTOR_SIZE);

  // Write meta with reset flag in an unused field
  LogEntry meta;
  meta.sensor2 = META_MARKER;    // 0xFE — this is a meta entry
  meta.sensor1 = 255;            // 255 = RESET flag (unused for meta normally)
  meta.timestamp = startSector;  // Store startSector for recovery
  meta.states = 0;
  meta.sessionId = 0;

  uint32_t addr = LOG_FLASH_START + (currentSector * SECTOR_SIZE);
  ESP.flashWrite(addr, (uint32_t*)&meta, sizeof(LogEntry));
  currentOffset = 1;
}
```

In `recoverSessionId()`, when you've scanned the current sector and found zero real entries (crossed back to the current sector itself), check the meta:

```cpp
LogEntry meta;
ESP.flashRead(LOG_FLASH_START + (currentSector * SECTOR_SIZE),
              (uint32_t*)&meta, sizeof(LogEntry));
if (meta.sensor2 == META_MARKER && meta.sensor1 == 255) {
  return 0;  // Reset flag seen — start fresh
}
```

---

## Core Pattern 4: Writing Entries

```cpp
bool writeLogEntry(float sensor1, float sensor2, uint8_t states, uint8_t sessionId) {
  LogEntry entry;
  entry.timestamp = millis() / 1000;   // Relative time within session
  entry.sensor1 = encode(sensor1);
  entry.sensor2 = encode(sensor2);
  entry.states = states;
  entry.sessionId = sessionId;

  uint32_t addr = LOG_FLASH_START + (currentSector * SECTOR_SIZE)
                + (currentOffset * sizeof(LogEntry));
  ESP.flashWrite(addr, (uint32_t*)&entry, sizeof(LogEntry));

  currentOffset++;

  if (currentOffset >= ENTRIES_PER_SECTOR) {
    advanceSector();
  }
  return true;
}

void advanceSector() {
  currentSector = (currentSector + 1) % NUM_SECTORS;

  // Erase next sector
  ESP.flashEraseSector((LOG_FLASH_START + (currentSector * SECTOR_SIZE)) / SECTOR_SIZE);

  // Write meta entry at sector start
  LogEntry meta;
  meta.sensor2 = META_MARKER;
  meta.sensor1 = 0;
  meta.timestamp = startSector;  // Carry forward startSector
  meta.states = 0;
  meta.sessionId = currentSessionId;
  ESP.flashWrite(LOG_FLASH_START + (currentSector * SECTOR_SIZE),
                 (uint32_t*)&meta, sizeof(LogEntry));

  // Also erase sector+1 to guarantee an empty sentinel for next recovery
  int nextSector = (currentSector + 1) % NUM_SECTORS;
  uint8_t nextFirst;
  ESP.flashRead(LOG_FLASH_START + (nextSector * SECTOR_SIZE), &nextFirst, 1);
  if (nextFirst != 0xFF) {
    ESP.flashEraseSector((LOG_FLASH_START + (nextSector * SECTOR_SIZE)) / SECTOR_SIZE);
  }

  // Advance startSector if we wrapped around
  if (currentSector == startSector) {
    startSector = (startSector + 1) % NUM_SECTORS;
  }

  currentOffset = 1;  // Offset 0 is meta
}
```

---

## Boot Sequence (Putting It All Together)

```
setup():
  1. recoverSectorPointers()     // Find current position from flash scan
  2. sessionId = recoverSessionId()  // Get next session ID from last entry
  3. initialize subsystems with sessionId
  4. Continue normal operation
  // Zero EEPROM writes
```

---

## Flash Endurance Budget

| Operation | Erase cycles per occurrence | Wear impact |
|-----------|---------------------------|-------------|
| Per-boot sector recovery | 0 | None (read-only) |
| Per-boot session ID recovery | 0 | None (read-only) |
| Write entry (deduplicated) | Sector erase every 512 entries | ~0.5% per 100K writes (256 sectors) |
| clearLogs / reset | 2 sector erases | ~0.002% per reset |
| EEPROM settings save | 1 EEPROM sector erase | Use sparingly — 100K limit |

**Bottom line:** Your flash log buffer lasts the life of the device. The EEPROM sector only sees writes when you intentionally save settings.

---

## Reuse Checklist

To adapt this pattern to a new ESP8266 project:

1. **Pick a flash region** — must be sector-aligned, avoid regions used by OTA/SPIFFS
2. **Define LogEntry** — pack it, keep it as small as possible (8–16 bytes)
3. **Pick a marker field** — a byte that stays ≤100 for real data, 0xFE for meta, 0xFF for special
4. **Pick a reset field** — an unused byte in the meta entry that you set to 255 to signal reset
5. **Set NUM_SECTORS** — 256 is safe for 4MB flash, smaller for 1MB/2MB chips
6. **Call `recoverSectorPointers()` and `recoverSessionId()` in `setup()`**
7. **Never write EEPROM on boot** — only on explicit user actions (save settings, clear data)
