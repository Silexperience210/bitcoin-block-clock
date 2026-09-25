// Implémentations des stubs + état simulé (temps, tactile)
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <sys/time.h>
#include <deque>
#include <vector>
HardwareSerial Serial; EspClass ESP; WiFiClass WiFi; MDNSResponder MDNS; TwoWire Wire;
// ---- horloge simulée (ms) : avancée par le harness ----
unsigned long g_ms = 1000;
time_t g_epoch = 1790000000;     // epoch simulé (modifiable)
bool g_quietSerial = true;
unsigned long millis() { return g_ms; }
unsigned long micros() { return g_ms * 1000UL; }
void delay(unsigned long ms) { g_ms += ms; }
void delayMicroseconds(unsigned int) {}
void yield() {}
extern "C" time_t time(time_t *t) { time_t v = g_epoch + (time_t)(g_ms / 1000); if (t) *t = v; return v; }
bool getLocalTime(struct tm *info, uint32_t) { time_t v = time(nullptr); localtime_r(&v, info); return true; }
void configTzTime(const char *tz, const char *, const char *, const char *) { setenv("TZ", tz, 1); tzset(); }
long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
static uint32_t rs = 12345;
long random(long h) { if (h <= 0) return 0; rs = rs * 1103515245u + 12345u; return (long)((rs >> 8) % (uint32_t)h); }
long random(long a, long b) { if (b <= a) return a; return a + random(b - a); }
void randomSeed(unsigned long s) { rs = s; }
void *ps_malloc(size_t n) { return malloc(n); }
void *heap_caps_malloc(size_t n, uint32_t) { return malloc(n); }
bool psramFound() { return true; }
uint32_t analogReadMilliVolts(uint8_t) { return 2350; }
void analogReadResolution(uint8_t) {}
void analogSetPinAttenuation(uint8_t, int) {}
double ledcSetup(uint8_t, double f, uint8_t) { return f; }
void ledcAttachPin(uint8_t, uint8_t) {}
void ledcWrite(uint8_t, uint32_t) {}
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t, uint8_t) {}
int digitalRead(uint8_t) { return 0; }
size_t HardwareSerial::write(uint8_t c) { if (!g_quietSerial) fputc(c, stderr); return 1; }
size_t HardwareSerial::write(const uint8_t *b, size_t n) { if (!g_quietSerial) fwrite(b, 1, n, stderr); return n; }
void EspClass::restart() { fprintf(stderr, "[stub] ESP.restart()\n"); exit(3); }
uint32_t EspClass::getFreeHeap() { return 200000; }
uint32_t EspClass::getFreePsram() { return 7000000; }
// ---- files FreeRTOS : vraies files FIFO (les tâches ne tournent pas) ----
struct Q { int item; std::deque<std::vector<uint8_t>> d; int cap; };
QueueHandle_t xQueueCreate(int len, int item) { Q *q = new Q; q->item = item; q->cap = len; return q; }
BaseType_t xQueueSend(QueueHandle_t h, const void *it, TickType_t) { Q *q = (Q*)h; if ((int)q->d.size() >= q->cap) return 0; q->d.emplace_back((const uint8_t*)it, (const uint8_t*)it + q->item); return 1; }
BaseType_t xQueueReceive(QueueHandle_t h, void *it, TickType_t) { Q *q = (Q*)h; if (q->d.empty()) return 0; memcpy(it, q->d.front().data(), q->item); q->d.pop_front(); return 1; }
BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char *, uint32_t, void *, int, TaskHandle_t *, int) { return 1; }
void vTaskDelay(TickType_t t) { g_ms += t; }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) { return 4096; }
// ---- tactile simulé : protocole AXS15231B (8 octets/doigt pour 1 doigt) ----
int g_nTouch = 0; int g_tx[2], g_ty[2];
static uint8_t wbuf[32]; static int wlen = 0, wpos = 0;
uint8_t TwoWire::requestFrom(uint8_t, uint8_t n) {
  memset(wbuf, 0, sizeof(wbuf)); wlen = n; wpos = 0;
  wbuf[0] = 0; wbuf[1] = (uint8_t)g_nTouch;
  for (int i = 0; i < g_nTouch && i < 2; i++) {
    int o = i * 6;
    // mapping inverse de readTouch : x = ry ; y = 319 - rx
    int ry = g_tx[i], rx = 319 - g_ty[i];
    wbuf[o + 2] = (rx >> 8) & 0x0F; wbuf[o + 3] = rx & 0xFF;
    wbuf[o + 4] = (ry >> 8) & 0x0F; wbuf[o + 5] = ry & 0xFF;
  }
  return n;
}
int TwoWire::read() { return wpos < wlen ? wbuf[wpos++] : -1; }
int TwoWire::available() { return wlen - wpos; }
