#ifndef UPDATES_H
#define UPDATES_H

#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>

#define FIRMWARE_URL "https://github.com/Vinayrnani/motorESP/releases/latest/download/firmware.bin"
#define VERSION_URL "https://api.github.com/repos/Vinayrnani/motorESP/releases/latest"
#define FIRMWARE_VERSION "1.0.0"

extern ESP8266HTTPUpdateServer httpUpdater;

bool performUpdate() {
  WiFiClientSecure client;
  client.setInsecure();
  
  ESPhttpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  ESPhttpUpdate.rebootOnUpdate(false);
  
  t_httpUpdate_return result = ESPhttpUpdate.update(client, FIRMWARE_URL);
  
  if (result == HTTP_UPDATE_OK) {
    delay(1000);
    ESP.restart();
  }
  
  return false;
}

#endif // UPDATES_H