#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include "pzem_sensor.h"

// ============================================
// LOG ENTRY — 11 bytes packed
// ============================================
struct __attribute__((packed)) LogEntry {
  uint32_t timeSec;     // 4 bytes - seconds since boot
  uint8_t  voltage;     // 1 byte  - offset from 200V (40 = 240V, 90 = 290V)
  uint16_t current;     // 2 bytes - A × 10 (96 = 9.6A, 580 = 58.0A)
  uint8_t  energyDelta; // 1 byte  - Wh since last entry (0-255)
  uint8_t  pf;          // 1 byte  - PF × 100 (65 = 0.65), 0xFE/0xFF = meta marker
  uint8_t  states;      // 1 byte  - status code (SC_* below)
  uint8_t  bootId;      // 1 byte  - boot session ID
};

// ============================================
// FLASH STORAGE PARAMETERS
// ============================================
#define FLASH_LOG_START 0x200000
#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE 4096
#endif
#define FLASH_NUM_SECTORS 256
#define LOGS_PER_SECTOR (FLASH_SECTOR_SIZE / sizeof(LogEntry))  // 372
#define MAX_LOG_ENTRIES (LOGS_PER_SECTOR * FLASH_NUM_SECTORS)   // 95232

// ============================================
// EEPROM ADDRESSES
// ============================================
#define EEPROM_CURRENT_SECTOR 32
#define EEPROM_START_SECTOR 34

// ============================================
// META ENTRY MARKERS
// ============================================
#define META_SECTOR_POINTER 0xFE  // pf = 0xFE means meta entry (first of sector)
#define META_CORRECTION     0xFF  // pf = 0xFF means SAT correction entry

// ============================================
// TRIP BITMASK (activeTrips — separate from log status codes)
// ============================================
#define STATE_TRIP_OVERCURRENT 0x02
#define STATE_TRIP_DRYRUN      0x04
#define STATE_TRIP_OVERVOLT    0x08
#define STATE_TRIP_UNDERVOLT   0x10
#define STATE_PZEM_FAULT       0x40
#define STATE_START_FAIL       0x80

// ============================================
// STATUS CODES (LogEntry.states byte)
// ============================================
#define SC_OFF                       0
// STARTING (1-6)
#define SC_STARTING_MANUAL           1
#define SC_STARTING_SCHEDULE         2
#define SC_STARTING_PHYSICAL         3
#define SC_STARTING_RETRY_MANUAL     4
#define SC_STARTING_RETRY_SCHED      5
#define SC_STARTING_RETRY_PHYS       6
// RUNNING (7-16)
#define SC_RUNNING_MANUAL            7
#define SC_RUNNING_SCHEDULE          8
#define SC_RUNNING_PHYSICAL          9
#define SC_RUNNING_RETRY_MANUAL      10
#define SC_RUNNING_RETRY_SCHED       11
#define SC_RUNNING_RETRY_PHYS        12
#define SC_RUNNING_MERGED_MANUAL     13
#define SC_RUNNING_MERGED_PHYS       14
#define SC_RUNNING_RETRY_MERGE_MANUAL 15
#define SC_RUNNING_RETRY_MERGE_PHYS  16
// STOPPING (17-19)
#define SC_STOPPING_MANUAL           17
#define SC_STOPPING_SCHEDULE         18
#define SC_STOPPING_MAXRUN           19
// TRIPPED (20-25)
#define SC_TRIPPED_OC                20
#define SC_TRIPPED_DRYRUN            21
#define SC_TRIPPED_OVERVOLT          22
#define SC_TRIPPED_UNDERVOLT         23
#define SC_TRIPPED_PZEM              24
#define SC_TRIPPED_STARTFAIL         25

// ============================================
// GLOBALS
// ============================================
extern uint8_t currentBootId;
extern uint16_t currentSector;
extern uint16_t currentOffset;
extern uint16_t startSector;
extern uint32_t logsInCurrentBoot;
extern unsigned long lastLogTime;

struct BootSession {
  uint8_t bootId;
  uint16_t sector;
  uint16_t offset;
  uint32_t duration;
  uint32_t startUnix;
  uint32_t totalWh;
};

extern BootSession* bootSessions;
extern int bootSessionCount;
extern int bootSessionCapacity;

extern PZEMData lastLoggedPZEM;
extern uint8_t lastLoggedStates;

// ============================================
// FUNCTIONS
// ============================================
void initSectorPointers();
uint8_t recoverBootIdFromFlash();
void initLogging(uint8_t bootId);
bool logPumpData(const PZEMData &data, uint8_t states, unsigned long forceInterval = 0);
bool isMetaEntry(const LogEntry &entry);
bool validLogEntry(const LogEntry &entry);
int getLogHex(String& hex, int maxEntries = 200, uint8_t sinceBootId = 0, uint32_t sinceTimeSec = 0);
int getLogHexBackward(String& hex, int maxEntries = 200, uint8_t olderThanBootId = 0, uint32_t olderThanTimeSec = 0);
int getLogHexFromBoot(String& hex, int maxEntries = 200, uint8_t bootId = 0, uint32_t afterTimeSec = 0);
int getTotalLogs();
void clearLogs();
void writeCorrectionLog(uint8_t bootId, uint32_t duration);

#endif
