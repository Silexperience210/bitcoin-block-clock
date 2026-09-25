#pragma once
#include <Arduino.h>
class AudioOutput { public: virtual ~AudioOutput() {}
  virtual bool SetRate(int) { return true; } virtual bool SetBitsPerSample(int) { return true; }
  virtual bool SetChannels(int) { return true; } virtual bool SetGain(float) { return true; }
  virtual bool begin() { return true; } virtual bool ConsumeSample(int16_t[2]) { return true; }
  virtual uint16_t ConsumeSamples(int16_t *, uint16_t c) { return c; }
  virtual void flush() {} virtual bool stop() { return true; } virtual bool loop() { return true; } };
