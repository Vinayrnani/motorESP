#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include <Arduino.h>
#include <time.h>
#include <EEPROM.h>
#include "sat_manager.h"
#include "logging.h"
#include "wifi_manager.h"

// Non-blocking NTP sync — feeds into SAT anchor via the same correction path
// the browser's syncTime action uses. Retries up to 3 times with 10s timeout
// each. If all retries fail or browser syncs first, does nothing — SAT falls
// back to browser sync as before.
//
// Call handleNtpSync() from loop() after handleWiFi().

enum NtpState { NTP_IDLE, NTP_PENDING, NTP_DONE, NTP_FAILED };

inline void handleNtpSync() {
  static NtpState state = NTP_IDLE;
  static uint8_t retries = 0;
  static unsigned long stateTime = 0;

  switch (state) {
    case NTP_IDLE:
      if (isWiFiConnected()) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        state = NTP_PENDING;
        stateTime = millis();
      }
      break;

    case NTP_PENDING:
      if (isWiFiConnected()) {
        time_t now = time(nullptr);
        if (now > 0) {
          // NTP got a valid time — same correction as browser syncTime
          uint32_t bootUptime = getBootUptime();
          uint32_t ntpStartUnix = (uint32_t)now - bootUptime;
          for (int i = 0; i < bootSessionCount; i++) {
            if (bootSessions[i].bootId == currentBootId) {
              int32_t drift = abs((int32_t)(ntpStartUnix - bootSessions[i].startUnix));
              if (drift > 5) {
                bootSessions[i].startUnix = ntpStartUnix;
                EEPROM.put(EEPROM_LAST_KNOWN_START_UNIX, ntpStartUnix);
                EEPROM.write(EEPROM_LAST_KNOWN_BOOT_ID, currentBootId);
                EEPROM.commit();
              }
              break;
            }
          }
          state = NTP_DONE;
        } else if (millis() - stateTime > 10000) {
          // 10s timeout — retry
          retries++;
          if (retries < 3) {
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            stateTime = millis();
          } else {
            state = NTP_FAILED;
          }
        }
      } else {
        // WiFi dropped while waiting — reset to try again when it reconnects
        state = NTP_IDLE;
      }
      break;

    case NTP_DONE:
    case NTP_FAILED:
      // Terminal states — do nothing
      break;
  }
}

#endif
