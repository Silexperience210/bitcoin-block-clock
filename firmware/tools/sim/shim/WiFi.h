#pragma once
#include <Arduino.h>
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WIFI_STA 1
#define WIFI_AP 2
#define WIFI_AP_STA 3
class IPAddress { public: String toString() const { return String("192.168.1.42"); } };
class WiFiClient : public Print {
public:
  virtual ~WiFiClient() {}
  virtual int connect(const char *, uint16_t, int32_t = 0) { return 0; }
  virtual int connect(const char *h, uint16_t p, int32_t t, int32_t) { return connect(h, p, t); }
  size_t write(uint8_t) override { return 1; }
  using Print::write;
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  virtual int read(uint8_t *, size_t) { return -1; }
  virtual uint8_t connected() { return 0; }
  virtual void stop() {}
  void setTimeout(unsigned long) {}
  String readStringUntil(char) { return String(); }
  operator bool() { return false; }
};
class WiFiClass {
public:
  int status() { return WL_CONNECTED; }
  bool mode(int) { return true; }
  bool setSleep(bool) { return true; }
  int begin(const char *, const char * = nullptr) { return WL_CONNECTED; }
  IPAddress localIP() { return IPAddress(); }
  long RSSI() { return -58; }
  bool softAP(const char *, const char * = nullptr) { return true; }
  int scanNetworks() { return 0; }
  String SSID(int = 0) { return String("mock"); }
  bool disconnect(bool = false) { return true; }
  bool reconnect() { return true; }
};
extern WiFiClass WiFi;
