#include "sat_manager.h"
#include <EEPROM.h>

void prepareBootTable() {
  uint32_t lastKnownStartUnix = 0;
  uint8_t lastKnownBootId = EEPROM.read(EEPROM_LAST_KNOWN_BOOT_ID);
  EEPROM.get(EEPROM_LAST_KNOWN_START_UNIX, lastKnownStartUnix);
  if (lastKnownStartUnix > 2000000000) lastKnownStartUnix = 0;

  int anchorIdx = -1;
  for (int i = 0; i < bootSessionCount; i++) {
    bootSessions[i].startUnix = 0;
    if (bootSessions[i].bootId == lastKnownBootId) {
      bootSessions[i].startUnix = lastKnownStartUnix;
      anchorIdx = i;
    }
  }

  if (anchorIdx >= 0) {
    for (int i = anchorIdx - 1; i >= 0; i--)
      bootSessions[i].startUnix = bootSessions[i + 1].startUnix - bootSessions[i].duration;
    for (int i = anchorIdx + 1; i < bootSessionCount; i++)
      bootSessions[i].startUnix = bootSessions[i - 1].startUnix + bootSessions[i - 1].duration;
  }

  uint8_t curBootId = currentBootId;
  uint32_t curStart = 0;
  if (bootSessionCount > 0)
    curStart = bootSessions[bootSessionCount - 1].startUnix + bootSessions[bootSessionCount - 1].duration;
  else if (lastKnownStartUnix > 0)
    curStart = lastKnownStartUnix;

  // Ensure current session exists correctly
  if (bootSessionCount > 0 && bootSessions[bootSessionCount - 1].bootId == curBootId) {
    bootSessions[bootSessionCount - 1].startUnix = curStart;
  }

}

uint32_t getBootUptime() {
  return millis() / 1000;
}

uint32_t getElapsedSeconds() {
  if (bootSessions == NULL || bootSessionCount == 0) return 0;
  uint32_t currentStartUnix = bootSessions[bootSessionCount - 1].startUnix;
  if (currentStartUnix == 0 || batchStartUnix == 0 || currentStartUnix < batchStartUnix) {
    return getBootUptime();
  }
  return currentStartUnix + getBootUptime() - batchStartUnix;
}

uint32_t getCurrentDay() {
  return getElapsedSeconds() / 86400;
}

void handleTimestamps() {
  if (server.method() == HTTP_GET) {
    String json = "{\"currentBootId\":" + String(currentBootId) +
                  ",\"bootUptimeSec\":" + String(getBootUptime()) +
                  ",\"bootTable\":[";
    for (int i = 0; i < bootSessionCount; i++) {
      if (i > 0) json += ",";
      json += "{\"bootId\":" + String(bootSessions[i].bootId) +
              ",\"startUnix\":" + String(bootSessions[i].startUnix) + "}";
    }
    json += "]}";
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.send(200, "application/json", json);
    return;
  }

  if (server.method() == HTTP_PUT) {
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", "{\"synced\":false}");
      return;
    }

    String body = server.arg("plain");
    int count = 0, pos = 0;
    while (true) {
      int idx = body.indexOf("\"bootId\"", pos);
      if (idx == -1) break;
      count++;
      pos = idx + 8;
    }
    if (count == 0) { server.send(400, "application/json", "{\"synced\":false}"); return; }
    
    pos = 0;
    uint32_t prevStartUnix = 0;
    uint8_t prevBootId = 0;
    
    for (int i = 0; i < count; i++) {
      int bidx = body.indexOf("\"bootId\"", pos);
      int suidx = body.indexOf("\"startUnix\"", pos);
      int didx = body.indexOf("\"duration\"", pos);
      if (bidx == -1 || suidx == -1) break;

      int bstart = body.indexOf(':', bidx + 8) + 1;
      int bend = body.indexOf(',', bstart);
      if (bend == -1) bend = body.indexOf('}', bstart);
      uint8_t bootId = (uint8_t)body.substring(bstart, bend).toInt();

      int sustart = body.indexOf(':', suidx + 10) + 1;
      int suend = body.indexOf(',', sustart);
      if (suend == -1) suend = body.indexOf('}', sustart);
      uint32_t startUnix = (uint32_t)body.substring(sustart, suend).toInt();

      uint32_t browserDuration = 0;
      if (didx != -1 && didx < body.indexOf('}', bidx)) {
        int dstart = body.indexOf(':', didx + 10) + 1;
        int dend = body.indexOf(',', dstart);
        if (dend == -1) dend = body.indexOf('}', dstart);
        browserDuration = (uint32_t)body.substring(dstart, dend).toInt();
      }

      if (bootId == currentBootId) prevStartUnix = startUnix;
      if (bootId == currentBootId) prevBootId = bootId;

      int foundIdx = -1;
      for (int j = 0; j < bootSessionCount; j++) {
        if (bootSessions[j].bootId == bootId) {
          foundIdx = j;
          break;
        }
      }

      if (foundIdx != -1) {
        bootSessions[foundIdx].startUnix = startUnix;
        if (browserDuration > 0 && browserDuration > bootSessions[foundIdx].duration) {
          int32_t durationDrift = abs((int32_t)browserDuration - (int32_t)bootSessions[foundIdx].duration);
          if (durationDrift > 5) {
            writeCorrectionLog(bootId, browserDuration);
            bootSessions[foundIdx].duration = browserDuration;
          }
        }
      }
      pos = body.indexOf('}', bidx) + 1;
    }

    // Update EEPROM anchor only if drift > 5s on current boot
    if (prevStartUnix > 0 && prevBootId == currentBootId) {
      uint32_t espBootStart = 0;
      for (int i = 0; i < bootSessionCount; i++) {
        if (bootSessions[i].bootId == currentBootId) { espBootStart = bootSessions[i].startUnix; break; }
      }
      int32_t drift = abs((int32_t)(prevStartUnix - espBootStart));
      if (drift > 5) {
        EEPROM.put(EEPROM_LAST_KNOWN_START_UNIX, prevStartUnix);
        EEPROM.write(EEPROM_LAST_KNOWN_BOOT_ID, currentBootId);
        EEPROM.commit();
      }
    }

    server.send(200, "application/json",
      "{\"synced\":true,\"entriesStored\":" + String(bootSessionCount) + "}");
    return;
  }

  server.send(405, "text/plain", "Method not allowed");
}
