#pragma once
#include "WiFi.h"
class HTTPClient {
public:
  WiFiClient dummy;
  bool begin(WiFiClient &, const String &) { return true; }
  bool begin(WiFiClient &c, const char *u) { return begin(c, String(u)); }
  void setTimeout(uint16_t) {}
  void setConnectTimeout(int32_t) {}
  void useHTTP10(bool = true) {}
  void setReuse(bool) {}
  int GET() { return -1; }
  String getString() { return String(); }
  WiFiClient *getStreamPtr() { return &dummy; }
  WiFiClient &getStream() { return dummy; }
  int getSize() { return -1; }
  void end() {}
};
