#pragma once
#include <Arduino.h>
class AudioFileSource { public: virtual ~AudioFileSource() {}
  virtual bool open(const char *) { return false; } virtual uint32_t read(void *, uint32_t) { return 0; }
  virtual bool seek(int32_t, int) { return false; } virtual bool close() { return false; }
  virtual bool isOpen() { return false; } virtual uint32_t getSize() { return 0; } virtual uint32_t getPos() { return 0; } };
