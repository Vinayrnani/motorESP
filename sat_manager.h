#ifndef SAT_MANAGER_H
#define SAT_MANAGER_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "logging.h"

// EEPROM addresses for SAT
#define EEPROM_LAST_KNOWN_BOOT_ID 15
#define EEPROM_LAST_KNOWN_START_UNIX 16

extern uint32_t batchStartUnix;

extern ESP8266WebServer server;

void prepareBootTable();
uint32_t getBootUptime();
uint32_t getElapsedSeconds();
uint32_t getCurrentDay();
void handleTimestamps();

#endif
