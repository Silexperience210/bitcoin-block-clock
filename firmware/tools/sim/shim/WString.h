#pragma once
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
class String {
public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &x) : s(x) {}
  String(const String &o) = default;
  String &operator=(const String &) = default;
  explicit String(char c) : s(1, c) {}
  explicit String(int v, unsigned char base = 10) { char b[40]; if (base == 16) snprintf(b, 40, "%x", v); else snprintf(b, 40, "%d", v); s = b; }
  explicit String(unsigned v, unsigned char base = 10) { char b[40]; snprintf(b, 40, base == 16 ? "%x" : "%u", v); s = b; }
  explicit String(long v, unsigned char base = 10) { char b[40]; snprintf(b, 40, "%ld", v); s = b; (void)base; }
  explicit String(unsigned long v, unsigned char base = 10) { char b[40]; snprintf(b, 40, "%lu", v); s = b; (void)base; }
  explicit String(long long v) { char b[40]; snprintf(b, 40, "%lld", v); s = b; }
  explicit String(float v, unsigned int d = 2) { char b[64]; snprintf(b, 64, "%.*f", d, (double)v); s = b; }
  explicit String(double v, unsigned int d = 2) { char b[64]; snprintf(b, 64, "%.*f", d, v); s = b; }
  const char *c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  bool isEmpty() const { return s.empty(); }
  char operator[](unsigned i) const { return i < s.size() ? s[i] : 0; }
  char &operator[](unsigned i) { return s[i]; }
  char charAt(unsigned i) const { return (*this)[i]; }
  bool reserve(unsigned n) { s.reserve(n); return true; }
  bool concat(const String &o) { s += o.s; return true; }
  bool concat(const char *c) { if (c) s += c; return true; }
  bool concat(const char *c, unsigned n) { if (c) s.append(c, n); return true; }
  bool concat(char c) { s += c; return true; }
  String &operator+=(const String &o) { s += o.s; return *this; }
  String &operator+=(const char *c) { if (c) s += c; return *this; }
  String &operator+=(char c) { s += c; return *this; }
  String &operator+=(int v) { s += String(v).s; return *this; }
  String &operator+=(long v) { s += String(v).s; return *this; }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator==(const char *c) const { return s == (c ? c : ""); }
  bool operator!=(const String &o) const { return s != o.s; }
  bool operator!=(const char *c) const { return !(*this == c); }
  bool operator<(const String &o) const { return s < o.s; }
  bool equals(const String &o) const { return s == o.s; }
  long toInt() const { return atol(s.c_str()); }
  float toFloat() const { return (float)atof(s.c_str()); }
  double toDouble() const { return atof(s.c_str()); }
  String substring(unsigned a) const { return a >= s.size() ? String() : String(s.substr(a)); }
  String substring(unsigned a, unsigned b) const { if (a > b) std::swap(a, b); if (a >= s.size()) return String(); return String(s.substr(a, b - a)); }
  int indexOf(char c, unsigned from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(const String &x, unsigned from = 0) const { auto p = s.find(x.s, from); return p == std::string::npos ? -1 : (int)p; }
  bool startsWith(const String &x) const { return s.rfind(x.s, 0) == 0; }
  bool endsWith(const String &x) const { return s.size() >= x.s.size() && s.compare(s.size() - x.s.size(), x.s.size(), x.s) == 0; }
  void replace(const String &a, const String &b) { if (a.s.empty()) return; size_t p = 0; while ((p = s.find(a.s, p)) != std::string::npos) { s.replace(p, a.s.size(), b.s); p += b.s.size(); } }
  void trim() { size_t a = s.find_first_not_of(" \t\r\n"); if (a == std::string::npos) { s.clear(); return; } size_t b = s.find_last_not_of(" \t\r\n"); s = s.substr(a, b - a + 1); }
  void toLowerCase() { for (auto &c : s) c = tolower(c); }
  void toUpperCase() { for (auto &c : s) c = toupper(c); }
  // pour ArduinoJson (adaptateur Arduino String)
  explicit operator bool() const { return true; }
};
inline String operator+(const String &a, const String &b) { return String(a.s + b.s); }
inline String operator+(const String &a, const char *b) { return String(a.s + (b ? b : "")); }
inline String operator+(const char *a, const String &b) { return String(std::string(a ? a : "") + b.s); }
inline String operator+(const String &a, char c) { return String(a.s + c); }
inline String operator+(const String &a, int v) { return a + String(v); }
inline String operator+(const String &a, long v) { return a + String(v); }
typedef String StringSumHelper;
class __FlashStringHelper;
