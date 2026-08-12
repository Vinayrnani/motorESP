#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#include <Arduino.h>
#include <DHT.h>

#define DHTPIN D4
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

bool useMockSensor = false;
bool autoSimMode = false;
float mockTemp = 25.0;
float mockHum = 50.0;
float simTemp = 25.0;
float simHum = 40.0;
float lastValidTemp = 0.0;
float lastValidHum = 0.0;

const float SIM_AMBIENT_TEMP = 32.0;
const float SIM_AMBIENT_HUM = 35.0;
const float SIM_TARGET_TEMP = 39.0;
const float SIM_TARGET_HUM = 70.0;
const float SIM_FAN_COOL = 0.3;
const float SIM_SPEED_RISE = 0.04;
const float SIM_SPEED_DROP = 0.0075;
const float SIM_NOISE_TEMP = 0.1;
const float SIM_NOISE_HUM = 1.0;

void setAutoSim(bool enable);
void updateAutoSim(bool heaterOn, bool atomizerOn, bool fanOn);
float getSimTempWithNoise();
float getSimHumWithNoise();

extern float simTemp;
extern float simHum;

void initDHT() {
  dht.begin();
}

void setMockSensor(bool enable) {
  useMockSensor = enable;
}

void setMockValues(float temp, float hum) {
  mockTemp = temp;
  mockHum = hum;
}

float readDHT22() {
  if (autoSimMode) return getSimTempWithNoise();
  if (useMockSensor) return mockTemp;
  
  float t = dht.readTemperature();
  if (!isnan(t)) {
    lastValidTemp = t;
    return t;
  }
  return lastValidTemp > 0 ? lastValidTemp : -1;
}

float readHumidity() {
  if (autoSimMode) return getSimHumWithNoise();
  if (useMockSensor) return mockHum;
  
  float h = dht.readHumidity();
  if (!isnan(h)) {
    lastValidHum = h;
    return h;
  }
  return lastValidHum > 0 ? lastValidHum : -1;
}

void setAutoSim(bool enable) {
  autoSimMode = enable;
  if (enable) {
    useMockSensor = false;
    simTemp = SIM_AMBIENT_TEMP;
    simHum = SIM_AMBIENT_HUM;
    randomSeed(millis());
  }
}

void updateAutoSim(bool heaterOn, bool atomizerOn, bool fanOn) {
  if (!autoSimMode) return;
  
  float targetT = heaterOn ? SIM_TARGET_TEMP : SIM_AMBIENT_TEMP;
  float targetH = atomizerOn ? SIM_TARGET_HUM : SIM_AMBIENT_HUM;
  float fanCool = fanOn ? SIM_FAN_COOL : 0.0;
  float speedT = (heaterOn || simTemp < targetT) ? SIM_SPEED_RISE : SIM_SPEED_DROP;
  float speedH = (atomizerOn || simHum < targetH) ? SIM_SPEED_RISE : SIM_SPEED_DROP;
  
  simTemp += (targetT - simTemp - fanCool) * speedT;
  simHum += (targetH - simHum) * speedH;
}

float getSimTempWithNoise() {
  float noise = random(-10, 11) / 100.0;
  return simTemp + noise;
}

float getSimHumWithNoise() {
  float noise = random(-10, 11) / 10.0;
  return simHum + noise;
}

#endif
