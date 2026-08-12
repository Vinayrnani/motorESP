# SAT (Synchronised Absolute Timestamp) Architecture

## Problem: MICROCONTROLLER Lacks RTC

The MICROCONTROLLER has no built-in Real Time Clock (RTC), meaning it cannot keep accurate time after power loss. This is critical for time-sensitive applications which need to track:
- Operational day counters
- Scheduled intervals
- Data logging timestamps
- Up-time calculation

## Solution: SAT Architecture

SAT creates a **Virtual RTC** by synchronising time between MICROCONTROLLER and browser's Dexie (IndexedDB). When connected, browser provides absolute timestamps. When disconnected, MICROCONTROLLER uses relative time with drift compensation.

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        PROJECT_NAME System                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   ┌─────────────┐          ┌────────────────────────────┐    │
│   │   MICROCONTROLLER   │◄───────►│        Browser               │    │
│   │             │  WiFi   │                              │    │
│   │ ┌─────────┐ │         │ ┌──────────┐ ┌─────────────┐ │    │
│   │ │millis() │ │         │ │  Dexie   │ │  UI/Charts │ │    │
│   │ │relative │ │         │ │(IndexedDB)│ │            │ │    │
│   │ └────┬────┘ │         │ └────┬─────┘ └─────────────┘ │    │
│   │      │      │         │      │                     │    │
│   │ ┌────▼────┐ │         │ ┌────▼─────────────────┐   │    │
│   │ │SAT State│ │         │ │  Boot History Table  │   │    │
│   │ │         │ │         │ │  bootId → startUnix │   │    │
│   │ └────┬────┘ │         │ └─────────────────────┘   │    │
│   │      │      │         │      ▲                     │    │
│   │ ┌────▼────┐ │   Sync  │      │                     │    │
│   │ │GENERIC_STORAGE   │ │◄────────┴──────┘                     │    │
│   │ │         │ │                                    │    │
│   │ │lastKnown│ │  Source of Truth                   │    │
│   │ │BootId   │ │                                    │    │
│   │ │lastKnown│ │                                    │    │
│   │ │StartUnix│ │                                    │    │
│   │ └─────────┘ │                                    │    │
│   └─────────────┘                                    └────────┘
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Core Components

#### 1. MICROCONTROLLER Side

**RAM-based Timestamp Table:**
```cpp
struct BootTimestamp {
  uint8_t bootId;       // Boot session identifier (0-255)
  uint32_t startUnix;   // Unix timestamp when this boot started
  uint32_t duration;    // Duration in seconds (max ~194 days)
};
```

The table is held in RAM and prepared on boot from:
- Flash log sectors: read boot IDs and duration (last log entry's timeSec)
- GENERIC_STORAGE lastKnownStartUnix: critical for calculating startUnix of all previous boots

**How previous boots' startUnix is calculated:**
```
bootTable[previousBoot].startUnix = lastKnownStartUnix - sumOfAllPreviousDurations
```

This chain continues backwards for each boot:
```
boot[N-1].startUnix = lastKnownStartUnix - duration[N] - duration[N-1] - ...
```

**State Variables:**
```
(currentBootId retrieved from GENERIC_STORAGE_BOOT_ID on demand)
startTimestamp     - Unix timestamp when batch started (stored in GENERIC_STORAGE DeviceSettings)
```

**No stored elapsedSeconds** - Derived on-demand from bootTable + millis():
```
elapsedSeconds = bootTable[currentBootId].startUnix + (millis() / 1000) - startTimestamp
               = (currentBoot.startUnix - startTimestamp) + bootUptimeSec
```

**GENERIC_STORAGE Storage (essential for boot time calculation):**

| Address | Size | Purpose |
|---------|------|---------|
| GENERIC_STORAGE_LAST_KNOWN_BOOT_ID | 1 byte | Previous boot's bootId (identifies which boot was last synced) |
| GENERIC_STORAGE_LAST_KNOWN_START_UNIX | 4 bytes | Previous boot's startUnix (synced from browser, used to calculate ALL previous boots' start times) |
| GENERIC_STORAGE_SETTINGS_MAGIC | struct | Contains `startTimestamp` (fixed Unix timestamp when batch started) |

**Note:** Duration is retrieved from flash (last log entry's timeSec), not stored in GENERIC_STORAGE.

**Critical:** These values form the anchor for calculating startUnix of all previous boots. Without this, previous boots would have unknown timestamps.

**Note:** These values store the *previous* boot's data (not current, not batch start). On next boot, this becomes the "previous boot" entry in bootTable.

#### 2. Browser Side (Dexie)

**Database Schema:**
```javascript
db.version(2).stores({
  logs: 't, timeSec, bootId, temp, hum, h, a, f, s'
});
```

**Boot History Table (stored in Dexie/IndexedDB, not in memory):**
```javascript
// Stored in Dexie table 'bootTimestamps'
const db = new Dexie('PROJECT_NAMEDB');
db.version(3).stores({
  logs: 't, timeSec, bootId, temp, hum, h, a, f, s',
  bootTimestamps: 'bootId, startUnix, duration'  // NEW table
});

// Stored in Dexie (source of truth for browser)
// Derived from logs: group by bootId to get startUnix and duration
// Each boot's startUnix is stored once, not recalculated
const bootHistory = [
  { bootId: 1, startUnix: 1704067200 },  // startUnix stored
  { bootId: 2, startUnix: 1704153600 },  // startUnix stored
  // ... more entries
];
```

#### Derived Values (Calculated On-Demand)

Only the **current boot** needs to be derived. Previous boots' startUnix are already stored in bootTable (from previous sync).

**Key Functions:**
```cpp
// Get current boot uptime (seconds since this boot started)
uint32_t getBootUptime() {
  return millis() / 1000;
}

// Get current boot ID (from GENERIC_STORAGE)
uint8_t getCurrentBootId() {
  return GENERIC_STORAGE.read(GENERIC_STORAGE_BOOT_ID);
}

// Get elapsed seconds since batch started
// currentBoot.startUnix = stored value (or 0 if not yet synced)
uint32_t getElapsedSeconds() {
  uint8_t bootId = getCurrentBootId();
  return bootTable[bootId].startUnix + getBootUptime() - startTimestamp;
}

// Get current incubation day (1-21)
uint32_t getCurrentDay() {
  return getElapsedSeconds() / 86400;
}

// Check if in lockdown stage (Day 18+)
bool isLockdown() {
  return getCurrentDay() >= 18;
}
```

**Boot Table as Source of Truth:**
- All previous boots: startUnix stored in bootTable (from browser sync)
- Current boot: startUnix = 0 until first sync, then updated
- Duration: retrieved from flash (last log entry's timeSec)

**Important:** If `startUnix` is 0 (unknown), the derived elapsedSeconds will be incorrect.
This is okay because:
- MICROCONTROLLER control logic uses millis() only (not absolute time)
- Display shows "Day --" until sync provides real timestamps
- On first browser sync, all timestamps are corrected

**Key Insight:** No continuous counter, no periodic GENERIC_STORAGE writes, no boot-time loading.
All values calculated on-demand from bootTable + millis().

### API Endpoints

#### GET /timestamps
Returns MICROCONTROLLER's current timestamp knowledge.

**Response:**
```json
{
  "currentBootId": 5,
  "currentStartUnix": 1704153600,
  "bootUptimeSec": 3600,
  "bootTable": [
    { "bootId": 1, "startUnix": 1703808000 },
    { "bootId": 2, "startUnix": 1703894400 },
    ...
  ]
}
```

Note: Duration retrieved from flash, not sent over API.

Note: `elapsedSeconds`, `currentDay` are calculated on-demand, not stored.

#### PUT /timestamps
Browser sends its complete boot history. MICROCONTROLLER replaces its RAM table completely and saves fallback to GENERIC_STORAGE.

**Request:** (duration not sent - MICROCONTROLLER retrieves from flash)
```json
[
  { "bootId": 1, "startUnix": 1703808000 },
  { "bootId": 2, "startUnix": 1703894400 },
  { "bootId": 3, "startUnix": 1703980800 },
  { "bootId": 4, "startUnix": 1704067200 },
  { "bootId": 5, "startUnix": 1704153600 }
]
```

**MICROCONTROLLER Response:**
```json
{
  "synced": true,
  "entriesStored": 5,
  "driftCalculated": -12,
  "eepromUpdated": true,
  "currentBootId": 5,
  "currentStartUnix": 1704153600
}
```

**On receiving PUT /timestamps:**
1. Replace entire RAM bootTable[] with browser's data
2. Update lastKnownBootId, lastKnownStartUnix in GENERIC_STORAGE (new anchor)
3. Calculate drift from current boot's entry
4. Return sync confirmation

### Boot: Table Preparation

On MICROCONTROLLER boot, the RAM timestamp table is prepared by scanning flash log sectors:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Boot: Table Preparation                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   1. Read lastKnownStartUnix, lastKnownBootId from GENERIC_STORAGE       │
│      (This is the anchor for calculating all previous boots)    │
│                                                                  │
│   2. Scan flash sectors: read bootId and duration (last timeSec)│
│                                                                  │
│   3. Sort bootTable by bootId (ascending)                       │
│                                                                  │
│   4. Calculate startUnix chain BIDIRECTIONALLY from anchor:     │
│      ┌─────────────────────────────────────────────┐             │
│      │ Anchor: Boot[A].startUnix = lastKnownUnix   │             │
│      │                                             │             │
│      │ Backwards (older boots):                    │             │
│      │ Boot[A-1].startUnix = Boot[A].startUnix     │             │
│      │                     - Boot[A-1].duration    │             │
│      │                                             │             │
│      │ Forwards (newer offline boots):             │             │
│      │ Boot[A+1].startUnix = Boot[A].startUnix     │             │
│      │                     + Boot[A].duration      │             │
│      └─────────────────────────────────────────────┘             │
│                                                                  │
│   5. Create current boot entry:                                 │
│      current.startUnix = lastFlashBoot.startUnix                │
│                        + lastFlashBoot.duration                 │
│                                                                  │
│   Result: bootTable[] with all startUnix calculated            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Flash Log Entry Format (first entry per sector):**
- timeSec (4 bytes) - seconds since batch start
- bootId (1 byte) - boot session identifier

**Table Preparation Pseudocode:**
```cpp
void prepareBootTable() {
  uint8_t sectorCount = GENERIC_STORAGE.read(GENERIC_STORAGE_CURRENT_SECTOR) + 1;
  
  // Read lastKnown from GENERIC_STORAGE (anchor for timeline calculation)
  uint32_t lastKnownStartUnix;
  uint8_t lastKnownBootId;
  GENERIC_STORAGE.get(GENERIC_STORAGE_LAST_KNOWN_START_UNIX, lastKnownStartUnix);
  lastKnownBootId = GENERIC_STORAGE.read(GENERIC_STORAGE_LAST_KNOWN_BOOT_ID);
  
  // Allocate dynamic array based on flash sectors + 1 for current boot
  bootTable = (BootTimestamp*)malloc((sectorCount + 1) * sizeof(BootTimestamp));
  bootTableCount = 0;
  
  // Collect historical boots from flash
  for (uint8_t i = 0; i < sectorCount; i++) {
    uint32_t duration = readLastLogTimeSec(i);  // last entry's timeSec = duration
    uint8_t bootId = readLogBootId(i);
    
    if (bootId != 0xFF) {
      bootTable[bootTableCount].bootId = bootId;
      bootTable[bootTableCount].duration = duration;
      bootTableCount++;
    }
  }
  
  // Sort by bootId (ascending)
  sortBootTable();
  
  // Find anchor index
  int anchorIdx = -1;
  for (int i = 0; i < bootTableCount; i++) {
    if (bootTable[i].bootId == lastKnownBootId) {
      bootTable[i].startUnix = lastKnownStartUnix;
      anchorIdx = i;
      break;
    }
  }
  
  if (anchorIdx != -1) {
    // Bidirectional calculation from anchor
    // 1. Backwards (older boots)
    for (int i = anchorIdx - 1; i >= 0; i--) {
      bootTable[i].startUnix = bootTable[i+1].startUnix - bootTable[i].duration;
    }
    // 2. Forwards (newer offline boots)
    for (int i = anchorIdx + 1; i < bootTableCount; i++) {
      bootTable[i].startUnix = bootTable[i-1].startUnix + bootTable[i-1].duration;
    }
  }
  
  // For current boot (not in flash yet): calculate from previous boot
  uint8_t currentBootId = GENERIC_STORAGE.read(GENERIC_STORAGE_BOOT_ID);
  uint32_t currentStartUnix = 0;
  if (bootTableCount > 0) {
     currentStartUnix = bootTable[bootTableCount - 1].startUnix + bootTable[bootTableCount - 1].duration;
  } else {
     currentStartUnix = lastKnownStartUnix; // Fallback if flash is empty
  }
  
  addBootEntry(currentBootId, currentStartUnix); // Calculate initial estimate, gets corrected on sync
}
```

**Key Point:** bootTable is the source of truth for the MICROCONTROLLER. The timeline is dynamically calculated from the GENERIC_STORAGE anchor. It gets fully reconciled when the browser syncs.

**Duration Retrieval from Flash:**

The duration for each boot is retrieved from the **last log entry's timeSec** within that boot's flash sector:
- First log entry's timeSec is always 0 (relative to boot start)
- Last log entry's timeSec = uptime at end of that boot = duration
- This gives exact duration without needing to store it separately

```
Flash Sector N:
  [Entry 0] timeSec=0    ← start of boot
  [Entry 1] timeSec=120
  [Entry 2] timeSec=240
  ...
  [Entry 999] timeSec=3600  ← LAST entry, this is duration!
```

### Drift Calculation Logic

The MICROCONTROLLER's `millis()` runs slightly faster than real time (~5 seconds per hour). The drift is detected on browser sync:

#### On Browser Sync:

1. **Browser sends:** current boot's actual startUnix (from its history)

2. **MICROCONTROLLER calculates:** 
   ```
   currentUnix = Browser's current Unix time (from Date header or JS Date)
   currentUptime = millis() / 1000
   browserCalculatedStartUnix = currentUnix - currentUptime
   ```

3. **Compare with calculated value:**
   ```
   calculatedStartUnix = bootTable[currentBootId].startUnix  // Calculated on boot
   drift = abs(browserCalculatedStartUnix - calculatedStartUnix)
   ```

4. **If drift > 5 seconds:**
   ```
   // Update GENERIC_STORAGE with correct value
   GENERIC_STORAGE.put(GENERIC_STORAGE_LAST_KNOWN_START_UNIX, browserCalculatedStartUnix);
   
   // Update RAM table
   bootTable[currentBootId].startUnix = browserCalculatedStartUnix;
   ```

#### Drift Formula:
```
driftRate = driftSeconds / uptimeHours
// Typically ~5 seconds per hour
```

### Sync Workflow

#### 1. Dashboard Load (On Connect)

```
Browser                    MICROCONTROLLER
  │                           │
  │──── GET /timestamps ─────►│
  │                           │
  │◄─── current timestamps ───│
  │                           │
  │  (load Dexie logs)        │
  │                           │
  │──── PUT /timestamps ─────►│
  │    [browser history]      │
  │                           │
  │◄─── sync response ────────│
  │                           │
  │  (save to Dexie           │
  │   bootTimestamps,         │
  │   load bootStartCache)    │
  │                           │
  │──── syncTime ────────────►│
  │    (corrects MICROCONTROLLER's        │
  │     startUnix for         │
  │     current boot)         │
  │                           │
  │──── GET /timestamps ─────►│
  │    (re-fetch to update    │
  │     bootStartCache with   │
  │     corrected values)     │
  │                           │
  │◄─── updated timestamps ───│
  │                           │
  │  Update bootStartCache    │
  │  with corrected values    │
  │                           │
  │  Update UI with correct  │
  │  absolute timestamps     │
```

**Critical Detail:** After syncTime corrects the MICROCONTROLLER's boot startUnix, the browser
must re-fetch /timestamps to update its local bootStartCache. Without this
re-fetch, bootStartCache still holds the stale startUnix=0 for the current boot,
causing decodeLogs() to compute timestamps near epoch 0 (1970) — particularly
after a new batch where the index page is loaded for the first time.

#### 2. First Connection (No Browser History)

If browser has NO previous logs (first connection):

1. MICROCONTROLLER provides its assumed timestamps (startUnix=0 for current boot)
2. Browser syncs time via syncTime, which corrects the current boot's startUnix
3. Browser re-fetches /timestamps to update bootStartCache with corrected value
4. Browser displays correct timestamps
5. If another browser with history connects later, it propagates corrected timestamps

#### 3. MICROCONTROLLER Fresh Flash (No GENERIC_STORAGE Data)

If MICROCONTROLLER has no startTimestamp (factory reset):

1. Browser is source of truth
2. Take startTimestamp from browser's oldest log
3. Initialize MICROCONTROLLER's lastKnown values

#### 4. Conflict Resolution (Browser-Side Merge)

MICROCONTROLLER operates simply: `PUT /timestamps` blindly replaces its RAM table. Therefore, conflict resolution happens **inside the browser** before the PUT request:

1. Browser `GET /timestamps` to receive MICROCONTROLLER's best-guess calculated timeline.
2. Browser reads its Dexie DB.
3. Browser merges the two timelines using **Newer Wins**:
   - If `dexie.startUnix > esp.startUnix` → Keep Dexie value (typically true for older boots)
   - If `esp.startUnix > dexie.startUnix` → Accept MICROCONTROLLER value
   - If boot is missing in Dexie → Accept MICROCONTROLLER value
4. Browser `PUT /timestamps` the final reconciled timeline back to MICROCONTROLLER.

This allows a new browser (empty history) to safely adopt the MICROCONTROLLER's calculated timeline, while an established browser can correct any accumulated drift.

### Time Handling for Critical Operations

| Operation | Time Source | Mechanism |
|-----------|-------------|-----------|
| Operational day | `getElapsedSeconds() / 86400` | Derived on-demand |
| Scheduled intervals | millis() interval | `millis() - lastServoTurn` |
| Data logging | timeSec within boot | Relative within boot session |
| Uptime display | millis() | `millis() / 1000` |
| Chart display | Dexie absolute timestamps | Browser converts to relative |

**No stored elapsedSeconds anywhere** - All derived from bootTable + millis().

### Preserving Time Across Power Cycles

1. **On Boot - Table Preparation**:
   - Read lastKnownStartUnix, lastKnownBootId from GENERIC_STORAGE
   - Scan flash sectors to get bootId and duration for each boot
   - Calculate startUnix chain backwards from lastKnown (see pseudocode above)
   - This is essential - without GENERIC_STORAGE values, previous boots have no timestamps

2. **On Sync - Save to GENERIC_STORAGE only when drift > 5 seconds**:
   ```cpp
   // Only save when drift detected, not on every sync
   if (drift > 5) {
     GENERIC_STORAGE.write(GENERIC_STORAGE_LAST_KNOWN_BOOT_ID, currentBootId);
     GENERIC_STORAGE.put(GENERIC_STORAGE_LAST_KNOWN_START_UNIX, correctedStartUnix);
     GENERIC_STORAGE.commit();
   }
   // Duration retrieved from flash on next boot
   ```

**Key Change:** No more `elapsedSeconds++` every second. No more periodic GENERIC_STORAGE writes.
Time is derived on-demand from bootTable + millis().

### Edge Cases

#### Scenario 1: Browser disconnected for 1 week
```
- MICROCONTROLLER continues using millis() for uptime
- millis() drifts ~120 seconds (5 sec/hour × 24 hours)
- On reconnect: calculate drift from bootTable, apply correction
- Update bootTable[currentBootId].startUnix if drift > 5 sec
```

#### Scenario 2: Multiple browsers with different histories
```
- Browser A has 3 days of logs
- Browser B has only 1 day of logs
- On connect: B receives A's full bootTable via PUT /timestamps
- B's timestamps are updated to match A's
- B now has correct historical data
```

#### Scenario 3: User manually adjusts day
```
- User clicks "Day -1" in settings
- MICROCONTROLLER: startTimestamp += 86400 (shifts timeline forward, reducing elapsed day)
- MICROCONTROLLER: writes updated startTimestamp to GENERIC_STORAGE (DeviceSettings)
- On next sync: browser receives adjusted timeline
```

#### Scenario 4: MICROCONTROLLER boots multiple times before browser connects
```
- Boot 1: bootTable[0] = {bootId:1, startUnix:0, duration:0}
- Boot 2: bootTable[1] = {bootId:2, startUnix:0, duration:0}  
- Boot 3: bootTable[2] = {bootId:3, startUnix:0, duration:0}
- MICROCONTROLLER continues functioning with millis() for all timing
- Browser connects: sends real timestamps from its history
- MICROCONTROLLER: bootTable replaced with correct startUnix values
- Display shows correct Day after sync
```

#### Scenario 5: Start new batch
```
- User clicks "Start New Batch"
- Browser sends: action=newBatch&timestamp=<currentUnix>
- MICROCONTROLLER: new boot entry in bootTable with new bootId
- MICROCONTROLLER: bootTable[newBootId].startUnix = currentUnix, duration = 0
- MICROCONTROLLER: writes currentUnix to startTimestamp in GENERIC_STORAGE (DeviceSettings)
- MICROCONTROLLER: updates lastKnown values in GENERIC_STORAGE
```

### Advantages

1. **No External RTC Hardware** - Reduce BOM cost and complexity
2. **Virtual RTC via Browser** - Accurate absolute timestamps when connected
3. **Drift Compensation** - Automatic correction for millis() drift
4. **Multi-browser Support** - History propagates to new browsers
5. **Offline Operation** - MICROCONTROLLER continues working without browser

### Limitations

1. **Requires Initial Sync** - Must connect to browser for absolute time
2. **Drift Accumulation** - millis() drifts ~5 sec/hour, corrected on sync
3. **First Boot Has No History** - Relies on user to start batch correctly
4. **Browser Time Dependent** - Assumes browser has correct system time

### Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Time Synchronization                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   Step 1: MICROCONTROLLER → Browser (GET /timestamps)                          │
│   ┌─────────┐                    ┌──────────┐                      │
│   │ MICROCONTROLLER │ ── bootId=5 ──────►│  Browser │                      │
│   │         │    startUnix=1704  │          │                      │
│   └─────────┘                    └────┬─────┘                      │
│                                        │                            │
│   Step 2: Browser loads Dexie history                              │
│   ┌──────────┐    logs from    ┌──────────┐                      │
│   │  Dexie   │ ◄── query ────── │  Browser │                      │
│   │  Logs    │ ── returns ────►│          │                      │
│   └──────────┘    [all logs]   └────┬─────┘                      │
│                                        │                            │
│   Step 3: Browser calculates boot history                          │
│   ┌──────────┐    derive from   ┌──────────┐                      │
│   │  Boot    │ ◄── logs ─────── │  Browser │                      │
│   │  History │ ── returns ────► │          │                      │
│   │ [5 rows] │                  └────┬─────┘                      │
│                                        │                            │
│   Step 4: Browser → MICROCONTROLLER (PUT /timestamps)                         │
│   ┌──────────┐ ── history[5] ─►┌─────────┐                      │
│   │  Browser │                  │ MICROCONTROLLER  │                      │
│   │          │ ◄─ synced=true ─ │          │                      │
│   └──────────┘                  └─────────┘                      │
│                                                                     │
│   Step 5: MICROCONTROLLER updates GENERIC_STORAGE                                       │
│   ┌─────────┐   write to    ┌──────────┐                          │
│   │ MICROCONTROLLER │ ◄── GENERIC_STORAGE ── │  GENERIC_STORAGE  │                          │
│   └─────────┘              └──────────┘                          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Implementation Checklist

- [ ] Add `lastKnownBootId` and `lastKnownStartUnix` to GENERIC_STORAGE
- [ ] Implement GET /timestamps endpoint
- [ ] Implement PUT /timestamps endpoint
- [ ] Add drift calculation on every sync
- [ ] Update GENERIC_STORAGE when drift > 5 seconds
- [ ] On dashboard load: fetch MICROCONTROLLER timestamps → load Dexie → sync
- [ ] Handle first-connection scenario
- [ ] Handle fresh-flash scenario
- [ ] Add conflict resolution (newer wins)
- [ ] Test offline drift over 24 hours
- [ ] Test multi-browser propagation
- [x] Re-fetch /timestamps after syncTime to refresh bootStartCache (fixes epoch-0 decode on first load after new batch)

### Version History

| Version | Changes |
|---------|---------|
| 1.3.31 | Fixed bootStartCache staleness after syncTime — re-fetch /timestamps post-sync to avoid epoch-0 timestamps on first load after new batch |
| 1.0 | Initial SAT architecture with drift compensation |