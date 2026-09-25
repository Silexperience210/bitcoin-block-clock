#pragma once
// ===== Shim desktop minimal pour compiler le sketch hors ESP32 (banc de rendu) =====
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <algorithm>
#include <string>
#include <functional>
using std::min; using std::max; using std::abs;
typedef uint8_t byte;
typedef bool boolean;
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define PROGMEM
#define PSTR(s) (s)
#define F(s) (s)
#define FPSTR(p) ((const char*)(p))
#define pgm_read_byte(a) (*(const uint8_t*)(a))
#define pgm_read_word(a) (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))
#define pgm_read_ptr(a) (*(void* const*)(a))
#define memcpy_P memcpy
#define strlen_P strlen
#define IRAM_ATTR
#define DRAM_ATTR
#define ESP_OK 0
#define ESP_INTR_FLAG_LEVEL1 (1<<1)
#include "WString.h"
#include "Print.h"
long map(long x, long in_min, long in_max, long out_min, long out_max);
unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);
void yield();
void *ps_malloc(size_t n);
void *heap_caps_malloc(size_t n, uint32_t caps);
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
#define MALLOC_CAP_INTERNAL 8
bool psramFound();
// ADC / LEDC / GPIO
#define ADC_11db 3
#define OUTPUT 1
#define INPUT 0
#define HIGH 1
#define LOW 0
uint32_t analogReadMilliVolts(uint8_t pin);
void analogReadResolution(uint8_t b);
void analogSetPinAttenuation(uint8_t pin, int att);
double ledcSetup(uint8_t ch, double freq, uint8_t res);
void ledcAttachPin(uint8_t pin, uint8_t ch);
void ledcWrite(uint8_t ch, uint32_t duty);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t v);
int digitalRead(uint8_t pin);
// Serial
class HardwareSerial : public Print {
public:
  void begin(unsigned long) {}
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *b, size_t n) override;
  using Print::write;
  operator bool() { return true; }
};
extern HardwareSerial Serial;
class EspClass { public: void restart(); uint32_t getFreeHeap(); uint32_t getFreePsram(); };
extern EspClass ESP;
// FreeRTOS minimal
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)(m))
#define portEXIT_CRITICAL(m) ((void)(m))
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define portTICK_PERIOD_MS 1
QueueHandle_t xQueueCreate(int len, int item);
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t t);
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t t);
BaseType_t xTaskCreatePinnedToCore(void (*fn)(void*), const char*, uint32_t, void*, int, TaskHandle_t*, int);
void vTaskDelay(TickType_t t);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t t);
// temps
bool getLocalTime(struct tm *info, uint32_t ms = 5000);
void configTzTime(const char *tz, const char *s1, const char *s2 = nullptr, const char *s3 = nullptr);
