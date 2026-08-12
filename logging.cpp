#include "logging.h"
#include <EEPROM.h>

uint8_t currentBootId = 0;
uint16_t currentSector = 0;
uint16_t currentOffset = 0;
uint16_t startSector = 0;
uint32_t logsInCurrentBoot = 0;
uint32_t totalLogsCached = 0;
unsigned long lastLogTime = 0;

BootSession* bootSessions = nullptr;
int bootSessionCount = 0;
int bootSessionCapacity = 0;

PZEMData lastLoggedPZEM = {0};
uint8_t lastLoggedStates = 0xFF;
float lastLoggedEnergyWh = 0;

// PZEM energy register wraps at 9999 kWh = 9,999,000 Wh
#define ENERGY_WRAP_WH 9999000UL

bool isMetaEntry(const LogEntry &entry) {
  return (entry.pf == META_SECTOR_POINTER || entry.pf == META_CORRECTION);
}

void writeMetaEntry() {
  LogEntry entry;
  entry.timeSec = startSector;
  entry.voltage = 0;
  entry.current = 0;
  entry.energyDelta = 0;
  entry.pf = META_SECTOR_POINTER;
  entry.states = 0;
  entry.bootId = currentBootId;

  uint32_t writeAddr = FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE);
  ESP.flashWrite(writeAddr, (uint32_t*)&entry, sizeof(LogEntry));
}

void initSectorPointers() {
  bool firstBoot = true;
  for (int s = 0; s < FLASH_NUM_SECTORS; s++) {
    uint8_t firstByte;
    ESP.flashRead(FLASH_LOG_START + (s * FLASH_SECTOR_SIZE), &firstByte, 1);
    if (firstByte != 0xFF) { firstBoot = false; break; }
  }

  if (firstBoot) {
    currentSector = 0;
  } else {
    for (int s = 0; s < FLASH_NUM_SECTORS; s++) {
      uint8_t firstByte;
      ESP.flashRead(FLASH_LOG_START + (s * FLASH_SECTOR_SIZE), &firstByte, 1);
      if (firstByte == 0xFF) {
        currentSector = (s + FLASH_NUM_SECTORS - 1) % FLASH_NUM_SECTORS;
        break;
      }
      if (s == FLASH_NUM_SECTORS - 1) {
        currentSector = 0;
      }
    }
  }

  LogEntry metaEntry;
  ESP.flashRead(FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE), (uint32_t*)&metaEntry, sizeof(LogEntry));

  if (isMetaEntry(metaEntry)) {
    startSector = metaEntry.timeSec;
  } else {
    uint16_t prevSector = (currentSector + FLASH_NUM_SECTORS - 1) % FLASH_NUM_SECTORS;
    ESP.flashRead(FLASH_LOG_START + (prevSector * FLASH_SECTOR_SIZE), (uint32_t*)&metaEntry, sizeof(LogEntry));
    if (isMetaEntry(metaEntry)) {
      startSector = metaEntry.timeSec;
    } else {
      startSector = currentSector;
    }
  }

  currentOffset = 0;
  for (int i = 0; i < LOGS_PER_SECTOR; i++) {
    uint32_t addr = FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE) + (i * sizeof(LogEntry));
    uint32_t timeSec;
    ESP.flashRead(addr, &timeSec, 4);
    if (timeSec == 0xFFFFFFFF) {
      currentOffset = i;
      break;
    }
  }
}

void initLogging(uint8_t bootId) {
  currentBootId = bootId;
  logsInCurrentBoot = 0;

  if (currentOffset == 0) {
    ESP.flashEraseSector((FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE)) / FLASH_SECTOR_SIZE);
    writeMetaEntry();
    currentOffset = 1;
  }

  lastLogTime = 0;
  lastLoggedPZEM = {0};
  lastLoggedStates = 0xFF;
  lastLoggedEnergyWh = 0;

  bootSessionCount = 0;
  bootSessionCapacity = 16;
  if (bootSessions != nullptr) free(bootSessions);
  bootSessions = (BootSession*)malloc(bootSessionCapacity * sizeof(BootSession));

  int s = startSector;
  int o = 0;
  totalLogsCached = 0;
  while (!(s == currentSector && o == currentOffset)) {
    ESP.wdtFeed();
    uint32_t addr = FLASH_LOG_START + (s * FLASH_SECTOR_SIZE) + (o * sizeof(LogEntry));
    LogEntry entry;
    ESP.flashRead(addr, (uint32_t*)&entry, sizeof(LogEntry));

    if (entry.timeSec == 0xFFFFFFFF) break;

    if (!isMetaEntry(entry)) {
      totalLogsCached++;
    }

    if (s != startSector || o != 0) {
      int foundIdx = -1;
      for (int i = 0; i < bootSessionCount; i++) {
        if (bootSessions[i].bootId == entry.bootId) {
          foundIdx = i;
          break;
        }
      }

      if (foundIdx == -1) {
        if (bootSessionCount >= bootSessionCapacity) {
          bootSessionCapacity *= 2;
          bootSessions = (BootSession*)realloc(bootSessions, bootSessionCapacity * sizeof(BootSession));
        }
        bootSessions[bootSessionCount].bootId = entry.bootId;
        bootSessions[bootSessionCount].sector = s;
        bootSessions[bootSessionCount].offset = o;
        bootSessions[bootSessionCount].duration = entry.timeSec;
        bootSessions[bootSessionCount].startUnix = 0;
        foundIdx = bootSessionCount;
        bootSessionCount++;
      } else {
        if (entry.timeSec > bootSessions[foundIdx].duration) {
          bootSessions[foundIdx].duration = entry.timeSec;
        }
      }
    }

    o++;
    if (o >= LOGS_PER_SECTOR) {
      o = 0;
      s = (s + 1) % FLASH_NUM_SECTORS;
    }
  }

  int curIdx = -1;
  for (int i = 0; i < bootSessionCount; i++) {
    if (bootSessions[i].bootId == currentBootId) {
      curIdx = i;
      break;
    }
  }
  if (curIdx == -1) {
    if (bootSessionCount >= bootSessionCapacity) {
      bootSessionCapacity = bootSessionCapacity == 0 ? 16 : bootSessionCapacity * 2;
      bootSessions = (BootSession*)realloc(bootSessions, bootSessionCapacity * sizeof(BootSession));
    }
    bootSessions[bootSessionCount].bootId = currentBootId;
    bootSessions[bootSessionCount].sector = currentSector;
    bootSessions[bootSessionCount].offset = currentOffset;
    bootSessions[bootSessionCount].duration = 0;
    bootSessions[bootSessionCount].startUnix = 0;
    bootSessionCount++;
  }
}

bool logPumpData(const PZEMData &data, uint8_t states, unsigned long forceInterval) {
  bool significant = false;
  if (states != lastLoggedStates) significant = true;
  else if (lastLogTime == 0 || (millis() - lastLogTime >= forceInterval)) significant = true;

  if (!significant) return false;

  // Energy delta since last entry (handles PZEM 9999kWh wrap and resets)
  float currentEnergy = data.energy;
  uint8_t energyDelta = 0;
  if (lastLoggedStates != 0xFF) {
    long delta = (long)(currentEnergy - lastLoggedEnergyWh);
    if (delta < 0) delta = (long)(currentEnergy + ENERGY_WRAP_WH - lastLoggedEnergyWh);
    if (delta < 0) delta = 0;
    if (delta > 255) delta = 255;
    energyDelta = (uint8_t)delta;
  }

  LogEntry entry;
  entry.timeSec = millis() / 1000;
  entry.voltage = (uint8_t)constrain((int)(data.voltage - 200.0f + 0.5f), 0, 255);
  entry.current = (uint16_t)(data.current * 10.0f + 0.5f);
  entry.energyDelta = energyDelta;
  entry.pf = (uint8_t)(data.pf * 100.0f + 0.5f);
  entry.states = states;
  entry.bootId = currentBootId;

  int curIdx = -1;
  for (int i = 0; i < bootSessionCount; i++) {
    if (bootSessions[i].bootId == currentBootId) {
      curIdx = i;
      break;
    }
  }
  if (curIdx == -1) {
    if (bootSessionCount >= bootSessionCapacity) {
      bootSessionCapacity = bootSessionCapacity == 0 ? 16 : bootSessionCapacity * 2;
      bootSessions = (BootSession*)realloc(bootSessions, bootSessionCapacity * sizeof(BootSession));
    }
    bootSessions[bootSessionCount].bootId = currentBootId;
    bootSessions[bootSessionCount].sector = currentSector;
    bootSessions[bootSessionCount].offset = currentOffset;
    bootSessions[bootSessionCount].duration = entry.timeSec;
    bootSessions[bootSessionCount].startUnix = 0;
    bootSessionCount++;
  } else {
    if (entry.timeSec > bootSessions[curIdx].duration) {
      bootSessions[curIdx].duration = entry.timeSec;
    }
  }

  uint32_t writeAddr = FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE) + (currentOffset * sizeof(LogEntry));
  ESP.flashWrite(writeAddr, (uint32_t*)&entry, sizeof(LogEntry));

  lastLoggedPZEM = data;
  lastLoggedStates = states;
  lastLoggedEnergyWh = currentEnergy;
  lastLogTime = millis();

  currentOffset++;
  logsInCurrentBoot++;
  totalLogsCached++;

  if (currentOffset >= LOGS_PER_SECTOR) {
    currentSector = (currentSector + 1) % FLASH_NUM_SECTORS;
    ESP.flashEraseSector((FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE)) / FLASH_SECTOR_SIZE);

    writeMetaEntry();

    int nextSector = (currentSector + 1) % FLASH_NUM_SECTORS;
    uint8_t nextFirstByte;
    ESP.flashRead(FLASH_LOG_START + (nextSector * FLASH_SECTOR_SIZE), &nextFirstByte, 1);
    if (nextFirstByte != 0xFF) {
      ESP.flashEraseSector((FLASH_LOG_START + (nextSector * FLASH_SECTOR_SIZE)) / FLASH_SECTOR_SIZE);
    }

    if (currentSector == startSector) {
      startSector = (startSector + 1) % FLASH_NUM_SECTORS;
    }
    currentOffset = 1;
  }

  return true;
}

int getLogHex(String& hex, int maxEntries, uint8_t sinceBootId, uint32_t sinceTimeSec) {
  hex.reserve(maxEntries * 22);
  int sent = 0;

  int targetSector = -1;
  int targetOffset = -1;

  if (sinceBootId == 0 && sinceTimeSec == 0) {
    if (bootSessionCount > 0) {
      targetSector = bootSessions[0].sector;
      targetOffset = bootSessions[0].offset;
    }
  } else {
    int bootStartS = -1, bootStartO = -1;
    for (int i = bootSessionCount - 1; i >= 0; i--) {
      if (bootSessions[i].bootId == sinceBootId) {
        bootStartS = bootSessions[i].sector;
        bootStartO = bootSessions[i].offset;
        break;
      }
    }

    if (bootStartS == -1) {
      int scanS = startSector;
      int scanO = 0;
      while (!(scanS == currentSector && scanO == currentOffset)) {
        ESP.wdtFeed();
        uint32_t addr = FLASH_LOG_START + (scanS * FLASH_SECTOR_SIZE) + (scanO * sizeof(LogEntry));
        LogEntry entry;
        ESP.flashRead(addr, (uint32_t*)&entry, sizeof(LogEntry));
        if (entry.timeSec == 0xFFFFFFFF) break;
        if (!isMetaEntry(entry) && entry.bootId == sinceBootId && entry.timeSec == sinceTimeSec) {
          scanO++;
          if (scanO >= LOGS_PER_SECTOR) { scanO = 0; scanS = (scanS + 1) % FLASH_NUM_SECTORS; }
          targetSector = scanS;
          targetOffset = scanO;
          break;
        }
        scanO++;
        if (scanO >= LOGS_PER_SECTOR) {
          scanS = (scanS + 1) % FLASH_NUM_SECTORS;
          scanO = 0;
        }
      }
    } else {
      int s = bootStartS;
      int o = bootStartO;
      while (!(s == currentSector && o == currentOffset)) {
        ESP.wdtFeed();
        uint32_t addr = FLASH_LOG_START + (s * FLASH_SECTOR_SIZE) + (o * sizeof(LogEntry));
        LogEntry entry;
        ESP.flashRead(addr, (uint32_t*)&entry, sizeof(LogEntry));
        if (entry.timeSec == 0xFFFFFFFF) break;
        if (!isMetaEntry(entry) && entry.bootId == sinceBootId && entry.timeSec == sinceTimeSec) {
          o++;
          if (o >= LOGS_PER_SECTOR) { o = 0; s = (s + 1) % FLASH_NUM_SECTORS; }
          targetSector = s;
          targetOffset = o;
          break;
        }
        o++;
        if (o >= LOGS_PER_SECTOR) { o = 0; s = (s + 1) % FLASH_NUM_SECTORS; }
      }
    }
  }

  if (targetSector == -1) return 0;

  int currentS = targetSector;
  int currentO = targetOffset;

  while (sent < maxEntries && !(currentS == currentSector && currentO == currentOffset)) {
    if (sent % 50 == 0) ESP.wdtFeed();

    uint32_t addr = FLASH_LOG_START + (currentS * FLASH_SECTOR_SIZE) + (currentO * sizeof(LogEntry));
    LogEntry entry;
    ESP.flashRead(addr, (uint32_t*)&entry, sizeof(LogEntry));

    if (entry.timeSec != 0xFFFFFFFF) {
      if (!isMetaEntry(entry)) {
        uint8_t* ptr = (uint8_t*)&entry;
        for (int j = 0; j < sizeof(LogEntry); j++) {
          if (ptr[j] < 16) hex += "0";
          hex += String(ptr[j], HEX);
        }
        sent++;
      }
    } else {
      break;
    }

    currentO++;
    if (currentO >= LOGS_PER_SECTOR) {
      currentO = 0;
      currentS = (currentS + 1) % FLASH_NUM_SECTORS;
    }
  }

  return sent;
}

void clearLogs() {
  startSector = (currentSector + 1) % FLASH_NUM_SECTORS;
  currentSector = startSector;

  ESP.flashEraseSector((FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE)) / FLASH_SECTOR_SIZE);

  uint16_t sentinelSector = (currentSector + 1) % FLASH_NUM_SECTORS;
  ESP.flashEraseSector((FLASH_LOG_START + (sentinelSector * FLASH_SECTOR_SIZE)) / FLASH_SECTOR_SIZE);

  LogEntry entry;
  entry.timeSec = startSector;
  entry.voltage = 0;
  entry.current = 0;
  entry.energyDelta = 0;
  entry.pf = META_SECTOR_POINTER;
  entry.states = 0;
  entry.bootId = 0;
  uint32_t addr = FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE);
  ESP.flashWrite(addr, (uint32_t*)&entry, sizeof(LogEntry));
  currentOffset = 1;

  lastLoggedPZEM = {0};
  lastLoggedStates = 0xFF;
  lastLoggedEnergyWh = 0;
  totalLogsCached = 0;
  bootSessionCount = 0;
}

int getTotalLogs() {
  return totalLogsCached;
}

void writeCorrectionLog(uint8_t bootId, uint32_t duration) {
  LogEntry entry;
  entry.timeSec = duration;
  entry.voltage = 0;
  entry.current = 0;
  entry.energyDelta = 0;
  entry.pf = META_CORRECTION;
  entry.states = lastLoggedStates;
  entry.bootId = bootId;

  uint32_t writeAddr = FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE) + (currentOffset * sizeof(LogEntry));
  ESP.flashWrite(writeAddr, (uint32_t*)&entry, sizeof(LogEntry));

  currentOffset++;
  if (currentOffset >= LOGS_PER_SECTOR) {
    currentSector = (currentSector + 1) % FLASH_NUM_SECTORS;
    ESP.flashEraseSector((FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE)) / FLASH_SECTOR_SIZE);

    writeMetaEntry();

    int nextSector = (currentSector + 1) % FLASH_NUM_SECTORS;
    uint8_t nextFirstByte;
    ESP.flashRead(FLASH_LOG_START + (nextSector * FLASH_SECTOR_SIZE), &nextFirstByte, 1);
    if (nextFirstByte != 0xFF) {
      ESP.flashEraseSector((FLASH_LOG_START + (nextSector * FLASH_SECTOR_SIZE)) / FLASH_SECTOR_SIZE);
    }

    if (currentSector == startSector) {
      startSector = (startSector + 1) % FLASH_NUM_SECTORS;
    }
    currentOffset = 1;
  }
}

uint8_t recoverBootIdFromFlash() {
  int s = currentSector;
  int o = currentOffset;
  bool currentSectorExhausted = false;

  if (o == 0) {
    o = LOGS_PER_SECTOR - 1;
    s = (s + FLASH_NUM_SECTORS - 1) % FLASH_NUM_SECTORS;
  } else {
    o--;
  }

  for (int scanned = 0; scanned < 200; scanned++) {
    if (!currentSectorExhausted && s != currentSector) {
      currentSectorExhausted = true;
      LogEntry meta;
      ESP.flashRead(FLASH_LOG_START + (currentSector * FLASH_SECTOR_SIZE),
                    (uint32_t*)&meta, sizeof(LogEntry));
      if (isMetaEntry(meta) && meta.bootId == 0) {
        return 0;
      }
    }

    uint32_t addr = FLASH_LOG_START + (s * FLASH_SECTOR_SIZE) + (o * sizeof(LogEntry));
    LogEntry entry;
    ESP.flashRead(addr, (uint32_t*)&entry, sizeof(LogEntry));

    if (entry.timeSec != 0xFFFFFFFF && !isMetaEntry(entry)) {
      return entry.bootId + 1;
    }

    if (o == 0) {
      o = LOGS_PER_SECTOR - 1;
      s = (s + FLASH_NUM_SECTORS - 1) % FLASH_NUM_SECTORS;
    } else {
      o--;
    }
  }

  return 0;
}
