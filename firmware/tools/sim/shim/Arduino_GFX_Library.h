#pragma once
// Shim : vraie lib Arduino_GFX (algos de rendu + Canvas) + panel/bus simulés
#include "Arduino_DataBus.h"
#include "Arduino_GFX.h"
#include "canvas/Arduino_Canvas.h"
class Arduino_ESP32QSPI : public Arduino_DataBus {
public:
  Arduino_ESP32QSPI(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t, bool = false) {}
  bool begin(int32_t = 0, int8_t = 0) override { return true; }
  void beginWrite() override {} void endWrite() override {}
  void writeCommand(uint8_t) override {} void writeCommand16(uint16_t) override {}
  void writeCommandBytes(uint8_t *, uint32_t) override {}
  void write(uint8_t) override {} void write16(uint16_t) override {}
  void writeRepeat(uint16_t, uint32_t) override {}
  void writeBytes(uint8_t *, uint32_t) override {}
  void writePixels(uint16_t *, uint32_t) override {}
};
// callback de capture de frame (défini par le harness)
extern void harness_on_flush(const uint16_t *fb, int w, int h);
class Arduino_AXS15231B : public Arduino_G {
public:
  Arduino_AXS15231B(Arduino_DataBus *, int8_t, uint8_t, bool, int16_t w, int16_t h) : Arduino_G(w, h) {}
  bool begin(int32_t = 0) override { return true; }
  void drawBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t, uint16_t, uint16_t) override {}
  void drawIndexedBitmap(int16_t, int16_t, uint8_t *, uint16_t *, int16_t, int16_t, int16_t = 0) override {}
  void draw3bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
  void draw16bitRGBBitmap(int16_t, int16_t, uint16_t *bmp, int16_t w, int16_t h) override { harness_on_flush(bmp, w, h); }
  void draw24bitRGBBitmap(int16_t, int16_t, uint8_t *, int16_t, int16_t) override {}
};
