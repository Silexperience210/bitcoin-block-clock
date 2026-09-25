#pragma once
#include <Arduino.h>
enum HTTPMethod { HTTP_ANY, HTTP_GET, HTTP_POST };
class WebServer {
public:
  WebServer(int) {}
  void on(const char *, HTTPMethod, std::function<void()>) {}
  void on(const char *, std::function<void()>) {}
  void onNotFound(std::function<void()>) {}
  void begin() {}
  void handleClient() {}
  void send(int, const char * = nullptr, const String & = String()) {}
  void send(int c, const char *t, const char *x) { send(c, t, String(x)); }
  void sendHeader(const String &, const String &, bool = false) {}
  String arg(const String &) { return String(); }
  bool hasArg(const String &) { return false; }
  String uri() { return String("/"); }
};
