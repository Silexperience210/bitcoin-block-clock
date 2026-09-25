#pragma once
#include <Arduino.h>
class Preferences {
public:
  bool begin(const char *, bool = false) { return true; }
  void end() {}
  bool clear() { return true; }
  String getString(const char *k, const String &d = String()) { return (k && !strcmp(k, "ssid")) ? String("MockWiFi") : d; }
  float getFloat(const char *, float d = 0) { return d; }
  uint8_t getUChar(const char *, uint8_t d = 0) { return d; }
  int32_t getInt(const char *, int32_t d = 0) { return d; }
  int32_t getLong(const char *, int32_t d = 0) { return d; }
  bool getBool(const char *, bool d = false) { return d; }
  size_t getBytes(const char *, void *, size_t) { return 0; }
  size_t putString(const char *, const String &) { return 1; }
  size_t putFloat(const char *, float) { return 4; }
  size_t putUChar(const char *, uint8_t) { return 1; }
  size_t putInt(const char *, int32_t) { return 4; }
  size_t putLong(const char *, int32_t) { return 4; }
  size_t putBool(const char *, bool) { return 1; }
  size_t putBytes(const char *, const void *, size_t n) { return n; }
};
