// =====================================================================
//  BITCOIN BLOCK CLOCK V4 — Guition JC3248W535 (ESP32-S3 N16R8)
//  ------------------------------------------------------------------
//  Paysage 480x320 · 9 pages (swipe + barre d'onglets) :
//   PRIX / ON-CHAIN / CUBE / POOLS WAR / LIGHTNING / NŒUD / IA / SIGNAUX / BTC DOOM
//  • netTask FreeRTOS (core 0) : TOUS les fetchs HTTP hors du loop()
//    → tactile toujours réactif, requêtes à la demande via fetchReq
//  • sndTask FreeRTOS + queue de notes : sons non bloquants
//  • DONG à chaque nouveau bloc · alertes prix (seuils via web)
//  • CUBE : particle art mempool + chaîne qui emporte le bloc miné
//  • POOLS WAR : course animée des pools 1 semaine (mempool.space)
//  • LIGHTNING : capacité réseau, channels, nodes, fees
//  • BTC DOOM : raycaster façon Wolfenstein 3D — démons qui chassent,
//    bouton FIRE hitscan, score, muzzle flash, minimap
//  • IA LOCALE : prochain bloc (Poisson, modèle exact) + P(1/5/10 min) ·
//    tendance fees (régression) + cycles hebdo APPRIS en continu (NVS, 168
//    créneaux heure×jour, sauvegarde 1×/30 min)
//  • SIGNAUX : divergence tendance 1D vs 1S (force faible/moyenne/forte) ·
//    squeeze Bollinger 30J · score technique · anomalies z-score + vol 2σ
//    [MLP tendance J+1 testé et rejeté : ne bat pas le hasard]
//  • Chiffres lissés anti-aliasés (smooth_font.h, alpha 4 bpp) : prix animé
//    en transition douce, bloc, F&G, capacité LN
//  • Config WiFi portail web (+ IP du nœud) · mode nuit · tap/appui long
//
//  FQBN : esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,
//         PartitionScheme=huge_app,CPUFreq=240,USBMode=hwcdc,CDCOnBoot=cdc
// =====================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <time.h>
#include <math.h>
#include <driver/i2s.h>
#include <Arduino_GFX_Library.h>
#include "btc_logo.h"
#include "smooth_font.h"

// ---------- synthèse vocale SAM (Software Automatic Mouth, s-macke/SAM) ----------
extern "C" {
  #include "src/sam/sam.h"
  #include "src/sam/reciter.h"
}
int debug = 0;   // requis par la lib SAM (sorties debug désactivées)

// ---------- TTS Google Translate (voix naturelle en ligne) + MP3 ESP8266Audio ----------
#include "AudioGeneratorMP3.h"
#include "AudioFileSource.h"
#include "AudioOutput.h"

// source audio depuis un buffer RAM/PSRAM (le MP3 du TTS)
class AudioFileSourceMem : public AudioFileSource {
public:
  AudioFileSourceMem(const uint8_t *data, uint32_t len) : p(data), n(len) {}
  bool open(const char*) override { pos = 0; return true; }
  uint32_t read(void *data, uint32_t len) override {
    if ((uint32_t)pos >= n) return 0;
    if (len > n - pos) len = n - pos;
    memcpy(data, p + pos, len); pos += len; return len;
  }
  bool seek(int32_t p2, int dir) override {
    if (dir == SEEK_SET) pos = p2;
    else if (dir == SEEK_CUR) pos += p2;
    else pos = n - p2;
    if (pos < 0) pos = 0;
    if ((uint32_t)pos > n) pos = n;
    return true;
  }
  bool close() override { return true; }
  bool isOpen() override { return true; }
  uint32_t getSize() override { return n; }
  uint32_t getPos() override { return pos; }
private:
  const uint8_t *p; uint32_t n; int32_t pos = 0;
};

#define TTS_GOOGLE 1     // 1 = voix naturelle Google (réseau requis) ; 0 = SAM local
#define TTS_LANG   "en"  // "fr" pour la voix française

// ---------------- PINS ----------------
#define PIN_BL      1
#define TP_SDA      4
#define TP_SCL      8
#define TP_ADDR     0x3B
#define I2S_BCLK    42
#define I2S_LRCLK   2
#define I2S_DOUT    41
#define PIN_BAT_ADC 5

// Résolution physique panel (portrait) et logique (paysage via canvas)
#define PANEL_W 320
#define PANEL_H 480
#define SCR_W 480
#define SCR_H 320

// ---------------- PALETTE ----------------
#define C_BG      0x0841
#define C_PANEL   0x10A2
#define C_LINE    0x2965
#define C_ORANGE  0xFBE0
#define C_ORANGE_D 0x8A60
#define C_WHITE   0xFFFF
#define C_GREY    0x8C51
#define C_DGREY   0x4A49
#define C_GREEN   0x2E68
#define C_RED     0xD186
#define C_GREEN_D 0x03E0
#define C_RED_D   0x8800
#define C_YELLOW  0xFE60
#define C_BLUE    0x2A7F
#define TRANSP    0xF81F

// ---------------- PAGES ----------------
enum { PG_PRICE = 0, PG_CHAIN, PG_CUBE, PG_POOLS, PG_LN, PG_NODE, PG_AI, PG_SIG, PG_DOOM, PG_COUNT };

// ---------------- TIMEFRAMES ----------------
enum { TF_1H = 0, TF_24H, TF_7J, TF_30J, TF_COUNT };
const char* TF_LABEL[TF_COUNT] = {"1H", "24H", "7J", "30J"};
const char* TF_DAYS[TF_COUNT]  = {"1", "1", "7", "30"};
const int   TF_TARGET[TF_COUNT] = {12, 60, 84, 90};
uint8_t curTf = TF_24H;

#define MAX_PTS 92
float   closes[MAX_PTS];
long    tsOf[MAX_PTS];
int     nPts = 0;
float   gChartMin = 0, gChartMax = 0;   // min/max du graphe courant (réservé)
int     cursorIdx = -1;          // curseur tactile sur le graphe (-1 = off)

// ---------------- DEVISES ----------------
enum { CUR_EUR = 0, CUR_USD, CUR_CHF, CUR_COUNT };
const char* CUR_LABEL[CUR_COUNT] = {"EUR", "USD", "CHF"};
const char* CUR_API[CUR_COUNT]   = {"eur", "usd", "chf"};
uint8_t curCur = CUR_EUR;

// ---------------- CACHE GRAPHE (devise × timeframe) ----------------
// réaffichage instantané au switch, rafraîchi en arrière-plan (~9 Ko)
float   closesC[CUR_COUNT][TF_COUNT][MAX_PTS];
long    tsC[CUR_COUNT][TF_COUNT][MAX_PTS];
int16_t nPtsC[CUR_COUNT][TF_COUNT] = {};
float   mnC[CUR_COUNT][TF_COUNT], mxC[CUR_COUNT][TF_COUNT];

// ---------------- OBJETS ----------------
Arduino_DataBus *bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
Arduino_AXS15231B *panel = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, PANEL_W, PANEL_H);
Arduino_Canvas *gfx = new Arduino_Canvas(PANEL_W, PANEL_H, panel, 0, 0, 1);
Preferences prefs;
WebServer server(80);

// ---------------- CONFIG / DONNEES (écrites par netTask, lues par UI) ----------------
String cfg_ssid, cfg_pass, cfg_nodeip = "192.168.1.110";
float  alertHi = 0, alertLo = 0;      // 0 = désactivé
bool   latchHi = false, latchLo = false;

float  btcPrice[CUR_COUNT] = {0, 0, 0};
float  dispPrice = 0;       // prix affiché (animé vers btcPrice[curCur])
float  btcChg24 = 0;
long   blockHeight = 0;
long   mempoolCount = 0;
int    feeFast = 0, feeHalf = 0, feeHour = 0, feeEco = 0;
// ---- moteur prédictif on-device (page IA) ----
long   blkTs[6] = {0};        // timestamps des derniers blocs (Poisson)
int    blkTsN = 0;
int    feeHist[32];           // ring buffer feeFast (échantillon ~2 min)
int    feeHistN = 0, feeHistIdx = 0;
// ---- cycles de fees appris en continu (168 créneaux heure×jour, NVS) ----
float  feeBkt[168] = {0};     // moyenne EMA par créneau (0 = pas encore appris)
long   feeSamples = 0;        // nb total d'échantillons appris
bool   feeBktDirty = false;
// ---- détection d'anomalies (stats EMA, RAM) ----
float  anomMemAvg = 0, anomMemVar = 0, anomMemZ = 0;
float  anomFeeAvg = 0, anomFeeVar = 0, anomFeeZ = 0;
bool   anomActive = false;    // latch événementiel (reset quand z < 1.5)
volatile bool evAnomaly = false;
char   lastPool[32] = "-";
long   lastBlockTx = 0;
float  whaleBtc = 0;
char   whaleTxid[20] = "";
int    fngValue = -1;
char   fngLabel[20] = "-";
long   diffRemaining = 0;
float  diffChange = 0;
volatile bool nodeOnline = false;
volatile bool dataOk = false;

// Pools war (protégés par dataMux)
long poolsTotal = 0;
int  poolsN = 0;
char poolNames[6][20];
long poolBlocks[6] = {0, 0, 0, 0, 0, 0};
unsigned long poolsDataAt = 0;

// Lightning (scalaires atomiques)
float lnCapBtc = 0;
long  lnChannels = 0, lnNodes = 0, lnAvgCap = 0;
int   lnAvgFeePpm = 0;

// ---------------- SYNCHRO netTask <-> UI ----------------
// spinlock pour les chaînes / tableaux partagés
portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE reqMux  = portMUX_INITIALIZER_UNLOCKED;

// requêtes de fetch à la demande (bitmask)
#define REQ_PRICE    0x001
#define REQ_HEIGHT   0x002
#define REQ_KLINES   0x004
#define REQ_MEMPOOL  0x008
#define REQ_POOLS    0x010
#define REQ_LN       0x020
#define REQ_WHALE    0x040
#define REQ_FNG      0x080
#define REQ_DIFF     0x100
#define REQ_NODE     0x200
#define REQ_KL30     0x400   // cache 30J (moteur prédictif page IA)
#define REQ_ALL      0x7FF
volatile uint32_t fetchReq = 0;

void requestFetch(uint32_t bits) {
  portENTER_CRITICAL(&reqMux);
  fetchReq |= bits;
  portEXIT_CRITICAL(&reqMux);
}

// événements net -> UI (consommés par le loop)
volatile bool evNewBlock = false;
volatile bool evWhale   = false;

// ---------------- ETAT UI ----------------
unsigned long blockDetectedMs = 0, lastDrawMs = 0;
uint8_t page = 0;
volatile bool nightMode = false, sleeping = false;
bool animNewBlock = false;
unsigned long animStart = 0;
volatile bool needRedraw = true;

// état tactile courant (pour BTC DOOM : suivi continu du doigt)
bool     gTouch = false;
uint16_t gTX = 0, gTY = 0;

// =====================================================================
//  AUDIO — cloche synthétisée I2S + sndTask non bloquante
//  ------------------------------------------------------------------
//  Les helpers (beep, playStart, playAlarm, playWhale, playBellQ)
//  poussent une note dans une FreeRTOS queue et retournent aussitôt.
//  sndTask (core 0, prio 2) dépile et synthétise : le loop() ne bloque
//  jamais sur i2s_write. Les notes SND_EVENT sont filtrées en mode
//  nuit / veille ; les bips UI (SND_UI) passent toujours.
// =====================================================================
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 22050

enum { SND_UI = 0, SND_EVENT = 1 };
struct SndNote { uint16_t freq; uint16_t durMs; uint8_t vol; uint8_t kind; };
QueueHandle_t sndQ = NULL;

// ---------- gestion du volume (icône HP header : tap = cycle) ----------
// 100 -> 60 -> 30 -> 0 (muet) -> 100 ... persisté en NVS ("sndvol").
// S'applique aux cloches ET à la voix SAM.
uint8_t sndVolPct = 100;

void audioInit() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_BCLK;
  pins.ws_io_num = I2S_LRCLK;
  pins.data_out_num = I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

// synthèse d'une note (tourne dans sndTask — blocage acceptable ici)
void synthBell(float freq, float durSec, float vol) {
  const int chunk = 512;
  int16_t buf[chunk * 2];
  long total = (long)(SAMPLE_RATE * durSec), pos = 0;
  while (pos < total) {
    for (int i = 0; i < chunk && pos < total; i++, pos++) {
      float t = (float)pos / SAMPLE_RATE;
      float env = expf(-3.2f * t);
      float s = sinf(2 * PI * freq * t) + 0.55f * sinf(2 * PI * freq * 2.76f * t);
      int16_t v = (int16_t)(s * env * vol * 30000);
      buf[i * 2] = v; buf[i * 2 + 1] = v;
    }
    size_t w;
    i2s_write(I2S_PORT, buf, chunk * 2 * sizeof(int16_t), &w, portMAX_DELAY);
  }
}

// ---------- parole SAM (voix anglaise, réglage "doux") ----------
#define SAM_SPEED  60    // 72 = défaut (plus petit = plus lent)
#define SAM_PITCH  55    // 64 = défaut (plus petit = plus grave/doux)
#define SAM_MOUTH  128
#define SAM_THROAT 128
#define SAM_GAIN   1     // gain appliqué aux samples 8 bits de SAM
#define SPEECH_BLOCKS 1  // 1 = annonce vocale des blocs ; 0 = cloche seule

struct SndTxt { char txt[96]; uint8_t kind; };
QueueHandle_t sndTxtQ = NULL;

// rend + joue une phrase anglaise via SAM (bloquant — tourne dans sndTask)
void speakSam(const char *textEn) {
  char tmp[256];
  strlcpy(tmp, textEn, sizeof(tmp));
  int ok1 = TextToPhonemes((unsigned char*)tmp);
  if (!ok1) return;
  SetInput(tmp);
  SetSpeed(SAM_SPEED); SetPitch(SAM_PITCH);
  SetMouth(SAM_MOUTH); SetThroat(SAM_THROAT);
  int ok2 = SAMMain();
  if (!ok2) return;
  int n = GetBufferLength() / 50;            // SAM compte en 1/50e d'échantillon
  char *buf = GetBuffer();
  Serial.printf("[SAM] \"%s\" -> %d samples\n", textEn, n);
  const int chunk = 512;
  int16_t out[chunk * 2];
  for (int pos = 0; pos < n; ) {
    int k = 0;
    for (; k < chunk && pos < n; k++, pos++) {
      // samples SAM = 8 bits NON SIGNÉS (0..240, silence ~128) -> 16 bits signé
      int32_t s = ((int32_t)(uint8_t)buf[pos] - 128) * 256 * SAM_GAIN * sndVolPct / 100;
      if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
      out[k * 2] = (int16_t)s; out[k * 2 + 1] = (int16_t)s;
    }
    size_t w; i2s_write(I2S_PORT, out, k * 2 * sizeof(int16_t), &w, portMAX_DELAY);
  }
}

// ---------- sortie audio vers notre driver I2S existant ----------
// Reçoit le PCM du décodeur MP3, ajuste l'horloge I2S au taux du MP3,
// applique le volume, restaure 22050 Hz à la fin (pour la synthèse cloche).
class AudioOutputI2SClock : public AudioOutput {
public:
  bool SetRate(int hz) override {
    return i2s_set_clk(I2S_PORT, hz, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO) == ESP_OK;
  }
  bool SetBitsPerSample(int bps) override { return bps == 16; }
  bool SetChannels(int ch) override { channels = ch; return ch >= 1 && ch <= 2; }
  bool begin() override { pos = 0; return true; }
  bool ConsumeSample(int16_t sample[2]) override {
    int32_t l = sample[0] * sndVolPct / 100;
    int32_t r = ((channels == 2) ? sample[1] : sample[0]) * sndVolPct / 100;
    if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
    if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
    buf[pos * 2] = (int16_t)l; buf[pos * 2 + 1] = (int16_t)r;
    if (++pos >= CHUNK) flush();
    return true;
  }
  void flush() override {
    if (pos > 0) { size_t w; i2s_write(I2S_PORT, buf, pos * 2 * sizeof(int16_t), &w, portMAX_DELAY); pos = 0; }
  }
  bool stop() override {
    flush();
    i2s_set_clk(I2S_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
    return true;
  }
private:
  static const int CHUNK = 512;
  int16_t buf[CHUNK * 2];
  int pos = 0, channels = 1;
};

// encode minimal pour une URL
String urlEncode(const String &s) {
  String out;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += c;
    else if (c == ' ') out += "%20";
    else { char h[4]; snprintf(h, sizeof(h), "%%%02X", (uint8_t)c); out += h; }
  }
  return out;
}

// TTS Google Translate : fetch MP3 en PSRAM puis décodage (bloquant, sndTask)
bool speakGoogle(const char *text, const char *lang) {
  String url = "https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=" +
               String(lang) + "&q=" + urlEncode(text);
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) { Serial.printf("[TTS] HTTP %d\n", code); http.end(); return false; }
  // Google répond en chunked (pas de Content-Length) : on lit jusqu'à EOF
  const int TTS_MAX = 65536;
  uint8_t *mp3 = (uint8_t*)ps_malloc(TTS_MAX);
  if (!mp3) { http.end(); return false; }
  WiFiClient *st = http.getStreamPtr();
  int got = 0;
  unsigned long t0 = millis();
  while (got < TTS_MAX && millis() - t0 < 10000) {
    int r = st->read(mp3 + got, TTS_MAX - got);
    if (r < 0) break;                    // fin du flux
    if (r == 0 && !st->connected()) break;
    got += r;
  }
  http.end();
  if (got < 1000) { Serial.printf("[TTS] trop court %d\n", got); free(mp3); return false; }
  Serial.printf("[TTS] \"%s\" -> mp3 %d bytes\n", text, got);
  AudioFileSourceMem *src = new AudioFileSourceMem(mp3, got);
  AudioGeneratorMP3 *dec = new AudioGeneratorMP3();
  AudioOutputI2SClock *out = new AudioOutputI2SClock();
  dec->begin(src, out);
  while (dec->loop()) {}
  dec->stop();
  delete dec; delete src; delete out;
  free(mp3);
  return true;
}

// dispatcher vocal : Google en ligne, sinon SAM local en repli
void speakAuto(const char *txt) {
#if TTS_GOOGLE
  if (speakGoogle(txt, TTS_LANG)) return;
#endif
  speakSam(txt);
}

// push non bloquant d'une phrase (queue pleine = phrase perdue, tant pis)
void speak(const String &txt, uint8_t kind) {
  if (!sndTxtQ || sndVolPct == 0) return;              // muet : voix coupée
  SndTxt t; t.kind = kind;
  strlcpy(t.txt, txt.c_str(), sizeof(t.txt));
  xQueueSend(sndTxtQ, &t, 0);
}

void sndTask(void *param) {
  SndNote n; SndTxt t;
  for (;;) {
    // parole en priorité
    if (xQueueReceive(sndTxtQ, &t, 0) == pdTRUE) {
      if (!(t.kind == SND_EVENT && (nightMode || sleeping))) speakAuto(t.txt);
      continue;
    }
    if (xQueueReceive(sndQ, &n, pdMS_TO_TICKS(120)) == pdTRUE) {
      if (n.kind == SND_EVENT && (nightMode || sleeping)) continue;  // silencieux la nuit / en veille
      synthBell((float)n.freq, n.durMs / 1000.0f, n.vol / 100.0f);
    }
  }
}

// push non bloquant (queue pleine = note perdue, tant pis)
void playNote(uint16_t freq, uint16_t durMs, uint8_t vol, uint8_t kind) {
  if (!sndQ || sndVolPct == 0) return;                 // muet : tout coupé
  SndNote n = {freq, durMs, (uint8_t)(vol * sndVolPct / 100), kind};
  xQueueSend(sndQ, &n, 0);
}

// ---- helpers publics ----
void beep(uint16_t f = 1200, uint16_t d = 70, uint8_t v = 40) { playNote(f, d, v, SND_UI); }
void playBellQ()  { playNote(880, 1200, 55, SND_EVENT); }                       // dong nouveau bloc
void playStart()  { playNote(659, 180, 40, SND_UI); playNote(880, 180, 40, SND_UI); playNote(1318, 450, 45, SND_UI); }
void playAlarm()  { playNote(1318, 300, 50, SND_EVENT); playNote(1568, 300, 50, SND_EVENT); playNote(1318, 600, 50, SND_EVENT); }
void playWhale()  { playNote(220, 500, 50, SND_EVENT); playNote(196, 700, 50, SND_EVENT); }

// =====================================================================
//  TACTILE — AXS15231B (I2C 0x3B) + mapping paysage (rotation 1)
// =====================================================================
bool readTouch(uint16_t &x, uint16_t &y) {
  static const uint8_t cmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0, 0, 0, 0x08};
  Wire.beginTransmission(TP_ADDR);
  Wire.write(cmd, 8);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TP_ADDR, (uint8_t)8) != 8) return false;
  uint8_t d[8];
  for (int i = 0; i < 8; i++) d[i] = Wire.read();
  if (d[0] != 0 || d[1] == 0) return false;
  uint16_t rx = ((d[2] & 0x0F) << 8) | d[3];
  uint16_t ry = ((d[4] & 0x0F) << 8) | d[5];
  // mapping portrait natif -> paysage rotation 1 (swap + mirror X)
  x = ry;
  y = (PANEL_W - 1) - rx;
  if (x >= SCR_W) x = SCR_W - 1;
  if (y >= SCR_H) y = SCR_H - 1;
  return true;
}

// lecture multi-touch AXS15231B : 6 octets par doigt (jusqu'à 5).
// ev : 1 = relevé, sinon pression. Retourne le nombre de doigts.
int readTouchMulti(uint16_t *xs, uint16_t *ys, uint8_t *ev, int maxPts) {
  static const uint8_t cmd[8] = {0xB5, 0xAB, 0xA5, 0x5A, 0, 0, 0, 0x08};
  Wire.beginTransmission(TP_ADDR);
  Wire.write(cmd, 8);
  if (Wire.endTransmission(false) != 0) return 0;
  int want = maxPts * 6;
  if (Wire.requestFrom(TP_ADDR, (uint8_t)want) != want) return 0;
  uint8_t d[30];
  for (int i = 0; i < want; i++) d[i] = Wire.read();
  if (d[1] == 0) return 0;
  int n = min((int)d[1], maxPts), cnt = 0;
  for (int i = 0; i < n; i++) {
    int o = i * 6;
    uint16_t rx = ((d[o + 2] & 0x0F) << 8) | d[o + 3];
    uint16_t ry = ((d[o + 4] & 0x0F) << 8) | d[o + 5];
    uint16_t x = ry, y = (PANEL_W - 1) - rx;
    if (x >= SCR_W) x = SCR_W - 1;
    if (y >= SCR_H) y = SCR_H - 1;
    xs[cnt] = x; ys[cnt] = y; ev[cnt] = (d[o + 2] >> 6) & 0x03;
    cnt++;
  }
  return cnt;
}

// =====================================================================
//  CONFIG NVS + PORTAIL WEB
// =====================================================================
void loadConfig() {
  prefs.begin("bc", true);
  cfg_ssid   = prefs.getString("ssid", "");
  cfg_pass   = prefs.getString("pass", "");
  cfg_nodeip = prefs.getString("nodeip", "192.168.1.110");
  alertHi    = prefs.getFloat("alertHi", 0);
  alertLo    = prefs.getFloat("alertLo", 0);
  sndVolPct  = prefs.getUChar("sndvol", 100);
  feeSamples = prefs.getLong("feesmp", 0);
  if (prefs.getBytes("feebkt", feeBkt, sizeof(feeBkt)) != sizeof(feeBkt))
    memset(feeBkt, 0, sizeof(feeBkt));
  prefs.end();
}

// persistance des cycles de fees (appelée max 1×/30 min — usure flash)
void saveFeeBuckets() {
  prefs.begin("bc", false);
  prefs.putBytes("feebkt", feeBkt, sizeof(feeBkt));
  prefs.putLong("feesmp", feeSamples);
  prefs.end();
  feeBktDirty = false;
}
void saveConfig() {
  prefs.begin("bc", false);
  prefs.putString("ssid", cfg_ssid);
  prefs.putString("pass", cfg_pass);
  prefs.putString("nodeip", cfg_nodeip);
  prefs.putFloat("alertHi", alertHi);
  prefs.putFloat("alertLo", alertLo);
  prefs.putUChar("sndvol", sndVolPct);
  prefs.end();
}

const char SETUP_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>BlockClock Setup</title><style>
body{font-family:sans-serif;background:#0d1117;color:#eee;max-width:400px;margin:40px auto;padding:0 16px}
h1{color:#F7931A}input,select{width:100%;padding:12px;margin:6px 0;border-radius:8px;border:1px solid #444;background:#161b22;color:#eee;box-sizing:border-box}
button{width:100%;padding:14px;background:#F7931A;color:#000;font-weight:bold;border:0;border-radius:8px;font-size:16px;margin-top:10px}
</style></head><body><h1>&#8383; Block Clock</h1>
<form method='POST' action='/save'><label>Reseau WiFi</label>%NETWORKS%
<input name='ssid' placeholder='ou SSID manuel' required>
<input name='pass' type='password' placeholder='Mot de passe WiFi'>
<label>Noeud Umbrel (optionnel)</label>
<input name='nodeip' value='%NODEIP%' placeholder='192.168.1.110'>
<button>Enregistrer &amp; redemarrer</button></form></body></html>)HTML";

String scanNetworks() {
  int n = WiFi.scanNetworks();
  String s = "<select onchange=\"document.getElementsByName('ssid')[0].value=this.value\"><option value=''>— reseaux —</option>";
  for (int i = 0; i < n && i < 15; i++) s += "<option>" + WiFi.SSID(i) + "</option>";
  return s + "</select>";
}

void startConfigPortal() {
  String apName = "BlockClock-Setup";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str(), "12345678");
  gfx->fillScreen(C_BG);
  gfx->setTextColor(C_ORANGE); gfx->setTextSize(3);
  gfx->setCursor(100, 80); gfx->print("CONFIG WIFI");
  gfx->setTextColor(C_WHITE); gfx->setTextSize(2);
  gfx->setCursor(60, 140); gfx->println("1. WiFi : " + apName);
  gfx->setCursor(60, 170); gfx->println("   mdp : 12345678");
  gfx->setCursor(60, 210); gfx->println("2. http://192.168.4.1");
  gfx->flush();
  server.on("/", HTTP_GET, []() {
    String p = FPSTR(SETUP_PAGE);
    p.replace("%NETWORKS%", scanNetworks());
    p.replace("%NODEIP%", cfg_nodeip);
    server.send(200, "text/html", p);
  });
  server.on("/save", HTTP_POST, []() {
    cfg_ssid = server.arg("ssid"); cfg_ssid.trim();
    cfg_pass = server.arg("pass");
    if (server.hasArg("nodeip")) { cfg_nodeip = server.arg("nodeip"); cfg_nodeip.trim(); }
    saveConfig();
    server.send(200, "text/html", "<meta charset='utf-8'><body style='background:#0d1117;color:#eee;font-family:sans-serif;text-align:center;padding-top:80px'><h2>Sauvegarde ! Redemarrage…</h2></body>");
    delay(1200); ESP.restart();
  });
  server.begin();
  while (true) { server.handleClient(); delay(2); }
}

// =====================================================================
//  HTTP / API — tournent toutes dans netTask (core 0), JAMAIS dans loop()
// =====================================================================
bool httpGet(const char *url, String &out, uint16_t timeoutMs = 5000) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(timeoutMs);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code == 200) { out = http.getString(); http.end(); return true; }
  Serial.printf("[HTTP] %s -> %d\n", url, code);
  http.end();
  return false;
}

// ---- Prix 3 devises + variation (CoinGecko) ----
void fetchPrice() {
  String r;
  if (!httpGet("https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=eur,usd,chf&include_24hr_change=true", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  btcPrice[CUR_EUR] = doc["bitcoin"]["eur"] | 0.0f;
  btcPrice[CUR_USD] = doc["bitcoin"]["usd"] | 0.0f;
  btcPrice[CUR_CHF] = doc["bitcoin"]["chf"] | 0.0f;
  btcChg24 = doc["bitcoin"][String(CUR_API[curCur]) + "_24h_change"] | 0.0f;
  dataOk = true;

  // ---- alertes prix (latch + hystérésis 2%) ----
  float p = btcPrice[curCur];
  if (p > 0) {
    if (alertHi > 0 && p >= alertHi && !latchHi) {
      latchHi = true;
      playAlarm();   // queue non bloquante, filtrée la nuit par sndTask
      Serial.println("[ALERTE] prix >= seuil haut !");
    }
    if (alertHi > 0 && p < alertHi * 0.98) latchHi = false;
    if (alertLo > 0 && p <= alertLo && !latchLo) {
      latchLo = true;
      playAlarm();
      Serial.println("[ALERTE] prix <= seuil bas !");
    }
    if (alertLo > 0 && p > alertLo * 1.02) latchLo = false;
  }
}

// ---- Historique prix (CoinGecko market_chart) ----
// Vue (devise, timeframe) en paramètres : le résultat alimente le cache
// du slot, et l'affichage live seulement si c'est encore la vue affichée.
void fetchKlinesView(uint8_t fc, uint8_t ft) {
  char url[200];
  snprintf(url, sizeof(url), "https://api.coingecko.com/api/v3/coins/bitcoin/market_chart?vs_currency=%s&days=%s",
           CUR_API[fc], TF_DAYS[ft]);
  String r;
  if (!httpGet(url, r, 8000)) return;
  JsonDocument filter;
  filter["prices"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, r, DeserializationOption::Filter(filter))) return;
  JsonArray prices = doc["prices"];
  int total = prices.size();
  if (total < 2) return;
  int step = max(1, total / TF_TARGET[ft]);
  // remplissage en local puis copie protégée (l'UI lit pendant ce temps)
  float tc[MAX_PTS]; long tt[MAX_PTS]; int tn = 0;
  for (int i = 0; i < total && tn < MAX_PTS; i += step) {
    tc[tn] = prices[i][1].as<float>();
    tt[tn] = (long)(prices[i][0].as<long long>() / 1000);
    tn++;
  }
  tc[tn - 1] = prices[total - 1][1].as<float>();
  tt[tn - 1] = (long)(prices[total - 1][0].as<long long>() / 1000);
  float mn = tc[0], mx = tc[0];
  for (int i = 1; i < tn; i++) { if (tc[i] < mn) mn = tc[i]; if (tc[i] > mx) mx = tc[i]; }
  portENTER_CRITICAL(&dataMux);
  memcpy(closesC[fc][ft], tc, tn * sizeof(float));
  memcpy(tsC[fc][ft], tt, tn * sizeof(long));
  nPtsC[fc][ft] = tn; mnC[fc][ft] = mn; mxC[fc][ft] = mx;
  if (fc == curCur && ft == curTf) {          // toujours la vue affichée ?
    memcpy(closes, tc, tn * sizeof(float));
    memcpy(tsOf, tt, tn * sizeof(long));
    nPts = tn;
    gChartMin = mn; gChartMax = mx;
  }
  portEXIT_CRITICAL(&dataMux);
  cursorIdx = -1;
}

// Charge le slot cache de la vue courante (instantané) ; slot vide -> nPts=0
// et drawChart affiche "chargement..." en attendant le fetch frais.
void chartCacheLoad() {
  portENTER_CRITICAL(&dataMux);
  int n = nPtsC[curCur][curTf];
  if (n > 1) {
    memcpy(closes, closesC[curCur][curTf], n * sizeof(float));
    memcpy(tsOf, tsC[curCur][curTf], n * sizeof(long));
    gChartMin = mnC[curCur][curTf]; gChartMax = mxC[curCur][curTf];
  }
  nPts = n;
  portEXIT_CRITICAL(&dataMux);
  cursorIdx = -1;
}

void fetchKlines() { fetchKlinesView(curCur, curTf); }

// ---- Hauteur de bloc (détection nouveau bloc -> evNewBlock) ----
void fetchLastBlock();   // fwd
void fetchHeight() {
  String r;
  if (!httpGet("https://mempool.space/api/blocks/tip/height", r)) return;
  long h = r.toInt();
  if (h <= 0) return;
  if (blockHeight > 0 && h > blockHeight) {
    Serial.printf("[BLOCK] Nouveau bloc %ld !\n", h);
    evNewBlock = true;          // le loop déclenchera anim + dong
    fetchLastBlock();
  }
  blockHeight = h;
  dataOk = true;
}

// ---- Mempool + fees ----
void fetchMempool() {
  String r;
  if (!httpGet("https://mempool.space/api/mempool", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  mempoolCount = doc["count"] | 0L;
  // stats EMA pour la détection d'anomalie mempool
  portENTER_CRITICAL(&dataMux);
  if (anomMemAvg == 0) { anomMemAvg = mempoolCount; anomMemVar = 0; }
  else {
    float d = mempoolCount - anomMemAvg;
    anomMemAvg += 0.03f * d;
    anomMemVar = 0.97f * anomMemVar + 0.03f * d * d;
  }
  anomMemZ = anomMemVar > 0 ? (mempoolCount - anomMemAvg) / sqrtf(anomMemVar) : 0;
  portEXIT_CRITICAL(&dataMux);
}

void fetchFees() {
  String r;
  if (!httpGet("https://mempool.space/api/v1/fees/recommended", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  feeFast = doc["fastestFee"] | 0;
  feeHalf = doc["halfHourFee"] | 0;
  feeHour = doc["hourFee"] | 0;
  feeEco  = doc["economyFee"] | 0;
  if (feeFast > 0) {          // historique pour la régression (page IA)
    portENTER_CRITICAL(&dataMux);
    feeHist[feeHistIdx] = feeFast;
    feeHistIdx = (feeHistIdx + 1) % 32;
    if (feeHistN < 32) feeHistN++;
    // stats EMA pour la détection d'anomalie fees
    if (anomFeeAvg == 0) { anomFeeAvg = feeFast; anomFeeVar = 0; }
    else {
      float d = feeFast - anomFeeAvg;
      anomFeeAvg += 0.03f * d;
      anomFeeVar = 0.97f * anomFeeVar + 0.03f * d * d;
    }
    anomFeeZ = anomFeeVar > 0 ? (feeFast - anomFeeAvg) / sqrtf(anomFeeVar) : 0;
    portEXIT_CRITICAL(&dataMux);
    // apprentissage du créneau horaire (cycles hebdo, persisté en NVS)
    struct tm t;
    if (getLocalTime(&t, 50)) {
      int b = constrain(t.tm_wday * 24 + t.tm_hour, 0, 167);
      portENTER_CRITICAL(&dataMux);
      if (feeBkt[b] <= 0) feeBkt[b] = feeFast;
      else feeBkt[b] = feeBkt[b] * 0.85f + feeFast * 0.15f;
      feeSamples++;
      feeBktDirty = true;
      portEXIT_CRITICAL(&dataMux);
    }
  }
}

// ---- Dernier bloc (pool + nb tx + timestamps pour l'estimation Poisson) ----
void fetchLastBlock() {
  String r;
  if (!httpGet("https://mempool.space/api/v1/blocks", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  JsonObject b = doc[0];
  if (b.isNull()) return;
  long ts[6]; int n = 0;
  for (JsonObject blk : doc.as<JsonArray>()) {
    if (n >= 6) break;
    long t = blk["timestamp"] | 0L;
    if (t > 0) ts[n++] = t;
  }
  portENTER_CRITICAL(&dataMux);
  strlcpy(lastPool, b["extras"]["pool"]["name"] | "-", sizeof(lastPool));
  lastBlockTx = b["tx_count"] | 0L;
  memcpy(blkTs, ts, n * sizeof(long));
  blkTsN = n;
  portEXIT_CRITICAL(&dataMux);
}

// ---- Whale Watch (grosses TX mempool) ----
void fetchWhale() {
  String r;
  if (!httpGet("https://mempool.space/api/mempool/recent", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  long maxV = 0; String maxId = "";
  for (JsonObject tx : doc.as<JsonArray>()) {
    long v = tx["value"] | 0L;
    if (v > maxV) { maxV = v; maxId = tx["txid"] | ""; }
  }
  float btc = maxV / 1e8f;
  if (btc >= 50.0f && maxId.length() > 8) {
    String short8 = maxId.substring(0, 8);
    bool nouveau;
    portENTER_CRITICAL(&dataMux);
    nouveau = (whaleBtc == 0 || short8 != String(whaleTxid));
    if (nouveau) { whaleBtc = btc; strlcpy(whaleTxid, short8.c_str(), sizeof(whaleTxid)); }
    portEXIT_CRITICAL(&dataMux);
    if (nouveau) {
      evWhale = true;           // le loop jouera le dong grave
      Serial.printf("[WHALE] %.1f BTC !\n", btc);
    }
  }
}

// ---- Fear & Greed ----
// ATTENTION : l'API renvoie "value" en CHAÎNE ("25") — l'opérateur `| -1`
// d'ArduinoJson ne parse pas les chaînes et renverrait -1 en permanence.
void fetchFng() {
  String r;
  if (!httpGet("https://api.alternative.me/fng/?limit=1", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  JsonVariant v = doc["data"][0]["value"];
  int fv = -1;
  if (v.is<int>()) fv = v.as<int>();
  else if (v.is<const char*>()) fv = atoi(v.as<const char*>());
  if (fv < 0 || fv > 100) return;
  fngValue = fv;
  portENTER_CRITICAL(&dataMux);
  strlcpy(fngLabel, doc["data"][0]["value_classification"] | "-", sizeof(fngLabel));
  portEXIT_CRITICAL(&dataMux);
  Serial.printf("[FNG] %d (%s)\n", fngValue, fngLabel);
}

// ---- Difficulté ----
void fetchDifficulty() {
  String r;
  if (!httpGet("https://mempool.space/api/v1/difficulty-adjustment", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  diffRemaining = doc["remainingBlocks"] | 0L;
  diffChange = doc["difficultyChange"] | 0.0f;
}

// ---- Pools war (1 semaine) — payload volumineux : filtre obligatoire ----
void fetchPools() {
  String r;
  if (!httpGet("https://mempool.space/api/v1/mining/pools/1w", r, 8000)) return;
  JsonDocument filter;
  filter["blockCount"] = true;
  filter["pools"][0]["name"] = true;
  filter["pools"][0]["blockCount"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, r, DeserializationOption::Filter(filter))) return;
  long total = doc["blockCount"] | 0L;
  JsonArray arr = doc["pools"];
  if (arr.isNull()) return;
  char names[6][20]; long blocks[6]; int n = 0;
  for (JsonObject p : arr) {          // l'API renvoie déjà trié par blockCount desc
    if (n >= 6) break;
    strlcpy(names[n], p["name"] | "?", sizeof(names[n]));
    blocks[n] = p["blockCount"] | 0L;
    n++;
  }
  portENTER_CRITICAL(&dataMux);
  poolsTotal = total; poolsN = n;
  for (int i = 0; i < n; i++) { strlcpy(poolNames[i], names[i], sizeof(poolNames[i])); poolBlocks[i] = blocks[i]; }
  poolsDataAt = millis();
  portEXIT_CRITICAL(&dataMux);
}

// ---- Lightning Network stats ----
void fetchLightning() {
  String r;
  if (!httpGet("https://mempool.space/api/v1/lightning/statistics/latest", r)) return;
  JsonDocument doc;
  if (deserializeJson(doc, r)) return;
  JsonObject l = doc["latest"];
  if (l.isNull()) return;
  double sats = l["total_capacity"].as<double>();   // peut être string ou nombre
  lnCapBtc = (float)(sats / 1e8);
  lnChannels  = l["channel_count"] | 0L;
  lnNodes     = l["node_count"] | 0L;
  lnAvgCap    = l["avg_capacity"] | 0L;
  lnAvgFeePpm = l["avg_fee_rate"] | 0;
}

// ---- Statut nœud Umbrel (simple : répond-il ?) ----
void checkNode() {
  WiFiClient c;
  nodeOnline = false;
  if (c.connect(cfg_nodeip.c_str(), 2105, 1500)) { nodeOnline = true; c.stop(); }
}

// =====================================================================
//  netTask — TOUS les appels réseau (core 0, prio 1, stack 12 Ko)
//  Timers périodiques + requêtes à la demande (bitmask fetchReq).
//  Le loop() ne fait plus aucun I/O bloquant -> tactile réactif.
// =====================================================================
void netTask(void *param) {
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
  unsigned long tHeight = 0, tPrice = 0, tKlines = 0, tMempool = 0, tWhale = 0,
                tDiff = 0, tFng = 0, tNode = 0, tPools = 0, tLn = 0, tKl30 = 0, tBktSave = 0;
  for (;;) {
    unsigned long now = millis();
    // consommation atomique des requêtes UI
    portENTER_CRITICAL(&reqMux);
    uint32_t req = fetchReq; fetchReq = 0;
    portEXIT_CRITICAL(&reqMux);
    bool worked = false;

    if ((req & REQ_HEIGHT)  || now - tHeight  > 20000)   { tHeight = now;  fetchHeight(); worked = true; }
    if ((req & REQ_PRICE)   || now - tPrice   > 30000)   { tPrice = now;   fetchPrice(); worked = true; }
    if ((req & REQ_MEMPOOL) || now - tMempool > 120000)  { tMempool = now; fetchMempool(); fetchFees(); fetchLastBlock(); worked = true; }
    if ((req & REQ_WHALE)   || now - tWhale   > 60000)   { tWhale = now;   fetchWhale(); worked = true; }
    if ((req & REQ_KLINES)  || now - tKlines  > 300000)  { tKlines = now;  fetchKlines(); worked = true; }
    if ((req & REQ_KL30)    || now - tKl30    > 1800000) { tKl30 = now;    if (curTf != TF_30J) fetchKlinesView(curCur, TF_30J); if (curTf != TF_7J) fetchKlinesView(curCur, TF_7J); worked = true; }
    if ((req & REQ_POOLS)   || now - tPools   > 600000)  { tPools = now;   fetchPools(); worked = true; }
    if ((req & REQ_LN)      || now - tLn      > 600000)  { tLn = now;      fetchLightning(); worked = true; }
    if ((req & REQ_DIFF)    || now - tDiff    > 600000)  { tDiff = now;    fetchDifficulty(); worked = true; }
    // FNG : si jamais récupéré, réessayer toutes les 5 min (sinon 1 h)
    if ((req & REQ_FNG) || now - tFng > (fngValue < 0 ? 300000UL : 3600000UL)) { tFng = now; fetchFng(); worked = true; }
    if ((req & REQ_NODE)    || now - tNode    > 60000)   { tNode = now;    checkNode(); worked = true; }

    // sauvegarde NVS des cycles de fees (max 1×/30 min)
    if (feeBktDirty && now - tBktSave > 1800000) { tBktSave = now; saveFeeBuckets(); }

    // latch détection d'anomalies (z > 2.5 -> event, reset z < 1.5)
    bool an = (fabsf(anomMemZ) > 2.5f || fabsf(anomFeeZ) > 2.5f);
    if (an && !anomActive) { anomActive = true; evAnomaly = true; }
    else if (!an && anomActive && fabsf(anomMemZ) < 1.5f && fabsf(anomFeeZ) < 1.5f) anomActive = false;

    if (worked) needRedraw = true;
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// =====================================================================
//  UI HELPERS
// =====================================================================
void setBacklight(uint8_t pct) { ledcWrite(0, (uint32_t)(pct * 10.23)); }

void textCenter(const String &s, int y, uint8_t size, uint16_t color) {
  gfx->setTextSize(size); gfx->setTextColor(color);
  gfx->setCursor((SCR_W - s.length() * 6 * size) / 2, y);
  gfx->print(s);
}

String prettyNum(long v) {
  String p = String(v), out;
  for (int i = 0; i < (int)p.length(); i++) {
    if (i > 0 && (p.length() - i) % 3 == 0) out += ' ';
    out += p[i];
  }
  return out;
}

void drawPill(int x, int y, int w, int h, uint16_t bg, uint16_t fg, const String &txt, uint8_t size = 1) {
  gfx->fillRoundRect(x, y, w, h, h / 2, bg);
  gfx->setTextSize(size); gfx->setTextColor(fg);
  int tw = txt.length() * 6 * size;
  gfx->setCursor(x + (w - tw) / 2, y + (h - 8 * size) / 2);
  gfx->print(txt);
}

void drawArrow(int x, int y, bool up, uint16_t color) {
  if (up) gfx->fillTriangle(x, y + 8, x + 5, y, x + 10, y + 8, color);
  else    gfx->fillTriangle(x, y, x + 5, y + 8, x + 10, y, color);
}

// ---------- batterie : lecture cache + détection de charge ----------
// Mesure sur GPIO5 (ADC1_CH4). ATTENTION : le diviseur RÉEL des cartes
// produites est R1=68K / R2=100K -> V_IO5 = VBAT x 100/168 -> VBAT = VIO5 x 1.68
// (le schéma V1.0 indique 33K/100K = x1.33 : FAUX sur les cartes réelles,
//  ratio 68K/100K confirmé par le firmware yoRadio JC3248W535C).
// Pas de senseur VBUS sur la carte : la charge USB est détectée par la TENSION
// (VBAT >= 4.25 V => l'USB force, ou hausse nette sur ~90 s => charge en cours).
#define BAT_DIV    1.68f   // ratio du diviseur (68K/100K)
#define BAT_OFFSET 0.13f   // calibration (ADC ESP32 ±3-8 % ; yoRadio mesure +0.127 V sur ces cartes)
#define DEBUG_BAT 1        // 1 = log série [BAT] toutes les 5 s (0 pour couper)

float vbatNow = 0.0f;
int   batPctNow = -1;
bool  batCharging = false;
float vbatHist[18];               // 1 échantillon / 5 s -> fenêtre ~90 s
uint8_t vbatHistN = 0, vbatHistIdx = 0;
unsigned long tBatMs = 0;

void updateBattery() {
  if (tBatMs != 0 && millis() - tBatMs < 5000) return;
  tBatMs = millis();
  uint32_t acc = 0;
  for (int i = 0; i < 8; i++) acc += analogReadMilliVolts(PIN_BAT_ADC);
  int raw = (int)(acc / 8);                  // millivolts sur IO5
  vbatNow = raw / 1000.0f * BAT_DIV + BAT_OFFSET;
  batPctNow = constrain(map((long)(vbatNow * 100), 330, 420, 0, 100), 0, 100);
  // historique + détection de charge (hystérésis)
  vbatHist[vbatHistIdx] = vbatNow;
  vbatHistIdx = (vbatHistIdx + 1) % 18;
  if (vbatHistN < 18) vbatHistN++;
  float oldest = vbatHist[vbatHistIdx];      // la plus ancienne de la fenêtre
  if (vbatNow >= 4.25f) batCharging = true;
  else if (vbatHistN >= 12 && (vbatNow - oldest) > 0.06f) batCharging = true;
  else if (vbatHistN >= 12 && (oldest - vbatNow) > 0.04f) batCharging = false;
#if DEBUG_BAT
  Serial.printf("[BAT] io5=%dmV vbat=%.2fV pct=%d charge=%d\n",
                raw, vbatNow, batPctNow, batCharging);
#endif
}

int batteryPct() {
  if (batPctNow >= 0) return batPctNow;
  updateBattery();
  return batPctNow >= 0 ? batPctNow : 0;
}

// snapshots protégés des chaînes partagées
void snapLastBlock(char *dst, size_t n, long *tx) {
  portENTER_CRITICAL(&dataMux);
  strlcpy(dst, lastPool, n); *tx = lastBlockTx;
  portEXIT_CRITICAL(&dataMux);
}
void snapFngLabel(char *dst, size_t n) {
  portENTER_CRITICAL(&dataMux);
  strlcpy(dst, fngLabel, n);
  portEXIT_CRITICAL(&dataMux);
}

// =====================================================================
//  FONT LISSÉE — chiffres anti-aliasés (smooth_font.h, alpha 4 bpp)
// =====================================================================
uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {   // a : 0..15
  if (a == 0) return bg;
  if (a == 15) return fg;
  uint16_t r = (((bg >> 11) & 31) * (15 - a) + ((fg >> 11) & 31) * a) / 15;
  uint16_t g = (((bg >> 5) & 63) * (15 - a) + ((fg >> 5) & 63) * a) / 15;
  uint16_t b = ((bg & 31) * (15 - a) + (fg & 31) * a) / 15;
  return (r << 11) | (g << 5) | b;
}

int sfGlyphIndex(char c) {
  for (int i = 0; i < SF_COUNT; i++)
    if ((char)pgm_read_byte(&SF_CHARS[i]) == c) return i;
  return -1;
}

int smoothWidth(const char *s) {
  int w = 0;
  for (const char *p = s; *p; p++) {
    int gi = sfGlyphIndex(*p);
    if (gi >= 0) { SFGlyph g; memcpy_P(&g, &SF_GLYPHS[gi], sizeof(g)); w += g.adv; }
  }
  return w;
}

// Dessine une chaîne en chiffres lissés. Retourne la largeur dessinée.
// Les pixels alpha=0 sont ignorés → le fond existant est préservé.
int drawSmooth(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
  int pen = x;
  for (const char *p = s; *p; p++) {
    int gi = sfGlyphIndex(*p);
    if (gi < 0) continue;
    SFGlyph g; memcpy_P(&g, &SF_GLYPHS[gi], sizeof(g));
    int stride = (g.w + 1) / 2;
    for (int yy = 0; yy < g.h; yy++)
      for (int xx = 0; xx < g.w; xx++) {
        uint8_t byte = pgm_read_byte(&SF_DATA[g.offset + yy * stride + xx / 2]);
        uint8_t a = (xx & 1) ? (byte & 0x0F) : (byte >> 4);
        if (a) gfx->drawPixel(pen + g.xoff + xx, y + g.yoff + yy, blend565(bg, fg, a));
      }
    pen += g.adv;
  }
  return pen - x;
}

// ---------- header / footer communs ----------
void drawHeader() {
  struct tm t;
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(10, 8);
  if (getLocalTime(&t, 50)) gfx->printf("%02d:%02d", t.tm_hour, t.tm_min); else gfx->print("--:--");
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(78, 12);
  if (getLocalTime(&t, 50)) gfx->printf("%02d/%02d", t.tm_mday, t.tm_mon + 1);
  // wifi : dot 1 = état (vert connecté / rouge déconnecté),
  //        dots 2-3 = force du signal (RSSI)
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  // gestion du son : icône HP à gauche des dots WiFi (tap = cycle volume)
  gfx->fillRect(346, 10, 4, 6, C_GREY);                       // corps du HP
  gfx->fillTriangle(350, 7, 350, 19, 356, 13, C_GREY);        // pavillon
  if (sndVolPct == 0) {                                       // muet : croix rouge
    gfx->drawLine(345, 6, 367, 20, C_RED);
    gfx->drawLine(367, 6, 345, 20, C_RED);
  } else {                                                    // ondes selon le niveau
    for (float a = -1.0f; a <= 1.0f; a += 0.12f) {
      if (sndVolPct >= 30)
        gfx->drawPixel(358 + (int)(cosf(a) * 4), 13 + (int)(sinf(a) * 4), C_GREY);
      if (sndVolPct >= 60)
        gfx->drawPixel(358 + (int)(cosf(a) * 7), 13 + (int)(sinf(a) * 7), C_GREY);
      if (sndVolPct >= 100)
        gfx->drawPixel(358 + (int)(cosf(a) * 10), 13 + (int)(sinf(a) * 10), C_GREY);
    }
  }
  gfx->fillCircle(384, 13, 3, wifiOk ? C_GREEN : C_RED);
  long rssi = wifiOk ? WiFi.RSSI() : -127;
  gfx->fillCircle(393, 13, 3, wifiOk && rssi > -70 ? C_GREEN : C_DGREY);
  gfx->fillCircle(402, 13, 3, wifiOk && rssi > -55 ? C_GREEN : C_DGREY);
  // batterie : icône avec le % intégré (lecture GPIO5, diviseur 68K/100K) ;
  // la jauge CLIGNOTE pendant la charge (batCharging, voir updateBattery)
  int bp = batteryPct();
  gfx->drawRect(444, 7, 28, 12, C_GREY);
  gfx->fillRect(472, 10, 3, 6, C_GREY);
  bool batBlinkOn = !batCharging || ((millis() / 800) % 2 == 0);
  if (batBlinkOn)
    gfx->fillRect(446, 9, (int)(24 * bp / 100.0f), 8, bp > 25 ? C_GREEN : C_RED);
  gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
  char bps[8];
  if (batPctNow >= 0) snprintf(bps, sizeof(bps), "%d%%", bp);
  else strlcpy(bps, "--", sizeof(bps));
  gfx->setCursor(444 + (28 - (int)strlen(bps) * 6) / 2, 9);
  gfx->print(bps);
  gfx->drawFastHLine(0, 26, SCR_W, C_LINE);
}

// barre d'onglets : 9 icônes, tap = accès direct à la page
#define NAV_Y   (SCR_H - 26)
#define TAB_W   (SCR_W / PG_COUNT)

void drawTabIcon(int i, int cx, int cy, uint16_t c) {
  switch (i) {
    case PG_PRICE:   // mini courbe ascendante
      gfx->drawLine(cx - 8, cy + 5, cx - 3, cy, c);
      gfx->drawLine(cx - 3, cy, cx + 1, cy + 3, c);
      gfx->drawLine(cx + 1, cy + 3, cx + 8, cy - 6, c);
      break;
    case PG_CHAIN:   // cube 3D
      gfx->drawRect(cx - 7, cy - 3, 10, 10, c);
      gfx->drawLine(cx - 7, cy - 3, cx - 3, cy - 7, c);
      gfx->drawLine(cx + 3, cy - 3, cx + 7, cy - 7, c);
      gfx->drawLine(cx - 3, cy - 7, cx + 7, cy - 7, c);
      gfx->drawLine(cx + 7, cy - 7, cx + 7, cy + 3, c);
      gfx->drawLine(cx + 3, cy + 7, cx + 7, cy + 3, c);
      break;
    case PG_CUBE:    // bac qui se remplit
      gfx->drawRect(cx - 6, cy - 7, 12, 14, c);
      gfx->fillRect(cx - 6, cy + 1, 12, 6, c);
      break;
    case PG_POOLS:   // podium (3 barres)
      gfx->fillRect(cx - 9, cy - 1, 5, 8, c);
      gfx->fillRect(cx - 2, cy - 6, 5, 13, c);
      gfx->fillRect(cx + 5, cy + 2, 5, 5, c);
      break;
    case PG_LN: {    // éclair
      gfx->fillTriangle(cx + 2, cy - 7, cx - 5, cy + 1, cx, cy + 1, c);
      gfx->fillTriangle(cx + 2, cy - 7, cx, cy + 1, cx + 5, cy - 1, c);
      gfx->fillTriangle(cx - 2, cy + 7, cx, cy - 1, cx + 5, cy - 1, c);
      break;
    }
    case PG_NODE: {  // jauge + aiguille
      for (float a = 0; a < PI; a += 0.28f)
        gfx->drawPixel(cx + (int)(cosf(a) * 7), cy + 4 - (int)(sinf(a) * 7), c);
      gfx->drawLine(cx, cy + 4, cx + 4, cy - 1, c);
      break;
    }
    case PG_AI:      // mini réseau de neurones
      gfx->drawLine(cx - 5, cy + 4, cx, cy - 5, c);
      gfx->drawLine(cx, cy - 5, cx + 5, cy + 4, c);
      gfx->drawLine(cx - 5, cy + 4, cx + 5, cy + 4, c);
      gfx->fillCircle(cx - 5, cy + 4, 2, c);
      gfx->fillCircle(cx + 5, cy + 4, 2, c);
      gfx->fillCircle(cx, cy - 5, 2, c);
      break;
    case PG_SIG:     // divergence : flèches opposées
      gfx->drawLine(cx - 8, cy + 4, cx + 6, cy - 4, c);
      gfx->fillTriangle(cx + 6, cy - 4, cx + 1, cy - 5, cx + 5, cy, c);
      gfx->drawLine(cx - 8, cy - 4, cx + 6, cy + 4, c);
      gfx->fillTriangle(cx + 6, cy + 4, cx + 5, cy - 1, cx + 1, cy + 5, c);
      break;
    case PG_DOOM:    // viseur
      gfx->drawCircle(cx, cy, 6, c);
      gfx->fillCircle(cx, cy, 2, c);
      break;
  }
}

void drawFooter() {
  gfx->drawFastHLine(0, NAV_Y, SCR_W, C_LINE);
  for (int i = 0; i < PG_COUNT; i++) {
    int cx = i * TAB_W + TAB_W / 2;
    bool act = (i == page);
    drawTabIcon(i, cx, SCR_H - 14, act ? C_ORANGE : C_DGREY);
    if (act) gfx->fillRect(i * TAB_W + TAB_W / 2 - 16, SCR_H - 3, 32, 3, C_ORANGE);
  }
}

// =====================================================================
//  PAGE 0 — PRIX + GRAPHE
// =====================================================================
#define GX 186
#define GY 66
#define GW 284
#define GH 190

// drawChart optimisé : polyline en drawLine natif (2 passes),
// fill dégradé en drawFastVLine tous les 2 px (≈ x10 plus rapide
// que l'ancien remplissage drawPixel par drawPixel).
void drawChart() {
  gfx->fillRoundRect(GX - 6, GY - 8, GW + 12, GH + 40, 8, C_PANEL);
  // snapshot protégé des données (écrites par netTask)
  static float c[MAX_PTS]; static long ts[MAX_PTS];
  portENTER_CRITICAL(&dataMux);
  int n = nPts;
  memcpy(c, closes, n * sizeof(float));
  memcpy(ts, tsOf, n * sizeof(long));
  portEXIT_CRITICAL(&dataMux);
  if (n < 2) { textCenter("chargement...", GY + GH / 2, 1, C_GREY); return; }

  float mn = c[0], mx = c[0];
  for (int i = 1; i < n; i++) { if (c[i] < mn) mn = c[i]; if (c[i] > mx) mx = c[i]; }
  float range = (mx - mn); if (range < 0.01f) range = 0.01f;

  auto yOf = [&](float v) { return GY + GH - 6 - (int)((v - mn) / range * (GH - 14)); };
  auto xOf = [&](int i) { return GX + 4 + (int)((long)i * (GW - 10) / (n - 1)); };

  // pointillés prix d'ouverture
  int yOpen = yOf(c[0]);
  for (int x = GX + 4; x < GX + GW - 6; x += 6) gfx->fillRect(x, yOpen, 3, 1, C_DGREY);

  // fill dégradé : vlines tous les 2 px (tiers haut orange foncé, reste panel)
  int base = GY + GH - 4;
  for (int i = 0; i < n - 1; i++) {
    int x0 = xOf(i), x1 = xOf(i + 1);
    int y0 = yOf(c[i]), y1 = yOf(c[i + 1]);
    for (int x = x0; x <= x1; x += 2) {
      int y = y0 + (int)((long)(y1 - y0) * (x - x0) / max(1, x1 - x0));
      int h = base - y;
      if (h <= 0) continue;
      int h1 = h / 3;
      if (h1 > 0) gfx->drawFastVLine(x, y, h1, C_ORANGE_D);
      if (h - h1 > 0) gfx->drawFastVLine(x, y + h1, h - h1, C_PANEL);
    }
  }

  // polyline native, 2 passes pour l'épaisseur
  bool up = c[n - 1] >= c[0];
  uint16_t lc = up ? C_GREEN : C_RED;
  for (int i = 0; i < n - 1; i++) {
    int x0 = xOf(i), y0 = yOf(c[i]);
    int x1 = xOf(i + 1), y1 = yOf(c[i + 1]);
    gfx->drawLine(x0, y0, x1, y1, lc);
    gfx->drawLine(x0, y0 + 1, x1, y1 + 1, lc);
  }

  // dot dernier point
  gfx->fillCircle(xOf(n - 1), yOf(c[n - 1]), 4, lc);
  gfx->fillCircle(xOf(n - 1), yOf(c[n - 1]), 2, C_WHITE);

  // curseur tactile
  if (cursorIdx >= 0 && cursorIdx < n) {
    int cx = xOf(cursorIdx), cy = yOf(c[cursorIdx]);
    for (int y = GY; y < GY + GH; y += 4) gfx->fillRect(cx, y, 1, 2, C_GREY);
    gfx->fillCircle(cx, cy, 4, C_WHITE);
    char lbl[48];
    time_t tt2 = (time_t)ts[cursorIdx];
    struct tm *lt = localtime(&tt2);
    snprintf(lbl, sizeof(lbl), "%s %02d:%02d", prettyNum((long)c[cursorIdx]).c_str(), lt->tm_hour, lt->tm_min);
    int lx = constrain(cx - 60, GX, GX + GW - 120);
    drawPill(lx, GY - 2, 120, 18, C_BG, C_YELLOW, lbl, 1);
  }

  // labels min/max
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(GX + 4, GY + 2);  gfx->print(prettyNum((long)mx));
  gfx->setCursor(GX + 4, GY + GH - 10); gfx->print(prettyNum((long)mn));
}

void drawPagePrice() {
  // colonne gauche : logo + prix + variation + devise
  gfx->draw16bitRGBBitmapWithTranColor(52, 34, (uint16_t*)BTC_LOGO_64, TRANSP, 64, 64);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(52, 104); gfx->print("BTC / " + String(CUR_LABEL[curCur]));
  // prix en chiffres lissés (animé via dispPrice)
  float p = (dispPrice > 0) ? dispPrice : btcPrice[curCur];
  if (p > 0) drawSmooth(10, 118, prettyNum((long)p).c_str(), C_WHITE, C_BG);
  else { gfx->setTextSize(3); gfx->setTextColor(C_WHITE); gfx->setCursor(10, 124); gfx->print("---"); }
  bool upDay = btcChg24 >= 0;
  char chg[16]; snprintf(chg, sizeof(chg), "%s%.2f%%", upDay ? "+" : "", btcChg24);
  drawPill(10, 166, 96, 20, upDay ? C_GREEN_D : C_RED_D, upDay ? C_GREEN : C_RED, chg, 1);
  drawArrow(114, 172, upDay, upDay ? C_GREEN : C_RED);
  // devise (tap = switch)
  drawPill(10, 194, 70, 22, C_PANEL, C_ORANGE, CUR_LABEL[curCur], 2);
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(86, 200); gfx->print("< tap");
  // seuils actifs
  gfx->setCursor(10, 226);
  if (alertHi > 0) gfx->printf("alerte > %s", prettyNum((long)alertHi).c_str());
  gfx->setCursor(10, 240);
  if (alertLo > 0) gfx->printf("alerte < %s", prettyNum((long)alertLo).c_str());
  // whale mini
  if (whaleBtc >= 50) {
    gfx->setTextColor(C_ORANGE);
    gfx->setCursor(10, 262); gfx->printf("baleine: %.0f BTC", whaleBtc);
  }

  // onglets timeframe
  for (int i = 0; i < TF_COUNT; i++) {
    bool active = (i == curTf);
    drawPill(GX + i * 72, 34, 66, 22, active ? C_ORANGE : C_PANEL, active ? C_BG : C_GREY, TF_LABEL[i], 2);
  }
  drawChart();
}

// =====================================================================
//  PAGE 1 — ON-CHAIN
// =====================================================================
void drawPageChain() {
  // bloc
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(12, 36); gfx->print("BLOCK HEIGHT");
  if (blockHeight > 0) drawSmooth(10, 50, String(blockHeight).c_str(), C_WHITE, C_BG);
  else { gfx->setTextSize(5); gfx->setTextColor(C_WHITE); gfx->setCursor(10, 50); gfx->print("------"); }
  unsigned long el = blockDetectedMs ? (millis() - blockDetectedMs) / 1000 : 0;
  char pool[32]; long btx; snapLastBlock(pool, sizeof(pool), &btx);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(12, 96);
  gfx->printf("il y a %lum%02lus  ·  %s  ·  %ld tx", el / 60, el % 60, pool, btx);
  int prog = constrain((int)(el * 280 / 600), 0, 280);
  gfx->fillRoundRect(10, 108, 280, 8, 4, C_PANEL);
  if (prog > 6) gfx->fillRoundRect(10, 108, prog, 8, 4, C_ORANGE);

  // colonne droite : mempool + halving
  gfx->fillRoundRect(306, 34, 164, 84, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(316, 42); gfx->print("MEMPOOL");
  gfx->setTextSize(3); gfx->setTextColor(C_WHITE);
  gfx->setCursor(316, 56); gfx->print(prettyNum(mempoolCount));
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(316, 82); gfx->print("TX en attente");
  long daysH = (1050000L - blockHeight) * 10L / 1440L;
  gfx->setCursor(316, 98); gfx->printf("halving ~%ld j", daysH);

  // fees
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(12, 132); gfx->print("FEES sat/vB");
  const char* fl[4] = {"rapide", "30min", "1h", "eco"};
  int fv[4] = {feeFast, feeHalf, feeHour, feeEco};
  for (int i = 0; i < 4; i++) {
    int fx = 10 + i * 78;
    gfx->fillRoundRect(fx, 144, 72, 36, 6, C_PANEL);
    gfx->setTextSize(2); gfx->setTextColor(i == 0 ? C_ORANGE : C_WHITE);
    gfx->setCursor(fx + 8, 148); gfx->print(fv[i] > 0 ? String(fv[i]) : "-");
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(fx + 8, 166); gfx->print(fl[i]);
  }

  // difficulté
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(12, 196);
  gfx->printf("ajustement difficulte : %ld blocs", diffRemaining);
  gfx->setCursor(12, 210);
  gfx->setTextColor(diffChange >= 0 ? C_GREEN : C_RED);
  gfx->printf("estimation %s%.2f %%", diffChange >= 0 ? "+" : "", diffChange);

  // whale
  gfx->fillRoundRect(10, 234, 460, 56, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(20, 242); gfx->print("WHALE WATCH (mempool)");
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(20, 258);
  if (whaleBtc >= 50) gfx->printf("%.1f BTC en transit !", whaleBtc);
  else gfx->print("calme plat...");
}

// =====================================================================
//  PAGE 5 — NŒUD & SENTIMENT
// =====================================================================
void drawGauge(int cx, int cy, int r, int value) {
  // segments pleins (quads triangulaires) : aucune strie, rendu net et rapide
  const uint16_t segC[5] = {C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_GREEN_D};
  int r0 = r - 15, r1 = r;
  const float step = 0.035f;
  for (int seg = 0; seg < 5; seg++) {
    float a0 = PI - seg * PI / 5;
    float a1 = PI - (seg + 1) * PI / 5;
    for (float a = a0; a > a1; a -= step) {
      float an = max(a - step, a1);
      int x0a = cx + (int)(cosf(a)  * r0), y0a = cy - (int)(sinf(a)  * r0);
      int x1a = cx + (int)(cosf(a)  * r1), y1a = cy - (int)(sinf(a)  * r1);
      int x0b = cx + (int)(cosf(an) * r0), y0b = cy - (int)(sinf(an) * r0);
      int x1b = cx + (int)(cosf(an) * r1), y1b = cy - (int)(sinf(an) * r1);
      gfx->fillTriangle(x0a, y0a, x1a, y1a, x1b, y1b, segC[seg]);
      gfx->fillTriangle(x0a, y0a, x1b, y1b, x0b, y0b, segC[seg]);
    }
  }
  // aiguille épaisse (3 lignes) + moyeu
  float ang = PI - (value / 100.0f) * PI;
  int nx = cx + (int)(cosf(ang) * (r - 26)), ny = cy - (int)(sinf(ang) * (r - 26));
  gfx->drawLine(cx, cy, nx, ny, C_WHITE);
  gfx->drawLine(cx + 1, cy, nx + 1, ny, C_WHITE);
  gfx->drawLine(cx, cy + 1, nx, ny + 1, C_WHITE);
  gfx->fillCircle(cx, cy, 5, C_WHITE);
  gfx->fillCircle(cx, cy, 2, C_BG);
}

void drawPageNode() {
  // Fear & Greed
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(20, 36); gfx->print("FEAR & GREED INDEX");
  drawGauge(110, 170, 92, fngValue >= 0 ? fngValue : 50);
  // valeur + label centrés sur l'axe de la jauge (cx=110), pas sur l'écran
  String fv = fngValue >= 0 ? String(fngValue) : "-";
  drawSmooth(110 - smoothWidth(fv.c_str()) / 2, 176, fv.c_str(), C_WHITE, C_BG);
  char fng[20]; snapFngLabel(fng, sizeof(fng));
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(110 - (int)strlen(fng) * 3, 224); gfx->print(fng);

  // Mon nœud
  gfx->fillRoundRect(236, 34, 234, 120, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(248, 44); gfx->print("MON NOEUD UMBREL");
  gfx->fillCircle(252, 74, 5, nodeOnline ? C_GREEN : C_RED);
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(266, 66); gfx->print(nodeOnline ? "en ligne" : "hors ligne");
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(248, 96); gfx->print(cfg_nodeip + " :2105");
  gfx->setCursor(248, 112); gfx->print("Bitcoin Knots");
  gfx->setCursor(248, 132); gfx->setTextColor(C_DGREY);
  gfx->print("detail sync: bientot");

  // infos diverses
  gfx->fillRoundRect(236, 164, 234, 126, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(248, 174); gfx->print("RESEAU");
  gfx->setTextColor(C_GREY);
  gfx->setCursor(248, 192); gfx->printf("bloc : %ld", blockHeight);
  gfx->setCursor(248, 208); gfx->printf("mempool : %ld TX", mempoolCount);
  gfx->setCursor(248, 224); gfx->printf("fee rapide : %d sat/vB", feeFast);
  if (whaleBtc >= 50) {
    gfx->setTextColor(C_ORANGE);
    gfx->setCursor(248, 248); gfx->printf("baleine : %.0f BTC", whaleBtc);
  }
}

// =====================================================================
//  PAGE 2 — CUBE : mempool particle art
//  Un cube isométrique se remplit de particules-transactions (niveau =
//  mempoolCount / 4000). Nouveau bloc -> une chaîne descend, s'accroche
//  et emporte le cube vers la gauche ; un nouveau cube arrive de droite.
// =====================================================================
#define CUBE_CX 240
#define CUBE_CY 214
#define CUBE_H  40          // demi-arête (unités 3D)
#define CUBE_ZH 80          // hauteur totale = 2 * CUBE_H
#define CUBE_MAXP 220       // particules max (fixe)
#define CUBE_GRID 216       // slots intérieurs 6x6x6

enum { PS_FREE = 0, PS_FALL, PS_SET, PS_ESC };
enum { CS_FILL = 0, CS_LOWER, CS_DRAG, CS_SPAWN };

struct Particle { float x, y, vx, vy; uint8_t state; uint8_t slot; };
Particle parts[CUBE_MAXP];            // zero-init = PS_FREE
int      settledCount = 0;
uint8_t  cubeState = CS_FILL;
unsigned long cubeSeqMs = 0;
float    cubeOffX = 0, cubeLift = 0, cubeTilt = 0;

// projection 3D -> 2D isométrique (+ tilt pendant le drag)
void cubeProj(float x, float y, float z, int &sx, int &sy) {
  float c = cosf(cubeTilt), s = sinf(cubeTilt);
  float xr = x * c - y * s, yr = x * s + y * c;
  sx = (int)(CUBE_CX + cubeOffX + (xr - yr) * 0.866f);
  sy = (int)(CUBE_CY - cubeLift + (xr + yr) * 0.433f - z);
}

// slot grille 6x6x6 -> position 3D (remplissage bas -> haut)
void slotPos(uint8_t k, float &x, float &y, float &z) {
  int gx = k % 6, gy = (k / 6) % 6, gz = k / 36;
  x = -33.0f + gx * 13.2f;
  y = -33.0f + gy * 13.2f;
  z = 6.0f + gz * 13.2f;
}

int cubeTargetSettled() {
  float lvl = mempoolCount / 4000.0f;      // un bloc ≈ 4000 tx
  if (mempoolCount <= 0) lvl = 0.08f;      // visuel par défaut avant données
  if (lvl > 1.0f) lvl = 1.0f;
  return (int)(lvl * 200);
}

int cubeCountFalling() {
  int n = 0;
  for (int i = 0; i < CUBE_MAXP; i++) if (parts[i].state == PS_FALL) n++;
  return n;
}

void cubeSpawnFalling(int budget) {
  int falling = cubeCountFalling();
  for (int i = 0; i < CUBE_MAXP && budget > 0; i++) {
    if (parts[i].state != PS_FREE) continue;
    int slot = settledCount + falling;
    if (slot >= CUBE_GRID) return;
    parts[i].state = PS_FALL;
    parts[i].slot = (uint8_t)slot;
    parts[i].x = CUBE_CX + random(-90, 91);
    parts[i].y = 28;
    parts[i].vx = random(-40, 41) / 100.0f;
    parts[i].vy = 1.0f + random(0, 100) / 100.0f;
    falling++; budget--;
  }
}

// libère n particules empilées (elles s'échappent pendant le drag)
void cubeReleaseEscapees(int n) {
  for (int i = CUBE_MAXP - 1; i >= 0 && n > 0; i--) {
    if (parts[i].state != PS_SET) continue;
    parts[i].state = PS_ESC;
    parts[i].vx = random(-160, 41) / 100.0f;    // plutôt vers la gauche
    parts[i].vy = -random(50, 200) / 100.0f;
    n--;
  }
}

// physique des particules qui s'échappent (tous états)
void cubeUpdateEsc() {
  for (int i = 0; i < CUBE_MAXP; i++) {
    Particle &p = parts[i];
    if (p.state != PS_ESC) continue;
    p.vy += 0.15f; p.x += p.vx; p.y += p.vy;
    if (p.y > SCR_H + 10 || p.x < -10) p.state = PS_FREE;
  }
}

// physique FILLING : chute + empilement, ajustement vers la cible
void cubeUpdateFill() {
  int target = cubeTargetSettled();
  // la cible a baissé (bloc miné) : le surplus s'échappe
  while (settledCount > target) {
    bool found = false;
    for (int i = 0; i < CUBE_MAXP; i++) {
      if (parts[i].state == PS_SET && parts[i].slot == settledCount - 1) {
        parts[i].state = PS_ESC;
        parts[i].vx = random(-100, 101) / 100.0f;
        parts[i].vy = -random(50, 150) / 100.0f;
        found = true;
        break;
      }
    }
    settledCount--;
    if (!found) break;
  }
  if (settledCount + cubeCountFalling() < target) cubeSpawnFalling(2);

  for (int i = 0; i < CUBE_MAXP; i++) {
    Particle &p = parts[i];
    if (p.state != PS_FALL) continue;
    float fx, fy, fz; slotPos(p.slot, fx, fy, fz);
    int tx, ty; cubeProj(fx, fy, fz, tx, ty);
    p.vy += 0.12f; if (p.vy > 4.5f) p.vy = 4.5f;
    p.x += (tx - p.x) * 0.10f + p.vx;      // guidage vers l'ouverture du cube
    p.y += p.vy;
    if (p.y >= ty - 2) { p.state = PS_SET; p.x = (float)tx; p.y = (float)ty; settledCount++; }
  }
}

void cubeDrawEdges() {
  static const int8_t V[8][3] = {
    {-CUBE_H, -CUBE_H, 0}, {CUBE_H, -CUBE_H, 0}, {CUBE_H, CUBE_H, 0}, {-CUBE_H, CUBE_H, 0},
    {-CUBE_H, -CUBE_H, CUBE_ZH}, {CUBE_H, -CUBE_H, CUBE_ZH}, {CUBE_H, CUBE_H, CUBE_ZH}, {-CUBE_H, CUBE_H, CUBE_ZH}
  };
  static const uint8_t E[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}
  };
  for (int e = 0; e < 12; e++) {
    for (int s = 0; s <= 16; s += 2) {   // arêtes en pointillés orange
      float t = s / 16.0f;
      int px, py;
      cubeProj(V[E[e][0]][0] + (V[E[e][1]][0] - V[E[e][0]][0]) * t,
               V[E[e][0]][1] + (V[E[e][1]][1] - V[E[e][0]][1]) * t,
               V[E[e][0]][2] + (V[E[e][1]][2] - V[E[e][0]][2]) * t, px, py);
      gfx->drawPixel(px, py, C_ORANGE);
    }
  }
}

void cubeDrawParticles() {
  for (int i = 0; i < CUBE_MAXP; i++) {
    Particle &p = parts[i];
    if (p.state == PS_FREE) continue;
    int px, py;
    if (p.state == PS_SET) {   // position recalculée (le cube peut bouger)
      float fx, fy, fz; slotPos(p.slot, fx, fy, fz);
      cubeProj(fx, fy, fz, px, py);
      gfx->fillRect(px, py, 2, 2, (p.slot % 5 == 0) ? C_YELLOW : C_ORANGE);
    } else {
      px = (int)p.x; py = (int)p.y;
      gfx->fillRect(px, py, 2, 2, p.state == PS_ESC ? C_ORANGE_D : C_ORANGE);
    }
  }
}

// chaîne = maillons alternés vertical / horizontal
void cubeDrawChain(int topY, int botY, int cx) {
  int i = 0;
  for (int y = topY; y < botY; y += 8, i++) {
    if (i % 2 == 0) gfx->drawRoundRect(cx - 2, y, 5, 8, 2, C_GREY);
    else            gfx->drawRoundRect(cx - 4, y + 1, 9, 5, 2, C_GREY);
  }
}

// déclenché par le loop sur evNewBlock (page Cube visible uniquement)
void cubeBlockSeq() {
  if (cubeState == CS_FILL) { cubeState = CS_LOWER; cubeSeqMs = millis(); }
}

void drawPageCube() {
  unsigned long now = millis();
  unsigned long e = now - cubeSeqMs;

  // ---- machine à états de la séquence nouveau bloc ----
  switch (cubeState) {
    case CS_LOWER: {
      cubeUpdateFill();
      if (e >= 800) { cubeState = CS_DRAG; cubeSeqMs = now; cubeReleaseEscapees(14); }
      break;
    }
    case CS_DRAG: {
      float k = e / 1200.0f; if (k > 1.0f) k = 1.0f;
      cubeOffX = -360.0f * k * k;        // accélère vers la gauche
      cubeLift = 26.0f * k;
      cubeTilt = 0.45f * k;
      if (e >= 1200) {
        for (int i = 0; i < CUBE_MAXP; i++) parts[i].state = PS_FREE;
        settledCount = 0;
        cubeOffX = 380; cubeLift = 0; cubeTilt = 0;
        cubeState = CS_SPAWN; cubeSeqMs = now;
      }
      break;
    }
    case CS_SPAWN: {
      float k = e / 500.0f; if (k > 1.0f) k = 1.0f;
      float ke = 1.0f - (1.0f - k) * (1.0f - k);   // ease-out
      cubeOffX = 380.0f * (1.0f - ke);
      if (e >= 500) { cubeState = CS_FILL; cubeOffX = 0; }
      break;
    }
    default:
      cubeUpdateFill();
      break;
  }
  cubeUpdateEsc();

  // ---- dessin ----
  cubeDrawEdges();
  cubeDrawParticles();

  // chaîne pendant LOWER / DRAG
  if (cubeState == CS_LOWER || cubeState == CS_DRAG) {
    int tx, ty; cubeProj(0, 0, CUBE_ZH, tx, ty);   // sommet du cube
    int botY;
    if (cubeState == CS_LOWER) {
      float k = e / 800.0f; if (k > 1.0f) k = 1.0f;
      botY = 28 + (int)((ty - 28) * k);
    } else botY = ty;
    cubeDrawChain(28, botY, tx);
  }

  // ---- HUD sobre ----
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(10, 34); gfx->print("MEMPOOL");
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(10, 46); gfx->print(prettyNum(mempoolCount));
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(SCR_W - 70, 34); gfx->print("FEE");
  gfx->setTextSize(2); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(SCR_W - 70, 46); gfx->printf("%d", feeFast);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(10, 282); gfx->printf("bloc %ld", blockHeight);
  char pool[32]; long btx; snapLastBlock(pool, sizeof(pool), &btx);
  gfx->setCursor(SCR_W - 130, 282); gfx->print(pool);
}

// =====================================================================
//  PAGE 3 — POOLS WAR : course animée des pools (1 semaine)
// =====================================================================
const uint16_t POOL_COL[6] = {C_ORANGE, C_GREEN, C_YELLOW, C_BLUE, C_RED, C_GREY};
float poolShown[6] = {0, 0, 0, 0, 0, 0};       // largeurs animées
bool  poolsReset = true;                        // remise à zéro à l'entrée
unsigned long poolsAnimUntil = 0;               // fin de la phase 30 FPS

void drawPagePools() {
  // snapshot protégé des données partagées
  char names[6][20]; long blocks[6]; long total; int n; unsigned long dataAt;
  portENTER_CRITICAL(&dataMux);
  n = poolsN; total = poolsTotal; dataAt = poolsDataAt;
  for (int i = 0; i < n; i++) { strlcpy(names[i], poolNames[i], 20); blocks[i] = poolBlocks[i]; }
  portEXIT_CRITICAL(&dataMux);

  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(12, 36); gfx->print("POOLS WAR — blocs mines sur 7 jours");
  gfx->setTextColor(C_GREY);
  gfx->setCursor(360, 36);
  if (total > 0) gfx->printf("total %ld", total);

  if (n == 0) { textCenter("chargement...", 160, 2, C_GREY); return; }

  // arrivée sur la page : course depuis zéro ; nouvelles données : nouvelle cible
  if (poolsReset) {
    for (int i = 0; i < 6; i++) poolShown[i] = 0;
    poolsReset = false;
    poolsAnimUntil = millis() + 1500;
  }
  static unsigned long lastDataSeen = 0;
  if (dataAt != lastDataSeen) { lastDataSeen = dataAt; poolsAnimUntil = millis() + 1500; }

  long mx = blocks[0] > 0 ? blocks[0] : 1;
  bool moving = false;
  for (int i = 0; i < n; i++) {
    int y = 62 + i * 38;
    float target = 268.0f * blocks[i] / mx;
    poolShown[i] += (target - poolShown[i]) * 0.15f;
    if (fabsf(target - poolShown[i]) > 1.0f) moving = true;
    int w = (int)poolShown[i];
    // nom (12 chars max)
    char nm[13]; strlcpy(nm, names[i], sizeof(nm));
    gfx->setTextSize(1); gfx->setTextColor(i == 0 ? C_ORANGE : C_WHITE);
    gfx->setCursor(10, y + 6); gfx->print(nm);
    // piste + barre animée
    gfx->fillRoundRect(140, y, 272, 20, 5, C_PANEL);
    if (w > 8) gfx->fillRoundRect(140, y, w, 20, 5, POOL_COL[i]);
    // marqueur #1 : petit pic orange au-dessus de la barre
    if (i == 0) gfx->fillTriangle(146, y - 2, 152, y - 8, 158, y - 2, C_ORANGE);
    // nb blocs + part
    gfx->setTextColor(C_GREY);
    gfx->setCursor(420, y + 6);
    int share = total > 0 ? (int)(blocks[i] * 100 / total) : 0;
    gfx->printf("%ld %d%%", blocks[i], share);
  }
  if (moving) poolsAnimUntil = millis() + 250;   // laisser les barres finir leur course
}

// =====================================================================
//  PAGE 4 — LIGHTNING NETWORK
// =====================================================================
void drawLnTile(int x, int y, const char *label, const String &val, const char *unit) {
  gfx->fillRoundRect(x, y, 204, 66, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(x + 10, y + 10); gfx->print(label);
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(x + 10, y + 30); gfx->print(val);
  if (unit) {
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(x + 12 + val.length() * 12, y + 36); gfx->print(unit);
  }
}

void drawPageLightning() {
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(12, 36); gfx->print("LIGHTNING NETWORK");
  if (lnNodes == 0 && lnCapBtc <= 0) { textCenter("chargement...", 160, 2, C_GREY); return; }

  // gros chiffre capacité réseau (chiffres lissés)
  char cap[16]; snprintf(cap, sizeof(cap), "%.0f", lnCapBtc);
  int cw = drawSmooth(24, 56, cap, C_WHITE, C_BG);
  gfx->setTextSize(2); gfx->setTextColor(C_GREY);
  gfx->setCursor(24 + cw + 8, 74); gfx->print("BTC");
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(26, 100); gfx->print("capacite totale du reseau");

  // éclair stylé : polygone plein + ombre portée + contour + reflet
  static const int8_t bolt[7][2] = {{18,0},{40,0},{24,29},{35,29},{5,71},{13,37},{3,37}};
  const int bx = 330, by = 44;
  auto px = [&](int i) { return bx + bolt[i][0]; };
  auto py = [&](int i) { return by + bolt[i][1]; };
  for (int i = 1; i < 6; i++)                       // ombre portée
    gfx->fillTriangle(px(0)+3, py(0)+3, px(i)+3, py(i)+3, px(i+1)+3, py(i+1)+3, C_ORANGE_D);
  for (int i = 1; i < 6; i++)                       // corps jaune
    gfx->fillTriangle(px(0), py(0), px(i), py(i), px(i+1), py(i+1), C_YELLOW);
  for (int i = 0; i < 7; i++)                       // contour orange
    gfx->drawLine(px(i), py(i), px((i+1)%7), py((i+1)%7), C_ORANGE);
  gfx->drawLine(px(6), py(6), px(0), py(0), C_WHITE); // reflets
  gfx->drawLine(px(0), py(0), px(1), py(1), C_WHITE);

  // tuiles stats
  drawLnTile(24, 132, "CHANNELS", prettyNum(lnChannels), NULL);
  drawLnTile(252, 132, "NODES", prettyNum(lnNodes), NULL);
  drawLnTile(24, 212, "CAPACITE MOYENNE", prettyNum(lnAvgCap), "sats");
  drawLnTile(252, 212, "FEE RATE MOYEN", String(lnAvgFeePpm), "ppm");
}

// =====================================================================
//  PAGE 6 — IA LOCALE (moteur prédictif 100 % on-device)
//  • Prochain bloc : processus de Poisson (modèle EXACT du minage)
//  • Fees : régression linéaire + cycles hebdo APPRIS en continu (NVS)
//  AUCUN réseau de neurones embarqué : testé (MLP 6->8->3, 2 ans de
//  données), la prédiction de direction J+1 ne bat pas le hasard.
// =====================================================================
void drawPageAI() {
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(12, 36); gfx->print("IA LOCALE");
  gfx->setTextColor(C_DGREY);
  gfx->setCursor(96, 36); gfx->print("calcul on-device, rien ne sort");

  // ---------- panneau 1 : prochain bloc (Poisson) ----------
  gfx->fillRoundRect(10, 48, 224, 140, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(20, 56); gfx->print("PROCHAIN BLOC");
  long ts[6]; int n;
  portENTER_CRITICAL(&dataMux);
  memcpy(ts, blkTs, sizeof(ts)); n = blkTsN;
  portEXIT_CRITICAL(&dataMux);
  time_t nowT = time(nullptr);
  if (n >= 2 && nowT > 1000000000) {
    long lambda = constrain((ts[0] - ts[n - 1]) / (n - 1), 60, 3600);
    long el = max(0L, (long)nowT - ts[0]);
    long rem = lambda - el;
    char est[16];
    long ar = rem >= 0 ? rem : -rem;
    snprintf(est, sizeof(est), "%ld:%02ld", ar / 60, ar % 60);
    int ew = drawSmooth(20, 66, est, rem >= 0 ? C_GREEN : C_ORANGE, C_PANEL);
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(24 + ew, 92); gfx->print(rem >= 0 ? "min est." : "min retard");
    int prog = constrain((int)(el * 196 / lambda), 0, 196);
    gfx->fillRoundRect(20, 116, 196, 8, 4, C_BG);
    if (prog > 6) gfx->fillRoundRect(20, 116, prog, 8, 4, rem >= 0 ? C_ORANGE : C_RED);
    gfx->setTextColor(C_GREY);
    gfx->setCursor(20, 132); gfx->printf("rythme reel ~%ld min (%d blocs)", lambda / 60, n);
    // probabilités Poisson : P(au moins 1 bloc) dans 1 / 5 / 10 min
    gfx->setCursor(20, 150); gfx->print("P(bloc) :  1m    5m    10m");
    const int pm[3] = {60, 300, 600};
    for (int i = 0; i < 3; i++) {
      float pr = 1.0f - expf(-(float)pm[i] / lambda);
      int bw = (int)(44 * pr);
      gfx->fillRoundRect(58 + i * 48, 162, 44, 10, 4, C_BG);
      if (bw > 5) gfx->fillRoundRect(58 + i * 48, 162, bw, 10, 4, pr > 0.63f ? C_GREEN : C_ORANGE);
      gfx->setTextColor(C_DGREY);
      gfx->setCursor(60 + i * 48, 176); gfx->printf("%d%%", (int)(pr * 100));
      gfx->setTextColor(C_GREY);
    }
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(20, 100); gfx->print("chargement...");
  }

  // ---------- panneau 2 : fees (régression + cycles appris) ----------
  gfx->fillRoundRect(246, 48, 224, 140, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(256, 56); gfx->print("FEES : TENDANCE");
  int h[32]; int m;
  portENTER_CRITICAL(&dataMux);
  for (int i = 0; i < feeHistN; i++) h[i] = feeHist[(feeHistIdx - feeHistN + i + 64) % 32];
  m = feeHistN;
  portEXIT_CRITICAL(&dataMux);
  if (m >= 4) {
    int nn = min(m, 16);
    float xm = (nn - 1) / 2.0f, ym = 0;
    for (int i = 0; i < nn; i++) ym += h[m - nn + i];
    ym /= nn;
    float num = 0, den = 0;
    for (int i = 0; i < nn; i++) { num += (i - xm) * (h[m - nn + i] - ym); den += (i - xm) * (i - xm); }
    float slopeH = den > 0 ? num / den * 30.0f : 0;   // sat/vB par heure
    bool up = slopeH > 1.0f, dn = slopeH < -1.0f;
    drawArrow(262, 80, up ? true : false, up ? C_RED : (dn ? C_GREEN : C_GREY));
    char sl[16]; snprintf(sl, sizeof(sl), "%s%.1f", slopeH >= 0 ? "+" : "", slopeH);
    int sw2 = drawSmooth(282, 66, sl, up ? C_RED : (dn ? C_GREEN : C_WHITE), C_PANEL);
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(286 + sw2, 92); gfx->print("sat/vB/h");
    gfx->setTextColor(C_GREY);
    gfx->setCursor(256, 118);
    if (feeFast <= max(1, feeEco)) gfx->print("c'est calme : bon moment");
    else if (dn) gfx->print("en baisse : attends un peu");
    else if (up) gfx->print("en hausse : envoie vite");
    else gfx->print("situation stable");
    gfx->setTextColor(C_DGREY);
    gfx->setCursor(256, 134); gfx->printf("regression sur %d echantillons", nn);
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(256, 100); gfx->print("collecte...");
  }
  // cycles appris : créneau actuel vs norme + prochain creux
  struct tm t;
  if (getLocalTime(&t, 50)) {
    int b0 = constrain(t.tm_wday * 24 + t.tm_hour, 0, 167);
    float norm;
    portENTER_CRITICAL(&dataMux);
    norm = feeBkt[b0];
    portEXIT_CRITICAL(&dataMux);
    gfx->setTextColor(C_GREY);
    if (norm > 0 && feeFast > 0) {
      int ratio = (int)(feeFast * 100 / norm) - 100;
      gfx->setCursor(256, 150);
      gfx->setTextColor(abs(ratio) < 20 ? C_GREY : (ratio > 0 ? C_RED : C_GREEN));
      gfx->printf("creneau : %s%d%% vs norme", ratio >= 0 ? "+" : "", ratio);
      // prochain creux : 1er créneau à < 75% de la norme actuelle (scan 48 h)
      int found = -1;
      portENTER_CRITICAL(&dataMux);
      for (int k = 1; k <= 48; k++) {
        float v = feeBkt[(b0 + k) % 168];
        if (v > 0 && v < norm * 0.75f) { found = k; break; }
      }
      portEXIT_CRITICAL(&dataMux);
      gfx->setTextColor(C_GREY);
      gfx->setCursor(256, 164);
      if (found > 0) gfx->printf("creux probable dans ~%dh", found);
      else gfx->print("pas de creux sous 48 h");
    } else {
      gfx->setCursor(256, 150); gfx->print("cycles : apprentissage...");
    }
    gfx->setTextColor(C_DGREY);
    gfx->setCursor(256, 178); gfx->printf("appris ici : %ld echantillons", feeSamples);
  }

  // ---------- panneau 3 : cycles de fees sur 24 h (aujourd'hui) ----------
  gfx->fillRoundRect(10, 196, 460, 94, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(20, 204); gfx->print("CYCLES DE FEES — 24 h apprises");
  gfx->setTextColor(C_DGREY); gfx->setCursor(220, 204); gfx->print("(barre orange = maintenant)");
  if (getLocalTime(&t, 50) && feeSamples > 40) {
    float day[24]; float mx = 1;
    portENTER_CRITICAL(&dataMux);
    for (int i = 0; i < 24; i++) { day[i] = feeBkt[t.tm_wday * 24 + i]; if (day[i] > mx) mx = day[i]; }
    portEXIT_CRITICAL(&dataMux);
    if (mx > 1) {
      for (int i = 0; i < 24; i++) {
        int bh = day[i] > 0 ? (int)(40 * day[i] / mx) : 2;
        gfx->fillRect(22 + i * 19, 272 - bh, 14, bh, i == t.tm_hour ? C_ORANGE : C_DGREY);
      }
      gfx->drawFastHLine(22, 272, 24 * 19 - 5, C_GREY);
      gfx->setTextColor(C_DGREY); gfx->setTextSize(1);
      gfx->setCursor(22, 278); gfx->print("0h");
      gfx->setCursor(22 + 11 * 19, 278); gfx->print("11h");
      gfx->setCursor(22 + 22 * 19, 278); gfx->print("22h");
    } else {
      gfx->setTextColor(C_GREY); gfx->setCursor(20, 240); gfx->print("pas encore de donnees pour ce jour");
    }
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(20, 240);
    gfx->printf("apprentissage en cours (%ld echantillons)...", feeSamples);
  }
}

// =====================================================================
//  PAGE 7 — SIGNAUX (indicateurs techniques, pas des promesses)
//  • Divergence tendance 1D vs 1S (cache 7J), force faible/moyenne/forte
//  • Squeeze Bollinger 30J · score technique · anomalies z-score + vol 2σ
// =====================================================================
void drawPageSIG() {
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(12, 36); gfx->print("SIGNAUX");
  gfx->setTextColor(C_DGREY);
  gfx->setCursor(86, 36); gfx->print("indicateurs, pas des promesses");

  // snapshots caches klines
  float c7[MAX_PTS]; int n7;
  portENTER_CRITICAL(&dataMux);
  n7 = nPtsC[curCur][TF_7J];
  if (n7 > 0) memcpy(c7, closesC[curCur][TF_7J], n7 * sizeof(float));
  portEXIT_CRITICAL(&dataMux);
  float c30[MAX_PTS]; int n30;
  portENTER_CRITICAL(&dataMux);
  n30 = nPtsC[curCur][TF_30J];
  if (n30 > 0) memcpy(c30, closesC[curCur][TF_30J], n30 * sizeof(float));
  portEXIT_CRITICAL(&dataMux);

  // ---------- calculs communs 7J (rendements ~2h) ----------
  float sd = 0, t1 = 0, t7 = 0, div = 0, last2h = 0;
  bool ok7 = n7 >= 40;
  if (ok7) {
    int nr = n7 - 1;
    float m = 0;
    for (int i = 1; i < n7; i++) m += c7[i] / c7[i - 1] - 1.0f;
    m /= nr;
    for (int i = 1; i < n7; i++) { float d2 = c7[i] / c7[i - 1] - 1.0f - m; sd += d2 * d2; }
    sd = sqrtf(sd / nr);
    if (sd < 1e-6f) sd = 1e-6f;
    int k1 = min(12, n7 - 1);
    t1 = (c7[n7 - 1] / c7[n7 - 1 - k1] - 1.0f) / (sd * sqrtf((float)k1));
    t7 = (c7[n7 - 1] / c7[0] - 1.0f) / (sd * sqrtf((float)nr));
    div = t1 - t7;
    last2h = c7[n7 - 1] / c7[n7 - 2] - 1.0f;
  }
  bool volAlert = ok7 && fabsf(last2h) > 2.0f * sd;

  // ---------- panneau 1 : divergence tendance 1D vs 1S ----------
  gfx->fillRoundRect(10, 48, 296, 116, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(20, 56); gfx->print("TENDANCE 1D vs 1S");
  if (ok7) {
    int dir = div > 0.3f ? 1 : div < -0.3f ? -1 : 0;
    int force = fabsf(div) > 1.6f ? 3 : fabsf(div) > 0.8f ? 2 : 1;
    uint16_t dc = dir > 0 ? C_GREEN : dir < 0 ? C_RED : C_GREY;
    drawArrow(28, 78, dir >= 0, dc);
    gfx->setTextSize(2); gfx->setTextColor(dc);
    gfx->setCursor(50, 72);
    gfx->print(dir > 0 ? "HAUSSIER" : dir < 0 ? "BAISSIER" : "NEUTRE");
    // force : 3 barres
    for (int i = 0; i < 3; i++)
      gfx->fillRoundRect(170 + i * 26, 86 - i * 8, 20, 8 + i * 8, 3, i < force ? dc : C_BG);
    gfx->setTextSize(1); gfx->setTextColor(C_GREY);
    gfx->setCursor(20, 100);
    gfx->print(force == 3 ? "signal FORT" : force == 2 ? "signal moyen" : "signal faible");
    gfx->setCursor(20, 118); gfx->printf("div : %s%.2f ecarts", div >= 0 ? "+" : "", div);
    gfx->setCursor(20, 134); gfx->printf("1D %s%.1f%%  1S %s%.1f%%",
      t1 >= 0 ? "+" : "", t1 * sd * 3.46f * 100, t7 >= 0 ? "+" : "", t7 * sd * 9.1f * 100);
    gfx->setTextColor(C_DGREY);
    gfx->setCursor(20, 150); gfx->print("divergence normalisee (cache 7J)");
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(20, 100); gfx->print("chargement 7J...");
  }

  // ---------- panneau 2 : squeeze Bollinger ----------
  gfx->fillRoundRect(318, 48, 152, 116, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(328, 56); gfx->print("SQUEEZE 30J");
  float d[31]; int nd = 0;
  for (int i = 0; i < n30 && nd < 31; i += 3) d[nd++] = c30[i];
  if (nd >= 21) {
    auto bwOf = [&](int end) {
      float ma = 0; for (int i = end - 19; i <= end; i++) ma += d[i]; ma /= 20;
      float s2 = 0; for (int i = end - 19; i <= end; i++) s2 += (d[i] - ma) * (d[i] - ma);
      return 4.0f * sqrtf(s2 / 20) / ma * 100.0f;
    };
    float bw = bwOf(nd - 1);
    float bwAvg = bw; int cnt = 1;
    for (int e = nd - 6; e >= 20; e -= 5) { bwAvg += bwOf(e); cnt++; }
    bwAvg /= cnt;
    bool sq = bw < 0.6f * bwAvg;
    char bws[12]; snprintf(bws, sizeof(bws), "%.1f", bw);
    drawSmooth(328, 70, bws, sq ? C_ORANGE : C_WHITE, C_PANEL);
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(332 + smoothWidth(bws), 96); gfx->print("% bw");
    gfx->fillRoundRect(328, 118, 130, 22, 6, sq ? C_ORANGE_D : C_PANEL);
    gfx->setTextColor(sq ? C_ORANGE : C_GREY);
    gfx->setCursor(338, 125); gfx->print(sq ? "COMPRESSION" : "pas de squeeze");
    gfx->setTextColor(C_DGREY);
    gfx->setCursor(328, 148); gfx->printf("ref %.1f%%", bwAvg);
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(328, 100); gfx->print("chargement...");
  }

  // ---------- panneau 3 : indicateur technique ----------
  gfx->fillRoundRect(10, 172, 460, 64, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(20, 180); gfx->print("INDICATEUR TECHNIQUE");
  if (nd >= 16) {
    float ru = 0, rd = 0;
    for (int i = nd - 14; i < nd; i++) {
      float diff = d[i] - d[i - 1];
      if (diff > 0) ru += diff; else rd -= diff;
    }
    float rsi = (rd == 0) ? 100.0f : 100.0f - 100.0f / (1.0f + ru / rd);
    float mom = d[nd - 1] / d[max(0, nd - 8)] - 1.0f;
    float mn = d[0], mx = d[0];
    for (int i = 1; i < nd; i++) { if (d[i] < mn) mn = d[i]; if (d[i] > mx) mx = d[i]; }
    float rangePos = (mx > mn) ? (d[nd - 1] - mn) / (mx - mn) : 0.5f;
    int score = constrain((int)(0.5f * rsi + 0.3f * (50 + mom * 400) + 0.2f * rangePos * 100), 0, 100);
    gfx->fillRoundRect(20, 198, 260, 12, 6, C_BG);
    uint16_t sc = score > 66 ? C_GREEN : score > 33 ? C_YELLOW : C_RED;
    gfx->fillRoundRect(20, 198, (int)(260 * score / 100.0f), 12, 6, sc);
    char scs[8]; snprintf(scs, sizeof(scs), "%d", score);
    drawSmooth(300, 184, scs, sc, C_PANEL);
    gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
    gfx->setCursor(300, 226);
    gfx->print(score > 66 ? "plutot surachete" : score > 33 ? "neutre" : "plutot survendu");
    gfx->setTextColor(C_GREY);
    gfx->setCursor(20, 216); gfx->printf("RSI14 %.0f  mom7j %s%.1f%%  pos30j %.0f%%",
      rsi, mom >= 0 ? "+" : "", mom * 100, rangePos * 100);
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(20, 200); gfx->print("chargement 30J...");
  }

  // ---------- panneau 4 : anomalies ----------
  gfx->fillRoundRect(10, 244, 460, 46, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(20, 252); gfx->print("ANOMALIES");
  bool anyA = false;
  int ay = 268;
  if (fabsf(anomMemZ) > 2.5f) {
    anyA = true;
    gfx->setTextColor(C_ORANGE); gfx->setCursor(110, ay - 16);
    gfx->printf("MEMPOOL x%.1f (z=%.1f)", mempoolCount / max(1.0f, anomMemAvg), anomMemZ);
  }
  if (fabsf(anomFeeZ) > 2.5f) {
    anyA = true;
    gfx->setTextColor(C_ORANGE); gfx->setCursor(110, ay);
    gfx->printf("FEES anormaux (z=%.1f)", anomFeeZ);
  }
  if (volAlert) {
    anyA = true;
    gfx->setTextColor(C_YELLOW); gfx->setCursor(280, ay - 16);
    gfx->printf("VOL %s%.1f%% > 2s", last2h >= 0 ? "+" : "", last2h * 100);
  }
  if (!anyA) {
    gfx->setTextColor(C_GREEN); gfx->setCursor(110, 260);
    gfx->print("aucune - tout est dans la norme");
  }
}

// =====================================================================
//  PAGE 8 — BTC DOOM (mini raycaster façon Wolfenstein 3D)
//  Drag = tourner/avancer · bouton FIRE = tirer · ✕ = quitter
//  Démons billboard avec occlusion z-buffer, chassent le joueur.
// =====================================================================
#define GAME_EX (SCR_W - 40)          // bouton quitter
#define GAME_EY 32
#define GAME_EW 32
#define GAME_EH 26
#define FIRE_X  356                   // bouton FIRE
#define FIRE_Y  216
#define FIRE_W  112
#define FIRE_H  76

#define DM_W 16
#define DM_H 16
const uint8_t dmMap[DM_H][DM_W] = {
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1},
  {1,0,1,1,0,1,1,0,1,0,1,1,1,1,0,1},
  {1,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1},
  {1,0,1,0,1,0,1,1,1,0,1,0,1,1,0,1},
  {1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1},
  {1,1,1,0,1,1,1,0,1,1,1,0,1,0,1,1},
  {1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,1},
  {1,0,1,1,1,0,1,1,1,0,1,1,1,1,0,1},
  {1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,1},
  {1,0,1,0,1,1,1,0,1,0,1,1,0,1,0,1},
  {1,0,1,0,0,0,0,0,0,0,1,0,0,1,0,1},
  {1,0,1,1,1,1,1,0,1,0,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1},
  {1,0,1,0,1,1,0,0,0,0,1,1,1,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};

#define DOOM_RAYS 120
#define DOOM_COLW (SCR_W / DOOM_RAYS)  // colonnes de 4 px
#define DOOM_TOP  28                   // sous le header
#define DOOM_BOT  (SCR_H - 28)         // au-dessus de la barre d'onglets
#define DOOM_FOV  (PI / 3.0f)          // 60°
#define DM_ENEMIES 6

// ---- DOOM v2 : ennemis Bitcoin personnalisés ----
enum { ET_SAYLOR = 0, ET_TRUMP, ET_LAGARDE };
struct DmEnemy { float x, y; uint8_t st; unsigned long t0;
                 uint8_t type; int hp; unsigned long hitMs, shootMs; };  // st: 0=mort 1=vif 2=agonise
DmEnemy  dmE[DM_ENEMIES];
float    dmX = 1.5f, dmY = 1.5f, dmA = 0.0f;
float    zbuf[DOOM_RAYS];
bool     doomReset = true;
int      doomScore = 0;
uint8_t  dmWave = 1;
unsigned long doomShotMs = 0, doomHurtMs = 0, doomLastMs = 0;
uint16_t dmShade[2][4];                // murs [côté][niveau]
uint16_t dmShadeE[4];                  // démons [niveau]

const int   DM_HP[3]    = {3, 1, 2};                        // Saylor tank, Trump fragile, Lagarde moyen
const float DM_SPD[3]   = {0.30f, 0.75f, 0.40f};            // vitesse (cell/s)
const int   DM_SCORE[3] = {300, 100, 200};                  // score au kill
const char* DM_NAME[3]  = {"SAYLOR", "TRUMP", "LAGARDE"};
const char* DM_TTS[3]   = {"Saylor", "Trump", "Lagarde"};
#define C_CYAN 0x07FF
#define DOOM_VOICE_TAUNTS 1                                  // taunt vocal au kill (TTS)
char dmPopup[40] = ""; unsigned long dmPopupMs = 0;          // popup kill / vague

// projectiles de Lagarde ("hausses des taux" esquivables)
struct DmShot { float x, y, vx, vy; bool on; unsigned long t0; };
#define DM_NSHOTS 3
DmShot dmShot[DM_NSHOTS];

// affiches Bitcoin sur les murs (billboards z-bufferés)
struct DmPoster { float x, y; const char *txt; };
const DmPoster DM_POSTERS[] = {
  {2.5f, 1.15f, "HODL"},
  {4.5f, 1.15f, "STACK SATS"},
  {6.5f, 1.15f, "BUY THE DIP"},
  {10.5f, 1.15f, "FIX THE MONEY"},
  {12.5f, 1.15f, "NOT YOUR KEYS"},
  {14.5f, 1.15f, "SAYLOR WAS RIGHT"},
};
#define DM_NPOSTERS 6

uint16_t shade565(uint16_t c, int pct) {
  uint16_t r = ((c >> 11) & 0x1F) * pct / 100;
  uint16_t g = ((c >> 5) & 0x3F) * pct / 100;
  uint16_t b = (c & 0x1F) * pct / 100;
  return (r << 11) | (g << 5) | b;
}

void doomInitShades() {
  const int lvl[4] = {100, 76, 56, 38};
  for (int i = 0; i < 4; i++) {
    dmShade[0][i] = shade565(C_ORANGE, lvl[i]);
    dmShade[1][i] = shade565(C_ORANGE, lvl[i] * 2 / 3);  // faces ombrées
    dmShadeE[i]   = shade565(C_RED, lvl[i]);
  }
}

void doomSpawn(int i) {
  for (int t = 0; t < 60; t++) {
    int x = random(1, DM_W - 1), y = random(1, DM_H - 1);
    if (dmMap[y][x]) continue;
    float ddx = x + 0.5f - dmX, ddy = y + 0.5f - dmY;
    if (ddx * ddx + ddy * ddy > 16) {
      dmE[i].x = x + 0.5f; dmE[i].y = y + 0.5f; dmE[i].st = 1;
      dmE[i].hp = DM_HP[dmE[i].type];
      dmE[i].shootMs = millis() + random(1000, 3000);
      return;
    }
  }
  dmE[i].st = 0;                       // pas de place : reste mort
}

float normAng(float a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }

// affiches Bitcoin : billboards z-bufferés, le texte grandit en approchant
void doomDrawPoster(int pi) {
  const DmPoster &p = DM_POSTERS[pi];
  float dx = p.x - dmX, dy = p.y - dmY;
  float dist = hypotf(dx, dy);
  float ang = normAng(atan2f(dy, dx) - dmA);
  if (fabsf(ang) > DOOM_FOV / 2 + 0.25f) return;
  float perp = dist * cosf(ang);
  if (perp < 0.15f) return;
  int rh = DOOM_BOT - DOOM_TOP;
  int sh = (int)(rh * 0.42f / perp);
  if (sh < 8) return;
  int sw = sh * 2;
  int sx = (int)((ang + DOOM_FOV / 2) / DOOM_FOV * SCR_W);
  int yb = DOOM_TOP + (rh + (int)(rh / perp)) / 2;
  int y0 = yb - sh - sh / 3;                       // accrochée à hauteur de mur
  for (int x = max(0, sx - sw / 2); x <= min(SCR_W - 1, sx + sw / 2); x++) {
    if (perp >= zbuf[x / DOOM_COLW]) continue;
    bool bd = (x <= sx - sw / 2 + 1 || x >= sx + sw / 2 - 1);
    gfx->drawFastVLine(x, y0, sh, bd ? C_ORANGE : C_PANEL);
  }
  if (sh > 30 && perp < zbuf[constrain(sx / DOOM_COLW, 0, DOOM_RAYS - 1)]) {
    int ts = sh > 60 ? 2 : 1;
    gfx->setTextSize(ts); gfx->setTextColor(C_ORANGE);
    gfx->setCursor(sx - (int)strlen(p.txt) * 3 * ts, y0 + (sh - 8 * ts) / 2);
    gfx->print(p.txt);
  }
}

void drawPageDoom() {
  unsigned long now = millis();
  if (doomReset) {
    doomReset = false; doomScore = 0; dmWave = 1; dmPopup[0] = 0;
    dmX = 1.5f; dmY = 1.5f; dmA = 0.0f;
    doomInitShades();
    for (int i = 0; i < DM_NSHOTS; i++) dmShot[i].on = false;
    for (int i = 0; i < DM_ENEMIES; i++) { dmE[i].type = i % 3; doomSpawn(i); }
  }
  float dt = doomLastMs ? min(0.1f, (now - doomLastMs) / 1000.0f) : 0.016f;
  doomLastMs = now;
  int rh = DOOM_BOT - DOOM_TOP;

  // ---- contrôle : deux joysticks MULTI-TOUCH (doigts indépendants) ----
  // doigt zone GAUCHE (x < 240) : déplacement — vertical = avant/arrière,
  //                               horizontal = STRAFE gauche/droite
  // doigt zone DROITE (x >= 240) : regard — horizontal = tourner
  // (FIRE et ✕ conservés ; base flottante à la pose de chaque doigt)
  static struct Finger { bool on; int bx, by, kx, ky; } fL = {false}, fR = {false};
  static bool fireHeld = false;
  uint16_t txs[2], tys[2]; uint8_t tev[2];
  int nt = readTouchMulti(txs, tys, tev, 2);
  bool seenL = false, seenR = false, fireTap = false;
  for (int i = 0; i < nt; i++) {
    if (tev[i] == 1) continue;                          // doigt relevé
    int x = txs[i], y = tys[i];
    bool onFire = (x >= FIRE_X && x <= FIRE_X + FIRE_W && y >= FIRE_Y && y <= FIRE_Y + FIRE_H);
    if (onFire && !fR.on) { fireTap = true; continue; } // doigt posé sur FIRE : pas un joystick
    if (x < SCR_W / 2) {
      if (!fL.on) { fL.on = true; fL.bx = x; fL.by = y; }
      fL.kx = x; fL.ky = y; seenL = true;
    } else {
      if (!fR.on) { fR.on = true; fR.bx = x; fR.by = y; }
      fR.kx = x; fR.ky = y; seenR = true;
    }
  }
  if (!seenL) fL.on = false;
  if (!seenR) fR.on = false;
  // déplacement : vitesse ∝ déport du joystick gauche (chaque frame)
  if (fL.on) {
    float fwd = (fL.by - fL.ky) * 1.5f / 48.0f * dt;        // max ~1.5 cell/s
    float lat = (fL.kx - fL.bx) * 1.5f / 48.0f * dt;        // strafe (perpendiculaire)
    if (fwd > 0.2f) fwd = 0.2f; if (fwd < -0.2f) fwd = -0.2f;
    if (lat > 0.2f) lat = 0.2f; if (lat < -0.2f) lat = -0.2f;
    float nx = dmX + cosf(dmA) * fwd - sinf(dmA) * lat;
    float ny = dmY + sinf(dmA) * fwd + cosf(dmA) * lat;
    if (!dmMap[(int)dmY][(int)nx]) dmX = nx;
    if (!dmMap[(int)ny][(int)dmX]) dmY = ny;
  }
  // regard : vitesse de rotation ∝ déport du joystick droit
  if (fR.on) dmA += (fR.kx - fR.bx) * 2.5f / 48.0f * dt;    // max ~2.5 rad/s

  if (fireTap && !fireHeld) {            // TIR (hitscan)
    fireHeld = true; doomShotMs = now;
    beep(160, 60, 45);
    for (int i = 0; i < DM_ENEMIES; i++) {
      if (dmE[i].st != 1) continue;
      float dx = dmE[i].x - dmX, dy = dmE[i].y - dmY;
      float dist = hypotf(dx, dy);
      if (dist > 12) continue;
      float ang = normAng(atan2f(dy, dx) - dmA);
      float perp = dist * cosf(ang);
      int sx = (int)((ang + DOOM_FOV / 2) / DOOM_FOV * SCR_W);
      int sh = (int)(rh * 0.75f / max(0.1f, perp));
      if (abs(sx - SCR_W / 2) < sh / 3 && perp < zbuf[constrain(sx / DOOM_COLW, 0, DOOM_RAYS - 1)]) {
        dmE[i].hitMs = now;
        if (--dmE[i].hp > 0) {                          // touché mais vivant
          beep(400, 40, 30);
        } else {                                        // KILL
          dmE[i].st = 2; dmE[i].t0 = now; doomScore += DM_SCORE[dmE[i].type];
          snprintf(dmPopup, sizeof(dmPopup), "%s DOWN +%d", DM_NAME[dmE[i].type], DM_SCORE[dmE[i].type]);
          dmPopupMs = now;
          if (dmE[i].type == ET_SAYLOR) { beep(300, 60, 35); beep(450, 60, 35); beep(600, 80, 35); }
          else if (dmE[i].type == ET_TRUMP) { beep(600, 40, 35); beep(350, 70, 35); }
          else { beep(700, 50, 35); beep(900, 70, 35); }
#if DOOM_VOICE_TAUNTS
          speak(String(DM_TTS[dmE[i].type]) + " down.", SND_UI);
#endif
        }
      }
    }
  }
  if (!fireTap) fireHeld = false;

  // ---- ennemis : poursuite + attaque + tirs + vagues ----
  float waveBoost = 1.0f + 0.15f * (dmWave - 1); if (waveBoost > 1.6f) waveBoost = 1.6f;
  bool anyAlive = false;
  for (int i = 0; i < DM_ENEMIES; i++) {
    if (dmE[i].st == 1) {
      anyAlive = true;
      float dx = dmX - dmE[i].x, dy = dmY - dmE[i].y;
      float d = hypotf(dx, dy);
      if (d > 0.5f) {
        float sp = DM_SPD[dmE[i].type] * waveBoost * dt;
        float nx = dmE[i].x + dx / d * sp, ny = dmE[i].y + dy / d * sp;
        if (!dmMap[(int)dmE[i].y][(int)nx]) dmE[i].x = nx;
        if (!dmMap[(int)ny][(int)dmE[i].x]) dmE[i].y = ny;
      } else if (now - doomHurtMs > 1000) {   // touché !
        doomHurtMs = now; doomScore = max(0, doomScore - 50);
        beep(120, 180, 50);
        doomSpawn(i);                          // l'ennemi se téléporte loin
      }
      // LAGARDE : tire des "hausses des taux" à distance (ligne de vue requise)
      if (dmE[i].type == ET_LAGARDE && d > 1.2f && d < 7.0f && now - dmE[i].shootMs > 3500) {
        bool los = true;
        for (float k = 0.5f; k < d; k += 0.5f)
          if (dmMap[(int)(dmE[i].y + dy / d * k)][(int)(dmE[i].x + dx / d * k)]) { los = false; break; }
        if (los) {
          dmE[i].shootMs = now;
          for (int s = 0; s < DM_NSHOTS; s++) if (!dmShot[s].on) {
            dmShot[s].on = true; dmShot[s].t0 = now;
            dmShot[s].x = dmE[i].x; dmShot[s].y = dmE[i].y;
            dmShot[s].vx = dx / d * 2.2f; dmShot[s].vy = dy / d * 2.2f;
            break;
          }
          beep(900, 30, 25);
        }
      }
    } else if (dmE[i].st == 2 && now - dmE[i].t0 > 350) {
      dmE[i].st = 0; dmE[i].t0 = now;          // mort -> respawn plus tard
    } else if (dmE[i].st == 0 && now - dmE[i].t0 > 6000) {
      doomSpawn(i);
    }
  }
  // vague suivante : les 6 tués -> vitesse +15 %
  static bool wavePending = false;
  if (!anyAlive && !wavePending) {
    wavePending = true; dmWave++;
    snprintf(dmPopup, sizeof(dmPopup), "WAVE %d !", dmWave); dmPopupMs = now;
    for (int i = 0; i < DM_ENEMIES; i++) { dmE[i].st = 0; dmE[i].t0 = now; }
  } else if (anyAlive) wavePending = false;

  // ---- projectiles de Lagarde ("hausses des taux") ----
  for (int s = 0; s < DM_NSHOTS; s++) {
    if (!dmShot[s].on) continue;
    dmShot[s].x += dmShot[s].vx * dt; dmShot[s].y += dmShot[s].vy * dt;
    if (dmMap[(int)dmShot[s].y][(int)dmShot[s].x] || now - dmShot[s].t0 > 4000) { dmShot[s].on = false; continue; }
    float ddx = dmShot[s].x - dmX, ddy = dmShot[s].y - dmY;
    if (ddx * ddx + ddy * ddy < 0.12f) {       // le joueur est touché
      dmShot[s].on = false;
      if (now - doomHurtMs > 800) {
        doomHurtMs = now; doomScore = max(0, doomScore - 25);
        beep(100, 200, 50);
      }
    }
  }

  // ---- ciel + sol ----
  gfx->fillRect(0, DOOM_TOP, SCR_W, rh / 2, C_BG);
  gfx->fillRect(0, DOOM_TOP + rh / 2, SCR_W, rh - rh / 2, C_PANEL);

  // ---- raycasting DDA (+ z-buffer pour les sprites) ----
  for (int i = 0; i < DOOM_RAYS; i++) {
    float ra = dmA - DOOM_FOV / 2 + DOOM_FOV * i / DOOM_RAYS;
    float rdx = cosf(ra), rdy = sinf(ra);
    int mx = (int)dmX, my = (int)dmY;
    float ddx = fabsf(1.0f / (rdx == 0 ? 1e-6f : rdx));
    float ddy = fabsf(1.0f / (rdy == 0 ? 1e-6f : rdy));
    int stx, sty; float sdx, sdy;
    if (rdx < 0) { stx = -1; sdx = (dmX - mx) * ddx; } else { stx = 1; sdx = (mx + 1 - dmX) * ddx; }
    if (rdy < 0) { sty = -1; sdy = (dmY - my) * ddy; } else { sty = 1; sdy = (my + 1 - dmY) * ddy; }
    int side = 0;
    for (int it = 0; it < 32; it++) {
      if (sdx < sdy) { sdx += ddx; mx += stx; side = 0; }
      else           { sdy += ddy; my += sty; side = 1; }
      if (mx < 0 || my < 0 || mx >= DM_W || my >= DM_H || dmMap[my][mx]) break;
    }
    float dist = (side == 0 ? sdx - ddx : sdy - ddy) * cosf(ra - dmA);  // anti-fisheye
    if (dist < 0.05f) dist = 0.05f;
    zbuf[i] = dist;
    int h = (int)(rh / dist);
    int lvl = dist > 5 ? 3 : dist > 3 ? 2 : dist > 1.5f ? 1 : 0;
    int y0 = DOOM_TOP + (rh - h) / 2;
    gfx->fillRect(i * DOOM_COLW, max(y0, DOOM_TOP), DOOM_COLW, min(h, rh), dmShade[side][lvl]);
  }

  // ---- affiches Bitcoin sur les murs ----
  for (int i = 0; i < DM_NPOSTERS; i++) doomDrawPoster(i);

  // ---- démons (billboards, tri peintre : loin -> près) ----
  int order[DM_ENEMIES];
  float pd[DM_ENEMIES];
  for (int i = 0; i < DM_ENEMIES; i++) {
    order[i] = i;
    float dx = dmE[i].x - dmX, dy = dmE[i].y - dmY;
    pd[i] = dmE[i].st ? dx * dx + dy * dy : -1;
  }
  for (int i = 0; i < DM_ENEMIES - 1; i++)
    for (int j = i + 1; j < DM_ENEMIES; j++)
      if (pd[order[j]] > pd[order[i]]) { int t2 = order[i]; order[i] = order[j]; order[j] = t2; }
  for (int oi = 0; oi < DM_ENEMIES; oi++) {
    int i = order[oi];
    if (!dmE[i].st) continue;
    float dx = dmE[i].x - dmX, dy = dmE[i].y - dmY;
    float dist = hypotf(dx, dy);
    float ang = normAng(atan2f(dy, dx) - dmA);
    if (fabsf(ang) > DOOM_FOV / 2 + 0.25f) continue;
    float perp = dist * cosf(ang);
    if (perp < 0.15f) continue;
    int sh = (int)(rh * 0.75f / perp);
    if (dmE[i].st == 2) sh = sh * (int)max(0L, 350L - (long)(now - dmE[i].t0)) / 350;  // agonie : rétrécit
    if (sh < 4) continue;
    int sx = (int)((ang + DOOM_FOV / 2) / DOOM_FOV * SCR_W);
    int yb = DOOM_TOP + (rh + (int)(rh / perp)) / 2;      // pieds au sol
    int cyE = yb - sh / 2;
    int lvl = perp > 5 ? 3 : perp > 3 ? 2 : perp > 1.5f ? 1 : 0;
    int rw = max(2, sh / 3);
    // corps : couleur par type (Saylor sombre, Trump orange, Lagarde bleu),
    // flash blanc quand touché
    uint16_t bodyC = (now - dmE[i].hitMs < 120) ? C_WHITE
                     : dmE[i].type == ET_TRUMP ? C_ORANGE
                     : dmE[i].type == ET_LAGARDE ? C_BLUE : C_DGREY;
    (void)lvl;
    for (int x = max(0, sx - rw); x <= min(SCR_W - 1, sx + rw); x++) {
      if (perp >= zbuf[x / DOOM_COLW]) continue;          // occlus par le mur
      float u = (float)(x - sx) / rw;
      int hh = (int)(sh / 2 * sqrtf(max(0.0f, 1.0f - u * u)));
      if (hh > 0) gfx->drawFastVLine(x, cyE - hh, hh * 2, bodyC);
    }
    if (perp < 4 && dmE[i].st == 1) {                     // visage quand proche
      int es = max(2, sh / 20);
      if (dmE[i].type == ET_SAYLOR) {
        // yeux LASER cyan (l'avatar légendaire de Saylor) + mini-BTC
        gfx->fillRect(sx - sh / 5, cyE - sh / 6, sh / 5, es, C_CYAN);
        gfx->fillRect(sx + sh / 10, cyE - sh / 6, sh / 5, es, C_CYAN);
        gfx->fillCircle(sx, cyE + sh / 5, es, C_ORANGE);
      } else {
        gfx->fillRect(sx - sh / 8, cyE - sh / 6, es, es, C_WHITE);
        gfx->fillRect(sx + sh / 8 - es, cyE - sh / 6, es, es, C_WHITE);
        if (dmE[i].type == ET_TRUMP) {
          // houppe blonde + cravate rouge
          gfx->fillRect(sx - sh / 6, cyE - sh / 2 + sh / 12, sh / 3, sh / 10, C_YELLOW);
          gfx->fillTriangle(sx - es, cyE + sh / 4, sx + es, cyE + sh / 4, sx, cyE + sh / 2, C_RED);
        } else if (dmE[i].type == ET_LAGARDE) {
          // col blanc de la régulatrice
          gfx->fillRect(sx - es, cyE + sh / 8, es * 2, es, C_WHITE);
        }
      }
    }
    // nom au-dessus de l'ennemi quand proche
    if (perp < 3.5f && dmE[i].st == 1) {
      gfx->setTextSize(1); gfx->setTextColor(C_GREY);
      gfx->setCursor(sx - (int)strlen(DM_NAME[dmE[i].type]) * 3, cyE - sh / 2 - 9);
      gfx->print(DM_NAME[dmE[i].type]);
    }
  }

  // ---- projectiles de Lagarde (billboards jaunes) ----
  for (int s = 0; s < DM_NSHOTS; s++) {
    if (!dmShot[s].on) continue;
    float dx = dmShot[s].x - dmX, dy = dmShot[s].y - dmY;
    float dist = hypotf(dx, dy);
    float ang = normAng(atan2f(dy, dx) - dmA);
    if (fabsf(ang) > DOOM_FOV / 2 + 0.25f) continue;
    float perp = dist * cosf(ang);
    if (perp < 0.15f) continue;
    int sh = (int)(rh * 0.25f / perp);
    if (sh < 4) sh = 4;
    int sx = (int)((ang + DOOM_FOV / 2) / DOOM_FOV * SCR_W);
    int yb = DOOM_TOP + (rh + (int)(rh / perp)) / 2;
    int cyE = yb - sh;
    int rw = max(2, sh / 3);
    for (int x = max(0, sx - rw); x <= min(SCR_W - 1, sx + rw); x++) {
      if (perp >= zbuf[x / DOOM_COLW]) continue;
      float u = (float)(x - sx) / rw;
      int hh = (int)(sh / 2 * sqrtf(max(0.0f, 1.0f - u * u)));
      if (hh > 0) gfx->drawFastVLine(x, cyE - hh, hh * 2, C_YELLOW);
    }
  }

  // ---- joysticks : repères fixes + base/knob de chaque doigt actif ----
  gfx->drawCircle(70, 252, 34, C_DGREY);                     // repère joystick gauche
  gfx->drawCircle(288, 252, 34, C_DGREY);                    // repère joystick regard
  for (int f = 0; f < 2; f++) {
    const Finger &fg = f ? fR : fL;
    if (!fg.on) continue;
    gfx->drawCircle(fg.bx, fg.by, 34, C_GREY);               // base flottante
    int kx = fg.kx, ky = fg.ky;                              // knob clampé à 34 px
    int ddx = fg.kx - fg.bx, ddy = fg.ky - fg.by;
    float dd = hypotf((float)ddx, (float)ddy);
    if (dd > 34) { kx = fg.bx + (int)(ddx * 34 / dd); ky = fg.by + (int)(ddy * 34 / dd); }
    gfx->fillCircle(kx, ky, 13, C_ORANGE);
  }

  // ---- arme + muzzle flash + viseur ----
  gfx->fillRect(SCR_W / 2 - 12, DOOM_BOT - 44, 24, 44, C_DGREY);
  gfx->fillRect(SCR_W / 2 - 5, DOOM_BOT - 58, 10, 20, C_GREY);
  if (now - doomShotMs < 90)
    gfx->fillTriangle(SCR_W / 2 - 12, DOOM_BOT - 58, SCR_W / 2 + 12, DOOM_BOT - 58, SCR_W / 2, DOOM_BOT - 78, C_YELLOW);
  gfx->drawFastHLine(SCR_W / 2 - 6, DOOM_TOP + rh / 2, 12, C_WHITE);
  gfx->drawFastVLine(SCR_W / 2, DOOM_TOP + rh / 2 - 6, 12, C_WHITE);

  // ---- HUD : minimap, score, FIRE, ✕, flash dégâts ----
  for (int y = 0; y < DM_H; y++)
    for (int x = 0; x < DM_W; x++)
      if (dmMap[y][x]) gfx->fillRect(8 + x * 3, DOOM_TOP + 4 + y * 3, 3, 3, C_DGREY);
  for (int i = 0; i < DM_ENEMIES; i++)
    if (dmE[i].st == 1) gfx->fillRect(8 + (int)(dmE[i].x * 3) - 1, DOOM_TOP + 4 + (int)(dmE[i].y * 3) - 1, 3, 3, C_RED);
  gfx->fillRect(8 + (int)(dmX * 3) - 1, DOOM_TOP + 4 + (int)(dmY * 3) - 1, 4, 4, C_ORANGE);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(64, DOOM_TOP + 6); gfx->print("SCORE");
  char sc[8]; snprintf(sc, sizeof(sc), "%d", doomScore);
  drawSmooth(64, DOOM_TOP + 14, sc, C_WHITE, C_BG);
  gfx->fillCircle(FIRE_X + FIRE_W / 2, FIRE_Y + FIRE_H / 2, 30, fireTap ? C_RED : C_RED_D);
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(FIRE_X + FIRE_W / 2 - 22, FIRE_Y + FIRE_H / 2 - 8); gfx->print("FIRE");
  // popup kill / vague
  if (dmPopup[0] && now - dmPopupMs < 1600) textCenter(dmPopup, 56, 2, C_ORANGE);
  gfx->drawRect(GAME_EX, GAME_EY, GAME_EW, GAME_EH, C_GREY);
  gfx->drawLine(GAME_EX + 8, GAME_EY + 6, GAME_EX + GAME_EW - 9, GAME_EY + GAME_EH - 7, C_RED);
  gfx->drawLine(GAME_EX + GAME_EW - 9, GAME_EY + 6, GAME_EX + 8, GAME_EY + GAME_EH - 7, C_RED);
  if (now - doomHurtMs < 500) {
    gfx->drawRect(2, DOOM_TOP + 2, SCR_W - 4, rh - 4, C_RED);
    textCenter("TOUCHE !", 100, 3, C_RED);
  }
}

// =====================================================================
//  ANIM NOUVEAU BLOC (flash classique — pages autres que Cube)
// =====================================================================
void drawBlockAnim() {
  unsigned long e = millis() - animStart;
  if (e > 2600) { animNewBlock = false; needRedraw = true; return; }
  int phase = (e / 260) % 2;
  gfx->fillScreen(phase ? C_ORANGE : C_BG);
  textCenter("NEW BLOCK", 110, 4, phase ? C_BG : C_ORANGE);
  textCenter(String(blockHeight), 170, 3, phase ? C_BG : C_ORANGE);
  char pool[32]; long btx; snapLastBlock(pool, sizeof(pool), &btx);
  textCenter(pool, 220, 2, phase ? C_BG : C_GREY);
  gfx->flush();
}

void drawCurrentPage() {
  gfx->fillScreen(C_BG);
  drawHeader();
  switch (page) {
    case PG_PRICE: drawPagePrice(); break;
    case PG_CHAIN: drawPageChain(); break;
    case PG_CUBE:  drawPageCube(); break;
    case PG_POOLS: drawPagePools(); break;
    case PG_LN:    drawPageLightning(); break;
    case PG_NODE:  drawPageNode(); break;
    case PG_AI:    drawPageAI(); break;
    case PG_SIG:   drawPageSIG(); break;
    case PG_DOOM:  drawPageDoom(); break;
  }
  drawFooter();
  gfx->flush();
}

// =====================================================================
//  SETUP — boot non bloquant : l'UI s'affiche tout de suite avec des
//  états "chargement...", REQ_ALL est poussé dans netTask qui fetchera.
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== BITCOIN BLOCK CLOCK V4 ===");

  ledcSetup(0, 5000, 10);
  ledcAttachPin(PIN_BL, 0);
  setBacklight(70);

  gfx->begin();
  gfx->fillScreen(C_BG);
  gfx->draw16bitRGBBitmapWithTranColor(SCR_W / 2 - 48, 90, (uint16_t*)BTC_LOGO_96, TRANSP, 96, 96);
  textCenter("BLOCK CLOCK", 210, 3, C_ORANGE);
  textCenter("demarrage...", 250, 2, C_GREY);
  gfx->flush();

  Wire.begin(TP_SDA, TP_SCL);
  Wire.setClock(400000);
  audioInit();
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

  loadConfig();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (cfg_ssid.length() > 0) WiFi.begin(cfg_ssid.c_str(), cfg_pass.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    if (millis() - t0 > 20000) startConfigPortal();
  }
  Serial.println("[WiFi] OK : " + WiFi.localIP().toString());
  if (MDNS.begin("blockclock")) MDNS.addService("http", "tcp", 80);

  // ---------- page web ----------
  server.on("/", HTTP_GET, []() {
    String s = "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    s += "<body style='background:#0d1117;color:#eee;font-family:sans-serif;max-width:420px;margin:30px auto;padding:0 16px'>";
    s += "<h1 style='color:#F7931A'>&#8383; Block Clock</h1>";
    s += "<p>BTC: " + prettyNum((long)btcPrice[curCur]) + " " + CUR_LABEL[curCur] + " (" + String(btcChg24, 2) + "%) &middot; Bloc " + String(blockHeight) + "</p>";
    s += "<h2 style='color:#F7931A'>Alertes prix (" + String(CUR_LABEL[curCur]) + ")</h2>";
    s += "<form method='POST' action='/alerts'>";
    s += "Au-dessus de : <input name='hi' type='number' step='any' value='" + String(alertHi, 0) + "' style='width:100%;padding:10px;margin:6px 0;background:#161b22;color:#eee;border:1px solid #444;border-radius:8px'>";
    s += "En-dessous de : <input name='lo' type='number' step='any' value='" + String(alertLo, 0) + "' style='width:100%;padding:10px;margin:6px 0;background:#161b22;color:#eee;border:1px solid #444;border-radius:8px'>";
    s += "<h2 style='color:#F7931A'>Noeud Umbrel</h2>";
    s += "IP : <input name='nodeip' value='" + cfg_nodeip + "' style='width:100%;padding:10px;margin:6px 0;background:#161b22;color:#eee;border:1px solid #444;border-radius:8px'>";
    s += "<button style='width:100%;padding:14px;background:#F7931A;border:0;border-radius:8px;font-weight:bold;margin-top:8px'>Enregistrer</button></form>";
    s += "<p style='margin-top:20px'><a style='color:#F7931A' href='/reset'>Oublier le WiFi</a></p></body>";
    server.send(200, "text/html", s);
  });
  // test vocal : /say?t=Hello world (anglais, via SAM)
  server.on("/say", HTTP_GET, []() {
    if (!server.hasArg("t")) { server.send(400, "text/plain", "missing t"); return; }
    speak(server.arg("t"), SND_UI);
    server.send(200, "text/plain", "ok");
  });
  server.on("/alerts", HTTP_POST, []() {
    alertHi = server.arg("hi").toFloat();
    alertLo = server.arg("lo").toFloat();
    if (server.hasArg("nodeip")) { cfg_nodeip = server.arg("nodeip"); cfg_nodeip.trim(); }
    saveConfig();
    latchHi = latchLo = false;
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.on("/reset", HTTP_GET, []() {
    prefs.begin("bc", false); prefs.clear(); prefs.end();
    server.send(200, "text/html", "WiFi oublie - redemarrage…");
    delay(800); ESP.restart();
  });
  server.begin();

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

  // ---------- tâches FreeRTOS ----------
  sndQ = xQueueCreate(8, sizeof(SndNote));
  sndTxtQ = xQueueCreate(4, sizeof(SndTxt));
  xTaskCreatePinnedToCore(sndTask, "snd", 20480, NULL, 2, NULL, 0);   // sons + TTS (TLS+MP3 = gros stack)
  xTaskCreatePinnedToCore(netTask, "net", 12288, NULL, 1, NULL, 0);   // tous les fetchs HTTP

  // boot non bloquant : tout fetcher en tâche de fond, UI immédiate
  requestFetch(REQ_ALL);
  blockDetectedMs = millis();

  drawCurrentPage();
  playStart();
}

// =====================================================================
//  LOOP — aucun appel réseau : tactile + events + redraw uniquement
// =====================================================================
void loop() {
  server.handleClient();
  unsigned long now = millis();
  updateBattery();          // batterie : lecture cache + détection de charge (5 s)
  // pendant la charge : forcer le redraw pour animer le clignotement de la jauge
  static unsigned long tBatBlink = 0;
  if (batCharging && !sleeping && millis() - tBatBlink > 800) {
    tBatBlink = millis();
    needRedraw = true;
  }

  // ---------- mode nuit + tick minute (horloge du header) ----------
  struct tm t;
  if (getLocalTime(&t, 50)) {
    bool night = (t.tm_hour >= 23 || t.tm_hour < 7);
    if (night != nightMode && !sleeping) {
      nightMode = night;
      setBacklight(nightMode ? 15 : 70);
      needRedraw = true;
    }
    static int lastMin = -1;
    if (t.tm_min != lastMin) { lastMin = t.tm_min; needRedraw = true; }
  }

  // ---------- tactile ----------
  static bool wasTouched = false, touchMoved = false;
  static unsigned long touchStart = 0;
  static uint16_t downX = 0, downY = 0;
  uint16_t tx, ty;
  bool touched = readTouch(tx, ty);
  // dernière position VALIDE du doigt (au relâchement tx/ty ne sont plus
  // mises à jour par readTouch — les utiliser = comportement indéfini)
  static uint16_t lastX = 0, lastY = 0;
  static unsigned long lastActionMs = 0;   // debounce : anti-rebond tactile
  static bool touchIgnored = false;
  if (touched && !wasTouched) {
    touchStart = now; wasTouched = true; downX = tx; downY = ty; touchMoved = false;
    touchIgnored = (now - lastActionMs < 250);   // appui fantôme après une action
  }
  gTouch = touched && !touchIgnored;
  if (touched) { gTX = tx; gTY = ty; lastX = tx; lastY = ty; }
  if (touched && wasTouched && !touchMoved) {
    // un appui qui bouge = un drag (BTC DOOM), pas un appui long
    if (abs((int)tx - (int)downX) > 40 || abs((int)ty - (int)downY) > 40) touchMoved = true;
  }
  if (!touched && wasTouched) {
    unsigned long dt = now - touchStart;
    wasTouched = false;
    if (touchIgnored) touchIgnored = false;
    else if (dt > 1200 && !touchMoved && page != PG_DOOM) {
      // appui long (immobile) : veille — désactivé sur DOOM (joysticks tenus)
      sleeping = !sleeping;
      setBacklight(sleeping ? 0 : (nightMode ? 15 : 70));
      if (!sleeping) needRedraw = true; else { gfx->fillScreen(C_BG); gfx->flush(); }
      beep(sleeping ? 500 : 900, 100);
      lastActionMs = now;
    } else if (!sleeping && dt <= 1200) {
      int dx = (int)lastX - (int)downX;
      int dy = (int)lastY - (int)downY;
      // icône HP (header) : tap = cycle volume 100 -> 60 -> 30 -> muet
      if (!touchMoved && downY < 26 && downX >= 340 && downX <= 372) {
        sndVolPct = (sndVolPct > 60) ? 60 : (sndVolPct > 30) ? 30 : (sndVolPct > 0) ? 0 : 100;
        saveConfig();
        needRedraw = true; lastActionMs = now;
        if (sndVolPct > 0) beep(1200, 60, 40);   // feedback au nouveau niveau
      }
      // barre d'onglets : tap sur une icône = accès direct à la page
      else if (!touchMoved && downY >= NAV_Y) {
        int tab = constrain((int)downX / TAB_W, 0, (int)PG_COUNT - 1);
        if (tab != page) {
          page = tab; lastActionMs = now;
          beep(1000, 50, 25);
          if (page == PG_POOLS) poolsReset = true;
          if (page == PG_DOOM) doomReset = true;
          if (page == PG_AI || page == PG_SIG) requestFetch(REQ_KL30);
          needRedraw = true;
        }
      }
      else if (page == PG_DOOM) {
        // page jeu : seul le ✕ compte — les drags sont des contrôles,
        // ils ne doivent ni changer de page ni déclencher le refresh
        if (downX >= GAME_EX && downX <= GAME_EX + GAME_EW && downY >= GAME_EY && downY <= GAME_EY + GAME_EH) {
          page = PG_PRICE; beep(900, 60, 30); needRedraw = true; lastActionMs = now;
        }
      }
      else if (abs(dx) > 50 && abs(dy) < 100) {
        // swipe horizontal : page suivante / précédente
        page = (dx < 0) ? (page + 1) % PG_COUNT : (page + PG_COUNT - 1) % PG_COUNT;
        lastActionMs = now;
        beep(1000, 50, 25);
        if (page == PG_POOLS) poolsReset = true;
        if (page == PG_DOOM) doomReset = true;
        if (page == PG_AI || page == PG_SIG) requestFetch(REQ_KL30);   // données 30J/7J
        needRedraw = true;
      }
      else if (page == PG_PRICE && downY >= 34 && downY <= 56 && downX >= GX) {
        // onglets timeframe : cache d'abord (instantané), fetch frais ensuite
        int tf = (downX - GX) / 72;
        if (tf >= 0 && tf < TF_COUNT && tf != curTf) {
          curTf = tf; beep(1500, 50, 30); lastActionMs = now;
          chartCacheLoad();
          requestFetch(REQ_KLINES); needRedraw = true;
        }
      }
      else if (page == PG_PRICE && downX >= 10 && downX <= 80 && downY >= 194 && downY <= 216) {
        // switch devise : cache d'abord, fetch frais ensuite
        curCur = (curCur + 1) % CUR_COUNT;
        beep(1500, 50, 30); lastActionMs = now;
        chartCacheLoad();
        requestFetch(REQ_PRICE | REQ_KLINES); needRedraw = true;
      }
      else if (page == PG_PRICE && downX >= GX && downX <= GX + GW && downY >= GY - 8 && downY <= GY + GH) {
        // curseur graphe
        if (nPts > 1) {
          int idx = (int)((long)(downX - GX - 4) * (nPts - 1) / (GW - 10));
          cursorIdx = constrain(idx, 0, nPts - 1);
          beep(1800, 40, 20); lastActionMs = now;
          needRedraw = true;
        }
      }
      else {
        // tap global : refresh complet à la demande (netTask répond vite)
        beep(1500, 50, 30); lastActionMs = now;
        requestFetch(REQ_ALL);
        needRedraw = true;
      }
    }
  }

  // ---------- événements net -> UI ----------
  if (evNewBlock) {
    evNewBlock = false;
    blockDetectedMs = now;
    if (!sleeping) {
      if (page == PG_CUBE) cubeBlockSeq();           // la chaîne emporte le cube
      else { animNewBlock = true; animStart = now; } // flash classique ailleurs
    }
#if SPEECH_BLOCKS
    // annonce vocale : "New block. <pool>." (filtrée la nuit / en veille)
    { char pool[32]; long btx; snapLastBlock(pool, sizeof(pool), &btx);
      const char *lead = (String(TTS_LANG) == "fr") ? "Nouveau bloc. " : "New block. ";
      char phrase[96];
      if (pool[0]) snprintf(phrase, sizeof(phrase), "%s%s.", lead, pool);
      else strlcpy(phrase, lead, sizeof(phrase));
      speak(phrase, SND_EVENT); }
#else
    playBellQ();
#endif
  }
  if (evWhale) { evWhale = false; playWhale(); }
  if (evAnomaly) { evAnomaly = false; playAlarm(); }   // z-score anormal détecté

  if (sleeping) { delay(50); return; }
  if (animNewBlock) { drawBlockAnim(); delay(40); return; }

  // ---------- redraw intelligent (plus de redraw forcé périodique) ----------
  // Les redraws des pages statiques sont différés tant que le doigt est posé :
  // un swipe n'est jamais perdu pendant un flush (~60 ms).
  if (page == PG_CUBE || page == PG_DOOM) {
    // pages animées : ~30 FPS permanent (le jeu doit tourner même doigt posé)
    if (now - lastDrawMs >= 33) { lastDrawMs = now; drawCurrentPage(); }
  } else if (page == PG_POOLS && (long)(poolsAnimUntil - now) > 0) {
    // course des barres : ~30 FPS le temps de converger
    if (!touched && now - lastDrawMs >= 33) { lastDrawMs = now; drawCurrentPage(); }
  } else if (page == PG_CHAIN) {
    // chrono "il y a Xs" : tick 1 Hz
    if (!touched && (needRedraw || now - lastDrawMs >= 1000)) { lastDrawMs = now; needRedraw = false; drawCurrentPage(); }
  } else if (page == PG_PRICE) {
    // prix animé : interpolation douce vers la cible (~30 FPS pendant la transition)
    float target = btcPrice[curCur];
    if (dispPrice <= 0 && target > 0) dispPrice = target;   // 1re valeur : directe
    if (fabsf(dispPrice - target) >= 0.5f) {
      if (!touched && now - lastDrawMs >= 33) {
        dispPrice += (target - dispPrice) * 0.18f;
        if (fabsf(dispPrice - target) < 0.5f) dispPrice = target;
        lastDrawMs = now; needRedraw = false; drawCurrentPage();
      }
    } else if (needRedraw && !touched) { lastDrawMs = now; needRedraw = false; drawCurrentPage(); }
  } else if (needRedraw && !touched) {
    // pages statiques : redraw uniquement sur événement / tick minute
    lastDrawMs = now; needRedraw = false; drawCurrentPage();
  }

  // ---------- WiFi perdu : reboot après 3 min ----------
  static unsigned long wifiLostSince = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiLostSince == 0) wifiLostSince = now;
    if (now - wifiLostSince > 180000) ESP.restart();
  } else wifiLostSince = 0;

  delay(5);
}
