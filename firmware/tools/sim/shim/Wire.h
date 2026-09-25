#pragma once
#include <Arduino.h>
class TwoWire {
public:
  bool begin(int, int, uint32_t = 0) { return true; }
  void setClock(uint32_t) {}
  void beginTransmission(uint8_t) {}
  size_t write(const uint8_t *, size_t n) { return n; }
  size_t write(uint8_t) { return 1; }
  uint8_t endTransmission(bool = true) { return 0; }
  uint8_t requestFrom(uint8_t addr, uint8_t n);
  int read();
  int available();
};
extern TwoWire Wire;
