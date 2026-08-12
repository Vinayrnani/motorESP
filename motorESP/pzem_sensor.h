#ifndef PZEM_SENSOR_H
#define PZEM_SENSOR_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"

// ============================================
// PZEM 004T 100A Interface
// ============================================
// SoftwareSerial on D5 (TX→PZEM RX) and D6 (RX→PZEM TX)
// Protocol: 9600 baud, Modbus-like read command
// 500ms timeout per read, 3 retries before fault
// Mock mode for development without hardware

struct PZEMData {
  float voltage;      // V
  float current;      // A (always positive)
  float power;        // W (always positive)
  float energy;       // Wh (cumulative)
  float frequency;    // Hz
  float pf;           // power factor (0-1)
  bool valid;
};

// Mock mode (defined in motorESP.ino — extern so only one TU owns them)
extern bool useMockPZEM;
extern float mockVoltage;
extern float mockCurrent;
extern float mockPower;
extern float mockEnergy;
extern float mockFrequency;
extern float mockPF;

// SoftwareSerial instance (static: internal to this header, used only here)
static SoftwareSerial pzemSerial(PZEM_RX_PIN, PZEM_TX_PIN);

// PZEM read command (broadcast address 0xF8, read registers 0x00-0x0A)
static const uint8_t pzem_cmd[] = {0xF8, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x64, 0x64};

static void initPZEM() {
  pzemSerial.begin(PZEM_BAUD);
}

// CRC16 for Modbus (PZEM uses standard Modbus CRC)
static uint16_t modbusCRC(uint8_t *data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

static bool pzemReadRegisters(uint8_t *buf, uint8_t len) {
  // Flush RX
  while (pzemSerial.available()) pzemSerial.read();

  // Send read command
  pzemSerial.write(pzem_cmd, 8);
  pzemSerial.flush();

  // Wait for response (25 bytes: addr + func + bytecount + 20 data + 2 crc)
  unsigned long start = millis();
  uint8_t resp[25];
  uint8_t idx = 0;

  while (idx < 25 && (millis() - start < PZEM_TIMEOUT_MS)) {
    if (pzemSerial.available()) {
      resp[idx++] = pzemSerial.read();
    }
  }

  if (idx < 25) return false;  // timeout

  // Verify CRC
  uint16_t recvCRC = resp[23] | (resp[24] << 8);
  uint16_t calcCRC = modbusCRC(resp, 23);
  if (recvCRC != calcCRC) return false;

  // Copy data
  if (buf && len > 0) {
    uint8_t copyLen = (len < 20) ? len : 20;
    memcpy(buf, resp + 3, copyLen);
  }

  return true;
}

static uint32_t be32(const uint8_t *d) {
  return ((uint32_t)d[0] << 24) | ((uint32_t)d[1] << 16) | ((uint32_t)d[2] << 8) | d[3];
}

static uint16_t be16(const uint8_t *d) {
  return ((uint16_t)d[0] << 8) | d[1];
}

static bool pzemRead(float &voltage, float &current, float &power, float &energy, float &freq, float &pf) {
  uint8_t data[20];
  if (!pzemReadRegisters(data, 20)) return false;

  // PZEM-004T V3/V4 register map (20 data bytes, big-endian):
  // Reg 0x0000: voltage        (uint16, 0.1V per LSB)        -> data[0..1]
  // Reg 0x0001-0x0002: current (uint32, 0.001A per LSB)      -> data[2..5]
  // Reg 0x0003-0x0004: power   (uint32, 0.1W per LSB)        -> data[6..9]
  // Reg 0x0005-0x0006: energy  (uint32, 1Wh per LSB)         -> data[10..13]
  // Reg 0x0007: frequency      (uint16, 0.1Hz per LSB)       -> data[14..15]
  // Reg 0x0008: power factor   (uint16, 0.01 per LSB)        -> data[16..17]

  voltage = be16(&data[0]) * 0.1f;
  current = be32(&data[2]) * 0.001f;
  power   = be32(&data[6]) * 0.1f;
  energy  = be32(&data[10]) * 1.0f;
  freq    = be16(&data[14]) * 0.1f;
  pf      = be16(&data[16]) * 0.01f;

  return true;
}

static PZEMData readPZEM() {
  PZEMData result = {0};

  if (useMockPZEM) {
    result.voltage = mockVoltage;
    result.current = mockCurrent;
    result.power = mockPower;
    result.energy = mockEnergy;
    result.frequency = mockFrequency;
    result.pf = mockPF;
    result.valid = true;
    return result;
  }

  float v, a, w, wh, hz, pf_val;
  for (uint8_t retry = 0; retry < PZEM_MAX_RETRIES; retry++) {
    if (pzemRead(v, a, w, wh, hz, pf_val)) {
      result.voltage = v;
      result.current = fabs(a);  // abs for CT direction
      result.power = fabs(w);
      result.energy = wh;
      result.frequency = hz;
      result.pf = fabs(pf_val);
      result.valid = true;
      return result;
    }
    delay(50);
  }

  result.valid = false;
  return result;
}

static void setMockPZEM(bool enable) {
  useMockPZEM = enable;
}

static void setMockValues(float v, float a, float w, float wh, float hz, float pf) {
  mockVoltage = v;
  mockCurrent = a;
  mockPower = w;
  mockEnergy = wh;
  mockFrequency = hz;
  mockPF = pf;
}

static void setMockRunning() {
  mockVoltage = 240.0;
  mockCurrent = 9.6;
  mockPower = 1100.0;
  mockEnergy += 3.0;  // simulate accumulation
  mockFrequency = 50.0;
  mockPF = 0.65;
}

static void setMockOff() {
  mockVoltage = 290.0;
  mockCurrent = 0.0;
  mockPower = 0.0;
  // energy stays (cumulative)
  mockFrequency = 50.0;
  mockPF = 0.0;
}

static void setMockDryRun() {
  mockVoltage = 240.0;
  mockCurrent = 2.5;
  mockPower = 300.0;
  mockEnergy += 0.8;
  mockFrequency = 50.0;
  mockPF = 0.45;
}

static void setMockOvercurrent() {
  mockVoltage = 235.0;
  mockCurrent = 15.0;
  mockPower = 1800.0;
  mockEnergy += 5.0;
  mockFrequency = 49.8;
  mockPF = 0.72;
}

#endif
