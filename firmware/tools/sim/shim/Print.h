#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "WString.h"
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buf, size_t n) { size_t k = 0; while (n--) k += write(*buf++); return k; }
  size_t write(const char *s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
  size_t write(const char *b, size_t n) { return write((const uint8_t*)b, n); }
  size_t print(const char *s) { return write(s); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(unsigned char v, int b = DEC) { return print((unsigned long)v, b); }
  size_t print(int v, int b = DEC) { return print((long)v, b); }
  size_t print(unsigned int v, int b = DEC) { return print((unsigned long)v, b); }
  size_t print(long v, int b = DEC) { char t[40]; snprintf(t, 40, b == 16 ? "%lx" : "%ld", v); return write(t); }
  size_t print(unsigned long v, int b = DEC) { char t[40]; snprintf(t, 40, b == 16 ? "%lx" : "%lu", v); return write(t); }
  size_t print(double v, int d = 2) { char t[64]; snprintf(t, 64, "%.*f", d, v); return write(t); }
  size_t println() { return write("\r\n"); }
  template <typename T> size_t println(const T &v) { size_t n = print(v); return n + println(); }
  size_t println(const char *s) { size_t n = print(s); return n + println(); }
  size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[512]; va_list a; va_start(a, fmt); int n = vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
    if (n < 0) return 0; return write((const uint8_t*)buf, strlen(buf));
  }
  void flush() {}
};
class Printable { public: virtual size_t printTo(Print &p) const = 0; virtual ~Printable() {} };
