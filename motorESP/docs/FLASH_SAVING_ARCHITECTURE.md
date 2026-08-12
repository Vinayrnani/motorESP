# ESP8266 EEPROM Endurance: Eliminating Per-Boot Writes

A reference guide for avoiding EEPROM flash sector exhaustion on ESP8266 NodeMCU projects.

---

## The Problem

ESP8266 emulates EEPROM using a **single flash sector** (4 KB at `0x3FB000` for 4 MB flash). Every `EEPROM.commit()` performs one sector erase+write cycle. ESP8266 flash is typically rated for **~100,000 erase cycles**.

### Common EEPROM Anti-Patterns

| Pattern | Writes per boot | Exhaustion time (15-min cycles) |
|---------|----------------|-------------------------------|
| Boot counter increment | 1 | ~1.7 years |
| Boot failure tracking (`initRecovery`) | 2–3 | ~0.5–1 year |
| Storing sector pointers on every advancement | 1 per event | Varies |
| `elapsedSeconds++` each second | 86,400/day | ~1 day |
| Storing settings on every change | 1 per change | Depends on frequency |

**Key insight:** Anything you write to EEPROM on every boot is a potential lifetime bomb. Anything you write periodically (every second, every minute) will kill the EEPROM sector in days or weeks.

---

## The Solution: State Recovery From Flash

Most things you'd store in EEPROM can be **recovered from flash content** instead. Flash reads are free (no wear). Flash writes to dedicated log sectors are distributed across 256 sectors, each rated for 100K erases.

### What to Move Out of EEPROM

| EEPROM candidate | Flash recovery method | Implementation |
|-----------------|----------------------|----------------|
| Boot/session counter | Walk backwards through log entries, return `lastBootId + 1` | `recoverSessionId()` |
| Current write position | Scan flash sectors for first erased sector | `recoverSectorPointers()` |
| Oldest valid sector | Read from sector meta entry (stored at sector[0]) | Meta `timestamp` field |
| Boot failure count | Remove entirely (was diagnostic-only) | Delete the feature |
| Last good boot flag | Remove entirely (provided no real gating) | Delete the feature |

### What SHOULD Stay in EEPROM

| Data | Why it stays | Write frequency |
|------|-------------|-----------------|
| Device settings (WiFi credentials, thresholds, calibration) | Must survive any crash/power loss scenario | On user action only |
| SAT/absolute-time anchor values | Needed to reconstruct absolute timestamps across boots | Once per sync session (drift > threshold) |
| Any value that flash scanning cannot derive | By definition | As infrequent as possible |

---

## General Pattern: State Recovery Architecture

```
Traditional (bad):                  Flash-Recovery (good):

setup():                            setup():
  EEPROM.begin(512)                   EEPROM.begin(512)
  bootCount = EEPROM.read(ADDR)       // Scan flash for state
  bootCount++                         currentSector = recoverSectorFromFlash()
  EEPROM.write(ADDR, bootCount)       sessionId = recoverSessionId()
  EEPROM.commit()                     currentOffset = recoverOffset()
  // ...                              // No EEPROM.commit()

  EEPROM.commit()                     // Load settings (EEPROM read)
                                      loadSettings()
```

**The rule:** On boot, read from EEPROM only for things that *cannot* be derived from flash content. Derive everything else by scanning flash.

---

## Implementation Recipes

### Recipe 1: Boot/Session Counter Without EEPROM

```cpp
// Each log entry includes a sessionId field.
// On boot, walk backwards from the current write position
// to find the most recent entry, then return its sessionId + 1.

uint8_t recoverSessionId() {
  int s = currentSector;
  int o = currentOffset;

  // Step back one position
  if (o == 0) {
    o = ENTRIES_PER_SECTOR - 1;
    s = (s + NUM_SECTORS - 1) % NUM_SECTORS;
  } else {
    o--;
  }

  for (int i = 0; i < 200; i++) {
    LogEntry e = readEntry(s, o);
    if (e.timestamp != 0xFFFFFFFF && e.dataField <= 100) {
      return e.sessionId + 1;
    }

    // Walk backwards
    if (o == 0) {
      o = ENTRIES_PER_SECTOR - 1;
      s = (s + NUM_SECTORS - 1) % NUM_SECTORS;
    } else {
      o--;
    }
  }
  return 0; // Fresh flash
}
```

### Recipe 2: Sector Position Recovery Without EEPROM

```cpp
// Find currentSector by scanning for the first fully-erased sector.
// The sector before it is the current write head.

void recoverSectorPointers() {
  // Scan for first erased (0xFF) sector
  for (int s = 0; s < NUM_SECTORS; s++) {
    uint8_t firstByte;
    flashRead(sectorAddress(s), &firstByte, 1);
    if (firstByte == 0xFF) {
      currentSector = (s - 1 + NUM_SECTORS) % NUM_SECTORS;
      // currentSector found
      break;
    }
  }

  // Read meta at currentSector[0] to recover startSector
  LogEntry meta = readEntry(currentSector, 0);
  if (meta.markerField == META_MARKER) {
    startSector = meta.timestamp; // timestamp field stores startSector
  }

  // Scan sector for first empty slot
  for (int i = 0; i < ENTRIES_PER_SECTOR; i++) {
    if (readTimestamp(currentSector, i) == 0xFFFFFFFF) {
      currentOffset = i;
      break;
    }
  }
}
```

### Recipe 3: Clear/Reset Without EEPROM

When user wants to start fresh, don't clear EEPROM — use a flag in the flash meta entry:

```cpp
void resetAllData() {
  // Advance to a clean sector
  currentSector = (currentSector + 1) % NUM_SECTORS;
  startSector = currentSector;

  // Erase
  eraseSector(currentSector);
  eraseSector((currentSector + 1) % NUM_SECTORS); // Empty sentinel

  // Write meta with RESET flag
  LogEntry meta = {
    .timestamp = startSector,
    .sensor1   = 255, // RESET flag — tells recoverSessionId() to return 0
    .sensor2   = META_MARKER,
    .states    = 0,
    .sessionId = 0
  };
  flashWrite(sectorAddress(currentSector), &meta, sizeof(LogEntry));
  currentOffset = 1;
}
```

### Recipe 4: Guaranteeing Empty Sentinel

Use a **two-sector erase** on reset to prevent `initSectorPointers` from misreading old data:

```cpp
void eraseSector(uint16_t sector) {
  ESP.flashEraseSector((LOG_FLASH_START + (sector * SECTOR_SIZE)) / SECTOR_SIZE);
}

void advanceSector() {
  currentSector = (currentSector + 1) % NUM_SECTORS;
  eraseSector(currentSector);           // Erase next write target
  eraseSector((currentSector + 1) % NUM_SECTORS); // +1: empty sentinel

  // Write meta at new sector[0]
  writeMetaEntry();
  currentOffset = 1;
}
```

---

## Validating Your Boot Sequence

Before/after checklist for migrating a project off per-boot EEPROM writes:

1. **List every `EEPROM.commit()` call** in your codebase
2. **Categorize each**: boot-time vs event-driven vs periodic
3. **For boot-time commits**: can the value be recovered from flash instead?
4. **For periodic commits**: can you switch to event-driven (only on change)?
5. **Verify**: after removing boot-time commits, does the project boot correctly with a fresh flash? After a crash? After a clear/reset?

### Test Matrix

| Scenario | Expected outcome |
|----------|-----------------|
| Fresh flash (all 0xFF) | Boots with session=0, sector=0 |
| Normal boot after logging | session = last session + 1 |
| clearLogs → immediate reboot | session = 0 (reset flag read) |
| clearLogs → run for hours → reboot | session continues from last entry |
| Power loss mid-write | Next boot recovers from last intact entry |
| Sector wrap-around (256 sectors full) | Circular buffer continues, oldest data overwritten |
| Remove EEPROM chip (read fails) | Boot still works (all state from flash) |

---

## Common Pitfalls

1. **Forgetting to erase the sentinel sector** — `initSectorPointers` finds the first 0xFF sector by scanning from sector 0. If all sectors except the current one have old data, it incorrectly identifies the current sector as the one-before-empty. Always erase sector+1 on advancement.

2. **Meta entry bootId goes stale after clearLogs** — The meta entry at `startSector[0]` still has the old `sessionId` from before the clear. If you use this for session recovery, you'll get the wrong value. Either skip the meta entry (use `s != startSector || o != 0` check) or use the reset flag protocol.

3. **Too few scan iterations** — If your walk-back loop only checks 50 entries but there are 512 entries in a sector, you'll miss the last valid entry. Use at least `ENTRIES_PER_SECTOR × 2` iterations or a reasonable upper bound (200 covers most cases).

4. **EEPROM.begin() but never committed** — `EEPROM.begin(512)` allocates a RAM buffer. If you never call `EEPROM.commit()`, writes go to RAM only and are lost on power loss. This is fine for read-only usage but confusing if you mix read and write patterns.

5. **Sector pointer defines in code** — If you previously had `EEPROM_CURRENT_SECTOR` and `EEPROM_START_SECTOR` addresses defined, remove them. They're vestigial once you switch to flash recovery. Keeping them causes confusion about what's actually stored where.

---

## Reference: Full Boot Sequence

```
EEPROM.begin(512)           // Only needed if settings/SAT data uses EEPROM
                            // No commit() follows — pure read

recoverSectorPointers()     // Scans flash, finds currentSector, startSector, currentOffset
                            // 0 EEPROM reads, 0 EEPROM writes

recoverSessionId()          // Walks backwards from currentOffset
                            // Returns lastBootId + 1, or 0 if reset flag seen
                            // 0 EEPROM reads, 0 EEPROM writes

loadSettings()              // Reads DeviceSettings from EEPROM
                            // EEPROM read-only, no commit

Normal operation continues  // EEPROM writes only on:
                            //   - User saves settings
                            //   - SAT drift correction (infrequent)
                            //   - clearLogs / new batch (explicit action)
```

**Total EEPROM writes per boot: 0**
