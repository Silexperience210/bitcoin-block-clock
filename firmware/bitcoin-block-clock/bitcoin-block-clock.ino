// =====================================================================
//  BITCOIN BLOCK CLOCK V5 — Guition JC3248W535 (ESP32-S3 N16R8)
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
//  • V5 — moteur FX : transitions glissées, cinématique NOUVEAU BLOC, ligne
//    de vie du bloc, frise de blocs façon mempool.space, graphe qui se
//    dessine, cube en rotation, réseau Lightning vivant, DOOM texturé,
//    splash animé. Niveau MAX / ECO / OFF sur la page web (ECO la nuit).
//  • V5 — canvas à bornes vérifiées (débordement mémoire du texte GFX 1.4.9)
//  • Simulateur desktop : firmware/tools/sim (vrai code de dessin + ASan)
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

#define DEBUG_WM   0     // 1 = log série des stack high-water marks (diagnostic)
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
// FIX MÉMOIRE : GFX Library 1.4.9 ne clippe le texte (police glcdfont) qu'à
// DROITE et en BAS. Un caractère qui déborde à gauche ou en haut est écrit
// HORS du framebuffer (x < 0 -> avant le buffer PSRAM = corruption du tas).
// BTC DOOM le déclenchait sans cesse (affiches / noms d'ennemis au bord de
// l'écran) : cause probable des "crash DOOM" traqués avec les logs [WM].
// Ce canvas vérifie les bornes des deux primitives "préclippées" utilisées
// par le rendu du texte ; les lignes/rectangles passaient déjà par un clip.
class SafeCanvas : public Arduino_Canvas {
public:
  using Arduino_Canvas::Arduino_Canvas;
  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return;
    Arduino_Canvas::writePixelPreclipped(x, y, color);
  }
  void writeFillRectPreclipped(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > _width)  w = _width - x;
    if (y + h > _height) h = _height - y;
    if (w <= 0 || h <= 0) return;
    Arduino_Canvas::writeFillRectPreclipped(x, y, w, h, color);
  }
};
Arduino_Canvas *gfx = new SafeCanvas(PANEL_W, PANEL_H, panel, 0, 0, 1);
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

// Frise des derniers blocs (page ON-CHAIN, protégée par dataMux)
#define STRIP_N 4
long  stripH[STRIP_N] = {0}, stripTx[STRIP_N] = {0}, stripTs[STRIP_N] = {0};
float stripFee[STRIP_N] = {0};
char  stripPool[STRIP_N][14];
int   stripN = 0;
long  lambdaEpoch = 0;        // s/bloc mesuré sur l'époque de difficulté (0 = inconnu)

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
#define REQ_SIGNALS  0x800   // signaux calibrés (Binance 1d/1w, signals.h)
#define REQ_ALL      0xFFF
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
//  FX — moteur d'animations V5
//  ------------------------------------------------------------------
//  animLevel (NVS "anim", réglable sur la page web) :
//    2 = MAX  : toutes les pages vivantes (~25 FPS) + transitions + events
//    1 = ECO  : pages statiques (redraw sur événement) + transitions + events
//    0 = OFF  : aucun effet (comportement V4)
//  La nuit (23h-7h), MAX retombe automatiquement en ECO (batterie/chauffe).
//  Tout est dessiné dans le canvas PSRAM puis flushé en une fois : aucun
//  effet ne touche directement le panel.
// =====================================================================
#define FX_FRAME_MS 40                 // ~25 FPS pour les pages vivantes
#define FB_PIX  (SCR_W * SCR_H)
uint8_t animLevel = 2;
unsigned long fxT = 0;                 // horodatage de la frame en cours
unsigned long pageEnterMs = 0;         // arrivée sur la page (animations d'entrée)
struct tm gTm;                         // heure locale en cache (1 lecture / loop)
bool gTmOk = false;
uint16_t *fxPrevFb = nullptr, *fxNextFb = nullptr;   // transitions (PSRAM)

inline bool fxFull() { return animLevel >= 2 && !nightMode; }
inline bool fxAny()  { return animLevel >= 1; }

// heure locale sans blocage : getLocalTime(&t, 50) attendait jusqu'à 50 ms
// PAR APPEL tant que le NTP n'était pas synchronisé (plusieurs fois par frame)
void refreshClock() { gTmOk = getLocalTime(&gTm, 0); }

// mélange RGB565 : t = 0 -> a, 255 -> b
uint16_t mix565(uint16_t a, uint16_t b, uint8_t t) {
  uint32_t r = ((a >> 11) & 31) * (255 - t) + ((b >> 11) & 31) * t;
  uint32_t g = ((a >> 5) & 63) * (255 - t) + ((b >> 5) & 63) * t;
  uint32_t bl = (a & 31) * (255 - t) + (b & 31) * t;
  return (uint16_t)(((r / 255) << 11) | ((g / 255) << 5) | (bl / 255));
}
inline uint8_t u8f(float f) { return f <= 0 ? 0 : f >= 1 ? 255 : (uint8_t)(f * 255); }
inline float clamp01(float k) { return k < 0 ? 0 : (k > 1 ? 1 : k); }
float easeOutCubic(float k) { k = clamp01(k); float u = 1 - k; return 1 - u * u * u; }
float easeInOut(float k)    { k = clamp01(k); return k < 0.5f ? 4 * k * k * k : 1 - powf(-2 * k + 2, 3) / 2; }
float easeOutBack(float k)  { k = clamp01(k); const float c1 = 1.70158f, c3 = c1 + 1; float u = k - 1; return 1 + c3 * u * u * u + c1 * u * u; }
float easeOutBounce(float k) {
  k = clamp01(k); const float n1 = 7.5625f, d1 = 2.75f;
  if (k < 1 / d1) return n1 * k * k;
  if (k < 2 / d1) { k -= 1.5f / d1; return n1 * k * k + 0.75f; }
  if (k < 2.5f / d1) { k -= 2.25f / d1; return n1 * k * k + 0.9375f; }
  k -= 2.625f / d1; return n1 * k * k + 0.984375f;
}
// oscillation 0..1 (cosinus) de période periodMs
float fxPulse(unsigned long periodMs, unsigned long phase = 0) {
  return 0.5f - 0.5f * cosf(2 * PI * (float)((fxT + phase) % periodMs) / periodMs);
}
// progression 0..1 d'une animation d'entrée de page (1 si animations coupées)
float fxEnter(unsigned long durMs, unsigned long delayMs = 0) {
  if (!fxFull()) return 1.0f;
  long e = (long)(fxT - pageEnterMs) - (long)delayMs;
  return easeOutCubic(e / (float)durMs);
}
// accès direct au framebuffer du canvas (rotation 1 : colonne logique x = ligne native)
inline uint16_t *fxPix(int x, int y) { return gfx->getFramebuffer() + (int32_t)x * PANEL_W + (PANEL_W - 1 - y); }

// halo radial (disques concentriques du bord, faible, vers le centre, fort)
void fxHalo(int cx, int cy, int r, uint16_t c, uint16_t bg, uint8_t strength, int step = 3) {
  for (int i = r; i > 0; i -= step) {
    uint8_t t = (uint8_t)((uint32_t)strength * (r - i + step) / (r + step));
    gfx->fillCircle(cx, cy, i, mix565(bg, c, t));
  }
}
// anneau d'onde (2 px) qui s'élargit et s'estompe : k = 0..1
void fxRing(int cx, int cy, int r0, int r1, float k, uint16_t c, uint16_t bg) {
  if (k <= 0 || k >= 1) return;
  int r = r0 + (int)((r1 - r0) * k);
  uint16_t col = mix565(bg, c, u8f(1 - k));
  gfx->drawCircle(cx, cy, r, col);
  gfx->drawCircle(cx, cy, r + 1, col);
}
// barre de progression arrondie + reflet + bande lumineuse qui défile
void fxBar(int x, int y, int w, int h, float frac, uint16_t c, uint16_t track, bool shimmer = true) {
  gfx->fillRoundRect(x, y, w, h, h / 2, track);
  int fw = (int)(w * clamp01(frac));
  if (fw < h) { if (fw > 1) gfx->fillRoundRect(x, y, h, h, h / 2, c); return; }
  gfx->fillRoundRect(x, y, fw, h, h / 2, c);
  if (h >= 6) gfx->drawFastHLine(x + h / 2, y + 1, fw - h, mix565(c, C_WHITE, 110));
  if (shimmer && fxFull() && fw > 16) {
    int sx = x + (int)((fxT % 1800) / 1800.0f * (fw + 40)) - 20;
    for (int i = 0; i < 14; i++) {
      int xx = sx + i;
      if (xx < x + 2 || xx >= x + fw - 2) continue;
      gfx->drawFastVLine(xx, y + 1, h - 2, mix565(c, C_WHITE, (uint8_t)(150 - abs(i - 7) * 20)));
    }
  }
}
// reflet diagonal sur un bitmap RGB565 à couleur transparente (logo)
void fxShineBitmap(int x0, int y0, const uint16_t *bmp, int w, int h, uint16_t tr, float pos, int bw, uint8_t amt) {
  if (pos <= 0 || pos >= 1) return;
  int span = w + h / 2 + bw;
  int xs = -h / 2 - bw + (int)(pos * span);
  for (int y = 0; y < h; y++) {
    int xb = xs + y / 2;
    for (int x = max(0, xb); x < min(w, xb + bw); x++) {
      if (bmp[y * w + x] == tr) continue;
      int sx = x0 + x, sy = y0 + y;
      if (sx < 0 || sx >= SCR_W || sy < 0 || sy >= SCR_H) continue;
      uint16_t *p = fxPix(sx, sy);
      *p = mix565(*p, C_WHITE, amt);
    }
  }
}
// assombrit tout l'écran vers C_BG (fondu de sortie)
void fxDimAll(uint8_t amt) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int i = 0; i < FB_PIX; i++) fb[i] = mix565(fb[i], C_BG, amt);
}

// ---------- poussière de sats : particules de fond très discrètes ----------
#define FX_NDUST 36
struct FxDust { float x, y, v, ph; uint8_t b; };
FxDust fxDust[FX_NDUST];
bool fxDustInit = false;
void fxDrawDust() {
  if (!fxFull()) return;
  static unsigned long last = 0;
  if (!fxDustInit) {
    for (int i = 0; i < FX_NDUST; i++) {
      fxDust[i].x = random(0, SCR_W); fxDust[i].y = random(30, 292);
      fxDust[i].v = 6 + random(0, 18); fxDust[i].ph = random(0, 628) / 100.0f;
      fxDust[i].b = 22 + random(0, 50);
    }
    fxDustInit = true; last = fxT;
  }
  float dt = min(0.1f, (fxT - last) / 1000.0f); last = fxT;
  for (int i = 0; i < FX_NDUST; i++) {
    FxDust &d = fxDust[i];
    d.y -= d.v * dt;
    if (d.y < 30) { d.y = 290; d.x = random(0, SCR_W); }
    int xx = (int)(d.x + sinf(fxT / 1000.0f * 0.7f + d.ph) * 7);
    uint16_t c = mix565(C_BG, C_ORANGE, d.b);
    if (d.b > 55) gfx->fillRect(xx, (int)d.y, 2, 2, c); else gfx->drawPixel(xx, (int)d.y, c);
  }
}

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

// bruit blanc filtré à décroissance rapide : tirs / explosions (BTC DOOM)
void synthNoise(float durSec, float vol) {
  const int chunk = 256;
  int16_t buf[chunk * 2];
  long total = (long)(SAMPLE_RATE * durSec), pos = 0;
  uint32_t r = 0x2545F491; float lp = 0;
  while (pos < total) {
    int i = 0;
    for (; i < chunk && pos < total; i++, pos++) {
      r = r * 1664525u + 1013904223u;
      float w = ((int32_t)(r >> 16) - 32768) / 32768.0f;
      lp += 0.45f * (w - lp);
      float t = (float)pos / SAMPLE_RATE;
      int16_t v = (int16_t)(lp * expf(-t * 5.5f / durSec) * vol * 30000);
      buf[i * 2] = v; buf[i * 2 + 1] = v;
    }
    size_t w;
    i2s_write(I2S_PORT, buf, i * 2 * sizeof(int16_t), &w, portMAX_DELAY);
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
  // FIX : HTTP/1.0 -> Google ne répond plus en "chunked" ; avant, les
  // marqueurs de chunk (\r\n1f40\r\n...) se retrouvaient DANS le MP3.
  http.useHTTP10(true);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) { Serial.printf("[TTS] HTTP %d\n", code); http.end(); return false; }
  // flux brut jusqu'à la fermeture de la connexion
  const int TTS_MAX = 65536;
  uint8_t *mp3 = (uint8_t*)ps_malloc(TTS_MAX);
  if (!mp3) { http.end(); return false; }
  WiFiClient *st = http.getStreamPtr();
  int got = 0;
  unsigned long t0 = millis();
  // FIX : WiFiClientSecure::read() renvoie -1 quand AUCUNE donnée n'est
  // encore arrivée (pas seulement en fin de flux) -> l'ancienne boucle
  // s'arrêtait au premier creux réseau et tronquait la phrase.
  while (got < TTS_MAX && millis() - t0 < 10000) {
    int av = st->available();
    if (av <= 0) {
      if (!st->connected()) break;       // vraie fin du flux
      delay(4); continue;
    }
    int r = st->read(mp3 + got, min(av, TTS_MAX - got));
    if (r > 0) got += r;
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
      if (n.freq == 0) synthNoise(n.durMs / 1000.0f, n.vol / 100.0f);   // effets DOOM
      else synthBell((float)n.freq, n.durMs / 1000.0f, n.vol / 100.0f);
    }
#if DEBUG_WM
    static unsigned long tWm2 = 0;
    if (millis() - tWm2 > 10000) { tWm2 = millis();
      Serial.printf("[WM] snd free=%u\n", uxTaskGetStackHighWaterMark(NULL)); }
#endif
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
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)8) != 8) return false;
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
  if (maxPts > 5) maxPts = 5;                      // d[30] = 5 doigts max
  Wire.beginTransmission(TP_ADDR);
  Wire.write(cmd, 8);
  if (Wire.endTransmission(false) != 0) return 0;
  int want = maxPts * 6;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)want) != want) return 0;
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
  animLevel  = min((uint8_t)2, prefs.getUChar("anim", 2));
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
  prefs.putUChar("anim", animLevel);
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

// échappement HTML (SSID / IP saisis par l'utilisateur ou diffusés par des voisins)
String htmlEsc(const String &in) {
  String o;
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;";
    else if (c == '&') o += "&amp;"; else if (c == '\'') o += "&#39;";
    else if (c == '"') o += "&quot;"; else o += c;
  }
  return o;
}

String scanNetworks() {
  int n = WiFi.scanNetworks();
  String s = "<select onchange=\"document.getElementsByName('ssid')[0].value=this.value\"><option value=''>-- reseaux --</option>";
  for (int i = 0; i < n && i < 15; i++) { String e = htmlEsc(WiFi.SSID(i)); s += "<option value='" + e + "'>" + e + "</option>"; }
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
    p.replace("%NODEIP%", htmlEsc(cfg_nodeip));
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
  // FIX : si un WiFi est déjà configuré (ex. coupure de courant, box plus
  // lente à redémarrer que l'horloge), on ne reste pas coincé à vie dans le
  // portail : redémarrage au bout de 5 min pour retenter la connexion.
  unsigned long tp = millis();
  while (true) {
    server.handleClient(); delay(2);
    if (cfg_ssid.length() > 0 && millis() - tp > 300000UL) { Serial.println("[WiFi] portail : délai, nouvel essai"); ESP.restart(); }
  }
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
  // FIX 1H : days=1 renvoie ~288 points à 5 min sur 24 h ; l'ancien
  // sous-échantillonnage (step = 288/12) affichait donc 24 h sous "1H".
  // On garde maintenant les 13 derniers points (= la dernière heure).
  int start = 0, step;
  if (ft == TF_1H) { start = max(0, total - (TF_TARGET[ft] + 1)); step = 1; }
  else step = max(1, total / TF_TARGET[ft]);
  // remplissage en local puis copie protégée (l'UI lit pendant ce temps)
  float tc[MAX_PTS]; long tt[MAX_PTS]; int tn = 0;
  for (int i = start; i < total && tn < MAX_PTS; i += step) {
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
    cursorIdx = -1;                           // FIX : seulement si c'est la vue affichée
  }
  portEXIT_CRITICAL(&dataMux);
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
    // FIX course core0/core1 : on récupère d'abord pool + tx du NOUVEAU
    // bloc et on publie la hauteur, PUIS on lève l'événement. Avant, l'UI
    // annonçait (voix + flash) le pool du bloc PRÉCÉDENT.
    fetchLastBlock();
    blockHeight = h;
    evNewBlock = true;          // le loop déclenchera anim + annonce
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
  // filtre : 15 blocs avec extras complets = gros payload, on ne garde que l'utile
  JsonDocument filter;
  filter[0]["height"] = true;
  filter[0]["timestamp"] = true;
  filter[0]["tx_count"] = true;
  filter[0]["extras"]["medianFee"] = true;
  filter[0]["extras"]["pool"]["name"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, r, DeserializationOption::Filter(filter))) return;
  JsonObject b = doc[0];
  if (b.isNull()) return;
  long ts[6]; int n = 0;
  long sH[STRIP_N]; long sTx[STRIP_N]; float sFee[STRIP_N]; long sTs[STRIP_N]; char sPool[STRIP_N][14]; int sn = 0;
  for (JsonObject blk : doc.as<JsonArray>()) {
    long t = blk["timestamp"] | 0L;
    if (n < 6 && t > 0) ts[n++] = t;
    if (sn < STRIP_N) {
      sH[sn] = blk["height"] | 0L; sTx[sn] = blk["tx_count"] | 0L;
      sFee[sn] = blk["extras"]["medianFee"] | 0.0f; sTs[sn] = t;
      strlcpy(sPool[sn], blk["extras"]["pool"]["name"] | "?", sizeof(sPool[sn]));
      sn++;
    }
    if (n >= 6 && sn >= STRIP_N) break;
  }
  portENTER_CRITICAL(&dataMux);
  strlcpy(lastPool, b["extras"]["pool"]["name"] | "-", sizeof(lastPool));
  lastBlockTx = b["tx_count"] | 0L;
  memcpy(blkTs, ts, n * sizeof(long));
  blkTsN = n;
  memcpy(stripH, sH, sizeof(sH)); memcpy(stripTx, sTx, sizeof(sTx));
  memcpy(stripFee, sFee, sizeof(sFee)); memcpy(stripTs, sTs, sizeof(sTs));
  memcpy(stripPool, sPool, sizeof(sPool)); stripN = sn;
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
  // rythme mesuré sur toute l'époque (~2016 blocs) : bien plus stable que
  // la moyenne des 6 derniers blocs (écart-type ~45 %) — cf. PATCH-SIGNAUX.md
  double tAvg = doc["timeAvg"] | 0.0;          // ms / bloc
  if (tAvg > 0) lambdaEpoch = constrain((long)(tAvg / 1000.0), 300L, 1200L);
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
static bool fetchSignals();   // signals.h (inclus plus bas, après les helpers UI)

void netTask(void *param) {
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
  unsigned long tHeight = 0, tPrice = 0, tKlines = 0, tMempool = 0, tWhale = 0,
                tDiff = 0, tFng = 0, tNode = 0, tPools = 0, tLn = 0, tKl30 = 0, tBktSave = 0,
                tSig = 0;
  bool sigOk = false;
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
    // signaux calibrés (signals.h) : 2 requêtes Binance ~55 Ko en streaming, 1×/4 h
    // (échec -> nouvel essai dans 10 min)
    if ((req & REQ_SIGNALS) || tSig == 0 || now - tSig > (sigOk ? 4UL * 3600 * 1000 : 600000UL)) {
      tSig = now; if (tSig == 0) tSig = 1;
      sigOk = fetchSignals(); worked = true;
    }

    // sauvegarde NVS des cycles de fees (max 1×/30 min)
    if (feeBktDirty && now - tBktSave > 1800000) { tBktSave = now; saveFeeBuckets(); }

    // latch détection d'anomalies (z > 2.5 -> event, reset z < 1.5)
    bool an = (fabsf(anomMemZ) > 2.5f || fabsf(anomFeeZ) > 2.5f);
    if (an && !anomActive) { anomActive = true; evAnomaly = true; }
    else if (!an && anomActive && fabsf(anomMemZ) < 1.5f && fabsf(anomFeeZ) < 1.5f) anomActive = false;

    if (worked) needRedraw = true;
    vTaskDelay(pdMS_TO_TICKS(200));
#if DEBUG_WM
    static unsigned long tWm3 = 0;
    if (worked && millis() - tWm3 > 10000) { tWm3 = millis();
      Serial.printf("[WM] net free=%u\n", uxTaskGetStackHighWaterMark(NULL)); }
#endif
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
// secondes depuis le dernier bloc, d'après l'horodatage RÉEL du bloc
// (FIX : l'ancien chrono partait du moment où l'horloge avait DÉTECTÉ le
//  bloc -> "il y a 0m03s" à chaque démarrage)
long secsSinceBlock() {
  long t0;
  portENTER_CRITICAL(&dataMux);
  t0 = blkTsN > 0 ? blkTs[0] : 0;
  portEXIT_CRITICAL(&dataMux);
  time_t now = time(nullptr);
  if (t0 <= 0 || now < 1600000000) return -1;
  return max(0L, (long)now - t0);
}

void drawHeader() {
  // fond : dégradé vertical discret
  for (int y = 0; y < 26; y++) gfx->drawFastHLine(0, y, SCR_W, mix565(C_PANEL, C_BG, (uint8_t)(y * 255 / 25)));
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(10, 8);
  if (gTmOk) {
    gfx->printf("%02d", gTm.tm_hour);
    bool colonOn = !fxFull() || (gTm.tm_sec % 2 == 0);       // deux-points qui battent la seconde
    gfx->setTextColor(colonOn ? C_WHITE : C_DGREY); gfx->print(":");
    gfx->setTextColor(C_WHITE); gfx->printf("%02d", gTm.tm_min);
  } else gfx->print("--:--");
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(78, 12);
  if (gTmOk) gfx->printf("%02d/%02d", gTm.tm_mday, gTm.tm_mon + 1);
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

  // ---- LIGNE DE VIE DU BLOC : progression vers ~10 min + comète ----
  gfx->drawFastHLine(0, 26, SCR_W, C_LINE);
  long el = secsSinceBlock();
  if (el >= 0) {
    float k = el / 600.0f;
    bool late = k > 1.0f;                                     // > 10 min : vire au rouge
    int w = (int)(SCR_W * clamp01(k));
    uint16_t c = late ? mix565(C_ORANGE, C_RED, u8f(fxFull() ? fxPulse(1600) : 1.0f)) : C_ORANGE;
    for (int x = 0; x < w; x += 8)
      gfx->drawFastHLine(x, 26, min(8, w - x), mix565(C_ORANGE_D, c, (uint8_t)(x * 255 / max(1, w))));
    gfx->drawFastHLine(0, 25, w, mix565(C_BG, c, 60));
    if (!late && w > 3) {
      if (fxFull()) {
        int r = 3 + (int)(2 * fxPulse(900));
        gfx->fillCircle(w, 26, r, mix565(C_BG, C_YELLOW, 70));
      }
      gfx->fillCircle(w, 26, 2, C_YELLOW);
      gfx->drawPixel(w, 26, C_WHITE);
    }
  }
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

float fxTabX = -1;          // position animée du soulignement d'onglet

void drawFooter() {
  gfx->drawFastHLine(0, NAV_Y, SCR_W, C_LINE);
  float target = page * TAB_W + TAB_W / 2.0f;
  if (fxTabX < 0 || !fxFull()) fxTabX = target;
  else { fxTabX += (target - fxTabX) * 0.35f; if (fabsf(target - fxTabX) < 0.6f) fxTabX = target; }
  int ux = (int)fxTabX;
  if (fxFull()) fxHalo(ux, SCR_H - 14, 11, C_ORANGE, C_BG, (uint8_t)(35 + 30 * fxPulse(2400)));
  for (int i = 0; i < PG_COUNT; i++) {
    int cx = i * TAB_W + TAB_W / 2;
    bool act = (i == page);
    drawTabIcon(i, cx, SCR_H - 14, act ? C_ORANGE : C_DGREY);
  }
  gfx->fillRoundRect(ux - 16, SCR_H - 3, 32, 3, 1, C_ORANGE);
  gfx->drawFastHLine(ux - 10, SCR_H - 3, 20, C_YELLOW);
}

// =====================================================================
//  PAGE 0 — PRIX + GRAPHE
// =====================================================================
#define GX 186
#define GY 66
#define GW 284
#define GH 190

// drawChart V5 : aire en VRAI dégradé (bandes), ligne avec lueur, grille,
// tracé qui "se dessine" à chaque nouvelle série, point final pulsant.
unsigned long chartAnimMs = 0;

void drawChart() {
  gfx->fillRoundRect(GX - 6, GY - 8, GW + 12, GH + 40, 8, C_PANEL);
  // snapshot protégé des données (écrites par netTask)
  static float c[MAX_PTS]; static long ts[MAX_PTS];
  portENTER_CRITICAL(&dataMux);
  int n = nPts;
  memcpy(c, closes, n * sizeof(float));
  memcpy(ts, tsOf, n * sizeof(long));
  portEXIT_CRITICAL(&dataMux);
  if (n < 2) {
    // FIX : "chargement..." centré sur le graphe (il l'était sur l'écran)
    gfx->setTextSize(1); gfx->setTextColor(C_GREY);
    gfx->setCursor(GX + GW / 2 - 39, GY + GH / 2 + 14); gfx->print("chargement...");
    for (int i = 0; i < 8; i++) {                         // spinner
      float a = i * PI / 4 + (fxFull() ? fxT / 180.0f : 0);
      uint8_t k = (uint8_t)(40 + i * 26);
      gfx->fillCircle(GX + GW / 2 + (int)(cosf(a) * 12), GY + GH / 2 - 8 + (int)(sinf(a) * 12), 2, mix565(C_PANEL, C_ORANGE, k));
    }
    return;
  }
  // nouvelle série (fetch, switch devise/timeframe) -> rejouer le tracé
  static int lastN = -1; static float lastA = 0, lastB = 0;
  if (n != lastN || c[0] != lastA || c[n - 1] != lastB) { lastN = n; lastA = c[0]; lastB = c[n - 1]; chartAnimMs = fxT; }
  float kr = fxFull() ? easeOutCubic((long)(fxT - chartAnimMs) / 900.0f) : 1.0f;

  float mn = c[0], mx = c[0];
  for (int i = 1; i < n; i++) { if (c[i] < mn) mn = c[i]; if (c[i] > mx) mx = c[i]; }
  float range = (mx - mn); if (range < 0.01f) range = 0.01f;

  auto yOf = [&](float v) { return GY + GH - 6 - (int)((v - mn) / range * (GH - 14)); };
  auto xOf = [&](int i) { return GX + 4 + (int)((long)i * (GW - 10) / (n - 1)); };
  const int xRev = GX + 4 + (int)((GW - 10) * kr);       // abscisse déjà tracée
  bool up = c[n - 1] >= c[0];
  uint16_t lc = up ? C_GREEN : C_RED;

  // grille discrète + pointillés du prix d'ouverture
  for (int g = 1; g <= 3; g++) {
    int gy = GY + (GH - 10) * g / 4;
    for (int x = GX + 4; x < GX + GW - 6; x += 5) gfx->drawPixel(x, gy, C_LINE);
  }
  int yOpen = yOf(c[0]);
  for (int x = GX + 4; x < GX + GW - 6; x += 6) gfx->fillRect(x, yOpen, 3, 1, C_DGREY);

  // aire sous la courbe : dégradé vertical en 8 bandes (couleur de tendance)
  const int NB = 8;
  uint16_t band[NB];
  for (int k = 0; k < NB; k++) band[k] = mix565(C_PANEL, lc, (uint8_t)(92 - k * 11));
  const int base = GY + GH - 4, top0 = GY, bandH = (base - top0) / NB + 1;
  for (int i = 0; i < n - 1; i++) {
    int x0 = xOf(i), x1 = xOf(i + 1);
    if (x0 > xRev) break;
    int y0 = yOf(c[i]), y1 = yOf(c[i + 1]);
    int xe = (i == n - 2) ? x1 : x1 - 1;
    for (int x = x0; x <= xe && x <= xRev; x++) {
      int y = y0 + (int)((long)(y1 - y0) * (x - x0) / max(1, x1 - x0));
      for (int k = max(0, (y - top0) / bandH); k < NB; k++) {
        int ys = max(y + 2, top0 + k * bandH), ye = min(base, top0 + (k + 1) * bandH);
        if (ye > ys) gfx->drawFastVLine(x, ys, ye - ys, band[k]);
      }
    }
  }
  // courbe : lueur puis trait 2 px
  uint16_t glow = mix565(C_PANEL, lc, 85);
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < n - 1; i++) {
      int x0 = xOf(i), y0 = yOf(c[i]), x1 = xOf(i + 1), y1 = yOf(c[i + 1]);
      if (x0 > xRev) break;
      if (x1 > xRev) { y1 = y0 + (int)((long)(y1 - y0) * (xRev - x0) / max(1, x1 - x0)); x1 = xRev; }
      if (pass == 0) { gfx->drawLine(x0, y0 - 1, x1, y1 - 1, glow); gfx->drawLine(x0, y0 + 2, x1, y1 + 2, glow); }
      else { gfx->drawLine(x0, y0, x1, y1, lc); gfx->drawLine(x0, y0 + 1, x1, y1 + 1, lc); }
    }
  }
  if (kr < 1.0f) {
    // tête lumineuse pendant le tracé
    int i = constrain((int)((long)(xRev - GX - 4) * (n - 1) / (GW - 10)), 0, n - 2);
    int x0 = xOf(i), x1 = xOf(i + 1);
    int yh = yOf(c[i]) + (int)((long)(yOf(c[i + 1]) - yOf(c[i])) * (xRev - x0) / max(1, x1 - x0));
    fxHalo(xRev, yh, 10, lc, C_PANEL, 150, 2);
    gfx->fillCircle(xRev, yh, 3, C_WHITE);
  } else {
    // dernier point : onde qui pulse ("live")
    int xl = xOf(n - 1), yl = yOf(c[n - 1]);
    if (fxFull()) {
      fxRing(xl, yl, 4, 20, (fxT % 1700) / 1700.0f, lc, C_PANEL);
      fxRing(xl, yl, 4, 20, ((fxT + 850) % 1700) / 1700.0f, lc, C_PANEL);
    }
    gfx->fillCircle(xl, yl, 4, lc);
    gfx->fillCircle(xl, yl, 2, C_WHITE);
  }

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

unsigned long priceFlashMs = 0;       // flash vert/rouge du prix au changement
bool priceFlashUp = true;

void stepDispPrice() {
  float target = btcPrice[curCur];
  if (fabsf(dispPrice - target) >= 0.5f) {
    dispPrice += (target - dispPrice) * 0.18f;
    if (fabsf(dispPrice - target) < 0.5f) dispPrice = target;
  }
}

void drawPagePrice() {
  // colonne gauche : logo (halo pulsant + reflet) + prix + variation + devise
  if (fxFull()) fxHalo(84, 66, 38, C_ORANGE, C_BG, (uint8_t)(35 + 45 * fxPulse(3200)), 3);
  gfx->draw16bitRGBBitmapWithTranColor(52, 34, (uint16_t*)BTC_LOGO_64, TRANSP, 64, 64);
  if (fxFull()) fxShineBitmap(52, 34, BTC_LOGO_64, 64, 64, TRANSP, (fxT % 5200) / 1100.0f, 12, 150);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(52, 104); gfx->print("BTC / " + String(CUR_LABEL[curCur]));
  // prix en chiffres lissés (animé via dispPrice) ; flash vert/rouge au changement
  float p = (dispPrice > 0) ? dispPrice : btcPrice[curCur];
  uint16_t pc = C_WHITE;
  if (fxAny() && priceFlashMs && millis() - priceFlashMs < 1400)
    pc = mix565(priceFlashUp ? C_GREEN : C_RED, C_WHITE, u8f(easeInOut((millis() - priceFlashMs) / 1400.0f)));
  if (p > 0) drawSmooth(10, 118, prettyNum((long)p).c_str(), pc, C_BG);
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
    if (active && fxFull()) gfx->drawRoundRect(GX + i * 72 - 2, 32, 70, 26, 13, mix565(C_BG, C_ORANGE, (uint8_t)(60 + 90 * fxPulse(2000))));
    drawPill(GX + i * 72, 34, 66, 22, active ? C_ORANGE : C_PANEL, active ? C_BG : C_GREY, TF_LABEL[i], 2);
  }
  drawChart();
}

// =====================================================================
//  PAGE 1 — ON-CHAIN (V5 : frise de blocs façon mempool.space)
//  [bloc en attente qui se remplit] ┊ [4 derniers blocs minés]
//  Nouveau bloc : il sort du bloc en attente et pousse la frise.
// =====================================================================
// couleur d'un niveau de fees (vert = pas cher -> rouge = cher)
uint16_t feeColor(float f) {
  if (f <= 0) return C_DGREY;
  if (f < 4)  return mix565(C_GREEN_D, C_GREEN, u8f(f / 4));
  if (f < 12) return mix565(C_GREEN, C_YELLOW, u8f((f - 4) / 8));
  if (f < 30) return mix565(C_YELLOW, C_ORANGE, u8f((f - 12) / 18));
  if (f < 80) return mix565(C_ORANGE, C_RED, u8f((f - 30) / 50));
  return C_RED;
}

#define BLK_W   70          // face avant
#define BLK_D   8           // profondeur 3D
#define BLK_Y   132         // haut de la face avant
#define BLK_X0  118         // 1er bloc miné
#define BLK_DX  88          // pas entre blocs
unsigned long stripAnimMs = 0;

// volume 3D : dessus + flanc droit (la face avant est dessinée par l'appelant)
void blockShell(int x, int y, int w, int d, uint16_t top, uint16_t side) {
  gfx->fillTriangle(x, y, x + d, y - d, x + w + d, y - d, top);
  gfx->fillTriangle(x, y, x + w + d, y - d, x + w, y, top);
  gfx->fillTriangle(x + w, y, x + w + d, y - d, x + w + d, y + w - d, side);
  gfx->fillTriangle(x + w, y, x + w + d, y + w - d, x + w, y + w, side);
}

void drawMinedBlock(int x, int i, long h, float fee, long tx, long ts, const char *pool, bool fresh) {
  uint16_t base = feeColor(fee);
  const int y = BLK_Y, w = BLK_W, d = BLK_D;
  blockShell(x, y, w, d, mix565(base, C_WHITE, 70), mix565(C_PANEL, base, 90));
  for (int k = 0; k < 7; k++)                                      // face avant en dégradé
    gfx->fillRect(x, y + k * 10, w, 10, mix565(C_PANEL, base, (uint8_t)(205 - k * 14)));
  if (fresh) gfx->drawRect(x, y, w, w, mix565(C_WHITE, C_YELLOW, u8f(fxPulse(900))));
  gfx->setTextSize(1);
  gfx->setTextColor(i == 0 ? C_ORANGE : C_GREY);
  char hs[12]; snprintf(hs, sizeof(hs), "%ld", h);
  gfx->setCursor(x + (w + d - (int)strlen(hs) * 6) / 2, 114); gfx->print(hs);
  char fs[10]; snprintf(fs, sizeof(fs), "~%d", (int)(fee + 0.5f));
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(x + (w - (int)strlen(fs) * 12) / 2, y + 7); gfx->print(fs);
  gfx->setTextSize(1); gfx->setTextColor(mix565(C_WHITE, base, 80));
  gfx->setCursor(x + (w - 36) / 2, y + 26); gfx->print("sat/vB");
  char ts2[16]; snprintf(ts2, sizeof(ts2), "%ld tx", tx);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(x + (w - (int)strlen(ts2) * 6) / 2, y + 42); gfx->print(ts2);
  time_t now = time(nullptr);
  if (ts > 0 && now > 1600000000) {
    long m = max(0L, ((long)now - ts) / 60);
    char ag[12]; snprintf(ag, sizeof(ag), m < 1 ? "<1 min" : "%ld min", m);
    gfx->setTextColor(mix565(C_WHITE, base, 90));
    gfx->setCursor(x + (w - (int)strlen(ag) * 6) / 2, y + 56); gfx->print(ag);
  }
  char pn[13]; strlcpy(pn, pool, sizeof(pn));
  gfx->setTextColor(C_GREY);
  gfx->setCursor(x + (w - (int)strlen(pn) * 6) / 2, 206); gfx->print(pn);
}

// bloc en attente : "liquide" qui monte avec le temps écoulé, vagues + bulles
void drawPendingBlock(long el) {
  const int x = 14, y = BLK_Y, w = BLK_W, d = BLK_D;
  uint16_t fc = feeColor((float)feeFast);
  blockShell(x, y, w, d, mix565(C_PANEL, fc, 60), mix565(C_BG, fc, 45));
  gfx->fillRect(x, y, w, w, mix565(C_BG, C_PANEL, 160));
  float lvl = el >= 0 ? 0.06f + 0.94f * clamp01(el / 600.0f) : 0.3f;
  float t = fxFull() ? fxT / 1000.0f : 0;
  for (int xx = 0; xx < w; xx++) {
    float wv = sinf(xx * 0.19f + t * 3.1f) * 2.2f + sinf(xx * 0.07f - t * 1.7f) * 1.6f;
    int top = y + w - (int)(lvl * (w - 4)) + (int)wv;
    top = constrain(top, y + 1, y + w - 1);
    gfx->drawFastVLine(x + xx, top, 2, mix565(fc, C_WHITE, 120));                // crête
    for (int k = 0; k < 3; k++) {                                                  // corps en dégradé
      int ys = max(top + 2, y + w - (int)((w - 2) * (3 - k) / 3.0f)), ye = y + w - (int)((w - 2) * (2 - k) / 3.0f);
      if (ye > ys) gfx->drawFastVLine(x + xx, ys, ye - ys, mix565(C_PANEL, fc, (uint8_t)(120 + k * 45)));
    }
  }
  if (fxFull()) {                                                                  // bulles
    for (int k = 0; k < 6; k++) {
      float ph = fmodf(t * (0.35f + k * 0.07f) + k * 0.37f, 1.0f);
      int bx = x + 8 + (k * 11) % (w - 16) + (int)(sinf(t * 2 + k) * 3);
      int by = y + w - 4 - (int)(ph * lvl * (w - 8));
      if (by > y + w - (int)(lvl * (w - 4)) + 3) gfx->drawCircle(bx, by, 1 + k % 2, mix565(fc, C_WHITE, 170));
    }
  }
  gfx->drawRect(x, y, w, w, mix565(C_BG, C_ORANGE, u8f(fxFull() ? 0.45f + 0.55f * fxPulse(1400) : 1.0f)));
  // textes
  char fs[10]; snprintf(fs, sizeof(fs), "~%d", feeFast);
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(x + (w - (int)strlen(fs) * 12) / 2, y + 7); gfx->print(fs);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(x + (w - 36) / 2, y + 26); gfx->print("sat/vB");
  long lam = lambdaEpoch > 0 ? lambdaEpoch : 600;
  long rem = el >= 0 ? max(0L, (lam - el) / 60) : -1;
  char rs[14];
  if (rem < 0) strlcpy(rs, "...", sizeof(rs));
  else if (rem == 0) strlcpy(rs, "imminent", sizeof(rs));
  else snprintf(rs, sizeof(rs), "~%ld min", rem);
  gfx->setTextColor(C_WHITE);
  gfx->setCursor(x + (w - (int)strlen(rs) * 6) / 2, y + 44); gfx->print(rs);
  gfx->setTextColor(C_ORANGE);
  gfx->setCursor(x + (w + d - 48) / 2, 114); gfx->print("prochain");
  gfx->setTextColor(C_GREY);
  gfx->setCursor(x + (w - 42) / 2, 206); gfx->print("mempool");
}

void drawPageChain() {
  // ---------- snapshot frise ----------
  long sH[STRIP_N], sTx[STRIP_N], sTs[STRIP_N]; float sFee[STRIP_N]; char sPool[STRIP_N][14]; int sn;
  portENTER_CRITICAL(&dataMux);
  sn = stripN;
  memcpy(sH, stripH, sizeof(sH)); memcpy(sTx, stripTx, sizeof(sTx)); memcpy(sTs, stripTs, sizeof(sTs));
  memcpy(sFee, stripFee, sizeof(sFee)); memcpy(sPool, stripPool, sizeof(sPool));
  portEXIT_CRITICAL(&dataMux);
  static long lastTop = 0;
  if (sn > 0 && sH[0] != lastTop) { if (lastTop != 0) stripAnimMs = fxT; lastTop = sH[0]; }
  float ka = fxFull() && stripAnimMs ? easeOutCubic((long)(fxT - stripAnimMs) / 800.0f) : 1.0f;
  long el = secsSinceBlock();

  // ---------- hauteur + chrono ----------
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(12, 34); gfx->print("BLOCK HEIGHT");
  uint16_t hc = ka < 1.0f ? mix565(C_ORANGE, C_WHITE, u8f(ka)) : C_WHITE;
  if (blockHeight > 0) drawSmooth(10, 44, String(blockHeight).c_str(), hc, C_BG);
  else { gfx->setTextSize(5); gfx->setTextColor(C_WHITE); gfx->setCursor(10, 50); gfx->print("------"); }
  char pool[32]; long btx; snapLastBlock(pool, sizeof(pool), &btx);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(12, 90);
  // FIX : séparateurs ASCII (le "·" UTF-8 s'affichait en glyphes parasites)
  if (el >= 0) gfx->printf("il y a %ldm%02lds  -  %s  -  %ld tx", el / 60, el % 60, pool, btx);
  else gfx->print("en attente des donnees...");
  float kp = el >= 0 ? el / 600.0f : 0;
  fxBar(10, 102, 286, 6, kp, kp > 1 ? C_RED : C_ORANGE, C_PANEL);

  // ---------- mempool (droite) ----------
  gfx->fillRoundRect(306, 32, 164, 76, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(316, 40); gfx->print("MEMPOOL");
  gfx->setTextSize(3); gfx->setTextColor(C_WHITE);
  gfx->setCursor(316, 52); gfx->print(prettyNum(mempoolCount));
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(316, 80); gfx->print("TX en attente");
  if (fxFull()) {                                   // flux de transactions vers le bloc en attente
    for (int k = 0; k < 14; k++) {
      int px = 460 - (int)(((fxT / 12) + k * 23) % 146);
      gfx->fillRect(px, 96, 2 + (k % 3), 2, mix565(C_PANEL, feeColor((float)feeFast), (uint8_t)(90 + (k * 37) % 150)));
    }
  } else gfx->drawFastHLine(316, 97, 144, C_LINE);

  // ---------- frise ----------
  // séparateur "maintenant" : pointillés qui défilent
  int dash = fxFull() ? (int)((fxT / 60) % 8) : 0;
  for (int y = 112 + dash; y < 214; y += 8) gfx->drawFastVLine(104, y, 4, C_GREY);
  int off = (int)((1.0f - ka) * BLK_DX);
  for (int i = min(sn, STRIP_N) - 1; i >= 0; i--)
    drawMinedBlock(BLK_X0 + i * BLK_DX - off, i, sH[i], sFee[i], sTx[i], sTs[i], sPool[i], i == 0 && ka < 1.0f);
  if (sn == 0) {
    gfx->setTextSize(1); gfx->setTextColor(C_GREY);
    gfx->setCursor(BLK_X0 + 90, 164); gfx->print("chargement des blocs...");
  }
  drawPendingBlock(el);

  // ---------- fees ----------
  const char* fl[4] = {"rapide", "30 min", "1 h", "eco"};
  int fv[4] = {feeFast, feeHalf, feeHour, feeEco};
  int fmax = max(1, feeFast);
  for (int i = 0; i < 4; i++) {
    int fx = 10 + i * 116, fy = 216;
    uint16_t c = feeColor((float)fv[i]);
    gfx->fillRoundRect(fx, fy, 110, 34, 6, C_PANEL);
    gfx->fillRoundRect(fx, fy + 4, 3, 26, 1, c);
    gfx->setTextSize(2); gfx->setTextColor(i == 0 ? C_ORANGE : C_WHITE);
    gfx->setCursor(fx + 10, fy + 5);
    String v = fv[i] > 0 ? String(fv[i]) : String("-");
    gfx->print(v);
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(fx + 14 + v.length() * 12, fy + 11); gfx->print("sat/vB");
    gfx->setCursor(fx + 10, fy + 24); gfx->print(fl[i]);
    int bh = (int)(24 * clamp01(fv[i] / (float)fmax) * fxEnter(700, i * 90));
    gfx->fillRoundRect(fx + 96, fy + 5, 6, 24, 2, C_BG);
    if (bh > 1) gfx->fillRoundRect(fx + 96, fy + 29 - bh, 6, bh, 2, c);
  }

  // ---------- difficulté / halving / whale ----------
  gfx->fillRoundRect(10, 256, 150, 34, 6, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(18, 261); gfx->print("DIFFICULTE");
  gfx->setTextSize(2); gfx->setTextColor(diffChange >= 0 ? C_GREEN : C_RED);
  gfx->setCursor(18, 272); gfx->printf("%s%.1f%%", diffChange >= 0 ? "+" : "", diffChange);
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(94, 261); gfx->printf("%ld", diffRemaining);
  gfx->setCursor(94, 277); gfx->print("blocs");

  // FIX : halving calculé (l'ancien 1 050 000 en dur devenait négatif après 2028)
  gfx->fillRoundRect(166, 256, 148, 34, 6, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(174, 261); gfx->print("HALVING");
  if (blockHeight > 0) {
    long nextH = (blockHeight / 210000L + 1) * 210000L;
    long daysH = (nextH - blockHeight) * 10L / 1440L;
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
    gfx->setCursor(174, 272); gfx->printf("%ldj", daysH);
    fxBar(240, 276, 66, 6, (blockHeight % 210000L) / 210000.0f * fxEnter(900), C_ORANGE, C_BG);
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(240, 262); gfx->printf("#%ld", nextH / 1000);
    gfx->print("k");
  }

  gfx->fillRoundRect(320, 256, 150, 34, 6, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_ORANGE);
  gfx->setCursor(328, 261); gfx->print("WHALE WATCH");
  if (whaleBtc >= 50) {
    if (fxFull()) fxRing(456, 266, 2, 9, (fxT % 1500) / 1500.0f, C_ORANGE, C_PANEL);
    gfx->fillCircle(456, 266, 3, C_ORANGE);
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
    gfx->setCursor(328, 272); gfx->printf("%.1f", whaleBtc);
    gfx->setTextSize(1); gfx->setTextColor(C_GREY); gfx->print(" BTC");
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(328, 276); gfx->print("calme plat...");
  }
}

// =====================================================================
//  PAGE 5 — NŒUD & SENTIMENT
// =====================================================================
void drawGauge(int cx, int cy, int r, float value) {
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
  // graduations 0 / 25 / 50 / 75 / 100
  for (int g = 0; g <= 4; g++) {
    float a = PI - g * PI / 4;
    gfx->drawLine(cx + (int)(cosf(a) * (r + 3)), cy - (int)(sinf(a) * (r + 3)),
                  cx + (int)(cosf(a) * (r + 8)), cy - (int)(sinf(a) * (r + 8)), C_GREY);
  }
  value = constrain(value, 0.0f, 100.0f);
  // aiguille épaisse (3 lignes) + moyeu + pointe lumineuse
  float ang = PI - (value / 100.0f) * PI;
  int nx = cx + (int)(cosf(ang) * (r - 26)), ny = cy - (int)(sinf(ang) * (r - 26));
  if (fxFull()) fxHalo(nx, ny, 7, C_WHITE, C_BG, 110, 2);
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
  // aiguille à ressort : part de 0 et dépasse légèrement avant de se poser
  float gv = fngValue >= 0 ? fngValue : 50;
  if (fxFull()) gv = gv * easeOutBack((long)(fxT - pageEnterMs) / 1400.0f);
  drawGauge(110, 170, 92, gv);
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
  if (nodeOnline && fxFull()) {                       // ping radar
    fxRing(252, 74, 5, 11, (fxT % 1800) / 1800.0f, C_GREEN, C_PANEL);
    fxRing(252, 74, 5, 11, ((fxT + 900) % 1800) / 1800.0f, C_GREEN, C_PANEL);
  }
  bool dotOn = nodeOnline || !fxFull() || (fxT / 500) % 2 == 0;   // hors ligne : clignote
  gfx->fillCircle(252, 74, 5, dotOn ? (nodeOnline ? C_GREEN : C_RED) : C_PANEL);
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
float    cubeSpin = 0;                // V5 : rotation lente permanente (lacet)

// projection 3D -> 2D isométrique (+ tilt pendant le drag, + rotation lente)
void cubeProj(float x, float y, float z, int &sx, int &sy) {
  float c = cosf(cubeTilt + cubeSpin), s = sinf(cubeTilt + cubeSpin);
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
      // couleur par couche : braise en bas -> jaune en haut (+ scintillement)
      uint8_t lay = p.slot / 36;
      uint16_t pc = mix565(C_ORANGE_D, C_YELLOW, (uint8_t)(lay * 46));
      if (fxFull() && ((p.slot * 7 + fxT / 90) % 23) == 0) pc = C_WHITE;
      gfx->fillRect(px, py, 2, 2, pc);
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

  // ---- rotation lente permanente (MAX) ----
  static unsigned long lastSpin = 0;
  if (fxFull()) { float dt = lastSpin ? min(0.1f, (now - lastSpin) / 1000.0f) : 0; cubeSpin += dt * 0.32f; if (cubeSpin > 2 * PI) cubeSpin -= 2 * PI; }
  lastSpin = now;

  // ---- sol isométrique (grille qui tourne avec le cube, reste en place au drag) ----
  {
    float c = cosf(cubeSpin), sn = sinf(cubeSpin);
    auto fp = [&](float x, float y, int &sx, int &sy) {
      float xr = x * c - y * sn, yr = x * sn + y * c;
      sx = (int)(CUBE_CX + (xr - yr) * 0.866f); sy = (int)(CUBE_CY + (xr + yr) * 0.433f + 2);
    };
    for (int g = -4; g <= 4; g++) {
      int ax, ay, bx, by;
      uint8_t k = (uint8_t)(70 - abs(g) * 12);
      fp(g * 22.0f, -88, ax, ay); fp(g * 22.0f, 88, bx, by); gfx->drawLine(ax, ay, bx, by, mix565(C_BG, C_ORANGE_D, k));
      fp(-88, g * 22.0f, ax, ay); fp(88, g * 22.0f, bx, by); gfx->drawLine(ax, ay, bx, by, mix565(C_BG, C_ORANGE_D, k));
    }
    // ombre douce sous le cube
    if (cubeState == CS_FILL) fxHalo(CUBE_CX, CUBE_CY + 4, 34, C_ORANGE_D, C_BG, 70, 4);
  }

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
  gfx->setCursor(12, 36); gfx->print("POOLS WAR - blocs mines sur 7 jours");
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
    float prog = target > 0 ? clamp01(poolShown[i] / target) : 1.0f;
    // médaille de rang (or / argent / bronze)
    const uint16_t medal[3] = {C_YELLOW, 0xC618, 0xCB00};
    uint16_t mc = i < 3 ? medal[i] : C_PANEL;
    if (i == 0 && fxFull()) fxHalo(17, y + 10, 12, C_YELLOW, C_BG, (uint8_t)(40 + 50 * fxPulse(1800)), 3);
    gfx->fillCircle(17, y + 10, 8, mc);
    gfx->setTextSize(1); gfx->setTextColor(i < 3 ? C_BG : C_GREY);
    gfx->setCursor(15, y + 7); gfx->print(i + 1);
    if (i == 0) {                                           // couronne
      gfx->fillTriangle(10, y - 3, 12, y - 8, 14, y - 3, C_YELLOW);
      gfx->fillTriangle(14, y - 3, 17, y - 10, 20, y - 3, C_YELLOW);
      gfx->fillTriangle(20, y - 3, 22, y - 8, 24, y - 3, C_YELLOW);
    }
    // nom (16 chars max)
    char nm[17]; strlcpy(nm, names[i], sizeof(nm));
    gfx->setTextColor(i == 0 ? C_ORANGE : C_WHITE);
    gfx->setCursor(32, y + 6); gfx->print(nm);
    // piste + barre : reflet haut, dégradé, bande lumineuse qui défile
    gfx->fillRoundRect(140, y, 272, 20, 5, C_PANEL);
    if (w > 8) {
      uint16_t bc = POOL_COL[i];
      gfx->fillRoundRect(140, y, w, 20, 5, mix565(bc, C_BG, 60));
      gfx->fillRoundRect(140, y, w, 11, 5, bc);
      gfx->drawFastHLine(144, y + 2, max(0, w - 8), mix565(bc, C_WHITE, 120));
      if (fxFull()) {
        int sx = 140 + (int)(((fxT + i * 260) % 2600) / 2600.0f * (w + 60)) - 30;
        for (int k = 0; k < 18; k++) {
          int xx = sx + k;
          if (xx < 143 || xx > 140 + w - 4) continue;
          gfx->drawFastVLine(xx, y + 2, 16, mix565(bc, C_WHITE, (uint8_t)(130 - abs(k - 9) * 14)));
        }
      }
      // tête de course lumineuse
      gfx->fillCircle(140 + w - 4, y + 10, 3, mix565(bc, C_WHITE, 150));
    }
    // nb blocs + part (compteur qui monte avec la barre)
    gfx->setTextColor(C_GREY);
    gfx->setCursor(420, y + 6);
    int share = total > 0 ? (int)(blocks[i] * 100 / total) : 0;
    gfx->printf("%ld %d%%", (long)(blocks[i] * prog + 0.5f), (int)(share * prog + 0.5f));
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

// ---- mini réseau Lightning animé : nœuds, canaux, paiements routés ----
#define LN_NN 14
#define LN_NE 24
#define LN_NP 8
struct LnNode { int16_t x, y; unsigned long hitMs; };
struct LnPay  { int8_t e; bool fwd; float p, v; };
LnNode  lnN[LN_NN];
uint8_t lnE[LN_NE][2];
int     lnNE = 0;
LnPay   lnP[LN_NP];
bool    lnInit = false;
uint32_t lnRnd = 0x1234567;
int lnRand(int n) { lnRnd = lnRnd * 1664525u + 1013904223u; return (int)((lnRnd >> 8) % (uint32_t)n); }

void lnNetInit() {
  for (int i = 0; i < LN_NN; i++) {                  // grille 5x3 bruitée (x 232..462, y 38..118)
    int gx = i % 5, gy = i / 5;
    lnN[i].x = 238 + gx * 55 + lnRand(22) - 11;
    lnN[i].y = 44 + gy * 34 + lnRand(16) - 8 + (gx % 2) * 8;
    lnN[i].hitMs = 0;
  }
  lnNE = 0;
  for (int i = 0; i < LN_NN && lnNE < LN_NE; i++) {   // chaque nœud -> ses 2 plus proches voisins
    for (int pass = 0; pass < 2; pass++) {
      int best = -1; long bd = 1L << 30;
      for (int j = 0; j < LN_NN; j++) {
        if (j == i) continue;
        bool dup = false;
        for (int e = 0; e < lnNE; e++) if ((lnE[e][0] == i && lnE[e][1] == j) || (lnE[e][0] == j && lnE[e][1] == i)) dup = true;
        if (dup) continue;
        long dx = lnN[i].x - lnN[j].x, dy = lnN[i].y - lnN[j].y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = j; }
      }
      if (best >= 0 && lnNE < LN_NE) { lnE[lnNE][0] = i; lnE[lnNE][1] = best; lnNE++; }
    }
  }
  for (int k = 0; k < LN_NP; k++) { lnP[k].e = lnRand(lnNE); lnP[k].fwd = lnRand(2); lnP[k].p = lnRand(100) / 100.0f; lnP[k].v = 0.7f + lnRand(80) / 100.0f; }
  lnInit = true;
}

void lnNetDraw() {
  if (!lnInit) lnNetInit();
  static unsigned long last = 0;
  float dt = (fxFull() && last) ? min(0.1f, (fxT - last) / 1000.0f) : 0; last = fxT;
  for (int e = 0; e < lnNE; e++)
    gfx->drawLine(lnN[lnE[e][0]].x, lnN[lnE[e][0]].y, lnN[lnE[e][1]].x, lnN[lnE[e][1]].y, mix565(C_BG, C_YELLOW, 38));
  for (int k = 0; k < LN_NP; k++) {
    LnPay &p = lnP[k];
    p.p += p.v * dt;
    if (p.p >= 1.0f) {                                  // arrivé : le nœud s'allume, on route au saut suivant
      int end = p.fwd ? lnE[p.e][1] : lnE[p.e][0];
      lnN[end].hitMs = fxT;
      int cand[LN_NE], nc = 0;
      for (int e = 0; e < lnNE; e++) if ((lnE[e][0] == end || lnE[e][1] == end) && e != p.e) cand[nc++] = e;
      if (nc) { p.e = cand[lnRand(nc)]; p.fwd = (lnE[p.e][0] == end); } else p.fwd = !p.fwd;
      p.p = 0;
    }
    const LnNode &a = lnN[p.fwd ? lnE[p.e][0] : lnE[p.e][1]], &b = lnN[p.fwd ? lnE[p.e][1] : lnE[p.e][0]];
    for (int t = 3; t >= 0; t--) {                      // traînée
      float q = max(0.0f, p.p - t * 0.06f);
      int x = a.x + (int)((b.x - a.x) * q), y = a.y + (int)((b.y - a.y) * q);
      if (t == 0) { gfx->fillCircle(x, y, 2, C_YELLOW); gfx->drawPixel(x, y, C_WHITE); }
      else gfx->fillRect(x, y, 2, 2, mix565(C_BG, C_YELLOW, (uint8_t)(200 - t * 55)));
    }
  }
  for (int i = 0; i < LN_NN; i++) {
    unsigned long age = fxT - lnN[i].hitMs;
    if (lnN[i].hitMs && age < 450) {
      float k = age / 450.0f;
      fxRing(lnN[i].x, lnN[i].y, 3, 11, k, C_YELLOW, C_BG);
      gfx->fillCircle(lnN[i].x, lnN[i].y, 3, mix565(C_YELLOW, C_ORANGE, u8f(k)));
    } else gfx->fillCircle(lnN[i].x, lnN[i].y, 3, mix565(C_BG, C_ORANGE, 170));
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

  // réseau animé derrière l'éclair
  lnNetDraw();
  // éclair stylé : halo pulsant + polygone plein + ombre portée + contour + reflet
  static const int8_t bolt[7][2] = {{18,0},{40,0},{24,29},{35,29},{5,71},{13,37},{3,37}};
  const int bx = 330, by = 44;
  if (fxFull()) fxHalo(bx + 21, by + 36, 44, C_YELLOW, C_BG, (uint8_t)(45 + 55 * fxPulse(1300)), 4);
  bool flick = fxFull() && ((fxT / 60) % 41 == 0 || (fxT / 60) % 41 == 2);   // éclat bref
  auto px = [&](int i) { return bx + bolt[i][0]; };
  auto py = [&](int i) { return by + bolt[i][1]; };
  for (int i = 1; i < 6; i++)                       // ombre portée
    gfx->fillTriangle(px(0)+3, py(0)+3, px(i)+3, py(i)+3, px(i+1)+3, py(i+1)+3, C_ORANGE_D);
  for (int i = 1; i < 6; i++)                       // corps jaune (blanc pendant l'éclat)
    gfx->fillTriangle(px(0), py(0), px(i), py(i), px(i+1), py(i+1), flick ? C_WHITE : C_YELLOW);
  for (int i = 0; i < 7; i++)                       // contour orange
    gfx->drawLine(px(i), py(i), px((i+1)%7), py((i+1)%7), C_ORANGE);
  gfx->drawLine(px(6), py(6), px(0), py(0), C_WHITE); // reflets
  gfx->drawLine(px(0), py(0), px(1), py(1), C_WHITE);

  // tuiles stats
  // compteurs qui montent à l'arrivée sur la page
  float ke = fxEnter(1100);
  drawLnTile(24, 132, "CHANNELS", prettyNum((long)(lnChannels * ke)), NULL);
  drawLnTile(252, 132, "NODES", prettyNum((long)(lnNodes * ke)), NULL);
  drawLnTile(24, 212, "CAPACITE MOYENNE", prettyNum((long)(lnAvgCap * ke)), "sats");
  drawLnTile(252, 212, "FEE RATE MOYEN", String((int)(lnAvgFeePpm * ke)), "ppm");
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
    // λ : rythme mesuré sur l'époque de difficulté si dispo (x5 plus stable),
    // sinon moyenne des 6 derniers blocs
    long lambda = lambdaEpoch > 0 ? lambdaEpoch : constrain((ts[0] - ts[n - 1]) / (n - 1), 60L, 3600L);
    long el = max(0L, (long)nowT - ts[0]);
    long rem = lambda - el;
    char est[16];
    long ar = rem >= 0 ? rem : -rem;
    snprintf(est, sizeof(est), "%ld:%02ld", ar / 60, ar % 60);
    int ew = drawSmooth(20, 66, est, rem >= 0 ? C_GREEN : C_ORANGE, C_PANEL);
    gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
    gfx->setCursor(24 + ew, 92); gfx->print(rem >= 0 ? "min est." : "min retard");
    fxBar(20, 116, 196, 8, (float)el / lambda, rem >= 0 ? C_ORANGE : C_RED, C_BG);
    gfx->setTextSize(1); gfx->setTextColor(C_GREY);
    gfx->setCursor(20, 132);
    if (lambdaEpoch > 0) gfx->printf("rythme epoque ~%ld:%02ld /bloc", lambda / 60, lambda % 60);
    else gfx->printf("rythme reel ~%ld min (%d blocs)", lambda / 60, n);
    // probabilités Poisson : P(au moins 1 bloc) dans 1 / 5 / 10 min
    gfx->setCursor(20, 150); gfx->print("P(bloc) :  1m    5m    10m");
    const int pm[3] = {60, 300, 600};
    for (int i = 0; i < 3; i++) {
      float pr = 1.0f - expf(-(float)pm[i] / lambda);
      int bw = (int)(44 * pr * fxEnter(800, i * 150));
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
  struct tm t = gTm;
  if (gTmOk) {
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
  gfx->setTextColor(C_GREY); gfx->setCursor(20, 204); gfx->print("CYCLES DE FEES - 24 h apprises");
  gfx->setTextColor(C_DGREY); gfx->setCursor(220, 204); gfx->print("(barre orange = maintenant)");
  if (gTmOk && feeSamples > 40) {
    float day[24]; float mx = 1;
    portENTER_CRITICAL(&dataMux);
    for (int i = 0; i < 24; i++) { day[i] = feeBkt[t.tm_wday * 24 + i]; if (day[i] > mx) mx = day[i]; }
    portEXIT_CRITICAL(&dataMux);
    if (mx > 1) {
      for (int i = 0; i < 24; i++) {
        int bh = day[i] > 0 ? (int)(40 * day[i] / mx * fxEnter(700, i * 25)) : 2;
        if (bh < 2) bh = 2;
        uint16_t bc = i == t.tm_hour ? mix565(C_ORANGE, C_YELLOW, u8f(fxFull() ? fxPulse(1200) : 0)) : C_DGREY;
        gfx->fillRect(15 + i * 19, 272 - bh, 14, bh, bc);
      }
      gfx->drawFastHLine(15, 272, 24 * 19 - 5, C_GREY);
      gfx->setTextColor(C_DGREY); gfx->setTextSize(1);
      gfx->setCursor(15, 278); gfx->print("0h");
      gfx->setCursor(15 + 11 * 19, 278); gfx->print("11h");
      gfx->setCursor(15 + 22 * 19, 278); gfx->print("22h");
    } else {
      gfx->setTextColor(C_GREY); gfx->setCursor(20, 240); gfx->print("pas encore de donnees pour ce jour");
    }
  } else {
    gfx->setTextColor(C_GREY); gfx->setCursor(20, 240);
    gfx->printf("apprentissage en cours (%ld echantillons)...", feeSamples);
  }
}

// signaux calibrés (momentum TTM / squeeze, fréquences historiques mesurées)
// FIX : ce fichier existait mais n'était inclus NULLE PART -> page Signaux v2 morte
#include "signals.h"

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
    for (int i = 0; i < 3; i++) {
      int bh = (int)((8 + i * 8) * fxEnter(600, i * 120));
      gfx->fillRoundRect(170 + i * 26, 86 - i * 8, 20, 8 + i * 8, 3, C_BG);
      if (i < force && bh > 2) gfx->fillRoundRect(170 + i * 26, 94 - bh, 20, bh, 3, dc);
    }
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

  // ---------- panneau 2 : compression calibrée (TTM squeeze, pctl 120 j) ----------
  drawPanelSqueezeCal(318, 48);

  // ---------- panneau 3 : direction calibrée (fréquences historiques 2017-) ----------
  drawPanelDirectionCal(10, 172);

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
//  PAGE 8 — BTC DOOM v3 : moteur façon Doom (voir doom.h / doom_assets.h)
//  Plein écran, textures / sols / plafonds, portes, clés, 3 niveaux,
//  4 monstres originaux, pistolet + fusil à pompe, automap, melt.
// =====================================================================
#include "doom.h"

// =====================================================================
//  CUBE ISOMÉTRIQUE (cinématique nouveau bloc, splash)
//  (cx, cy) = centre de la face du dessus, a = arête en pixels
// =====================================================================
void fxIsoCube(int cx, int cy, int a, uint16_t cTop, uint16_t cL, uint16_t cR, bool edges = true) {
  int hw = (int)(a * 0.866f), hh = a / 2;
  int Tx = cx, Ty = cy - hh, Rx = cx + hw, Ry = cy, Bx = cx, By = cy + hh, Lx = cx - hw, Ly = cy;
  gfx->fillTriangle(Tx, Ty, Rx, Ry, Bx, By, cTop);
  gfx->fillTriangle(Tx, Ty, Lx, Ly, Bx, By, cTop);
  gfx->fillTriangle(Lx, Ly, Bx, By, Bx, By + a, cL);
  gfx->fillTriangle(Lx, Ly, Lx, Ly + a, Bx, By + a, cL);
  gfx->fillTriangle(Bx, By, Rx, Ry, Rx, Ry + a, cR);
  gfx->fillTriangle(Bx, By, Bx, By + a, Rx, Ry + a, cR);
  if (edges) {
    uint16_t e = mix565(cTop, C_WHITE, 150);
    gfx->drawLine(Lx, Ly, Tx, Ty, e); gfx->drawLine(Tx, Ty, Rx, Ry, e);
    gfx->drawLine(Lx, Ly, Bx, By, mix565(cTop, C_WHITE, 70));
    gfx->drawLine(Bx, By, Rx, Ry, mix565(cTop, C_WHITE, 70));
    gfx->drawFastVLine(Bx, By, a, mix565(cL, C_WHITE, 60));
  }
}
// chaîne horizontale : maillons alternés
void fxChainH(int x0, int x1, int y, uint16_t c) {
  int i = 0;
  for (int x = x0; x + 10 <= x1; x += 9, i++) {
    if (i % 2 == 0) gfx->drawRoundRect(x, y - 3, 12, 7, 3, c);
    else            gfx->drawRoundRect(x + 2, y - 1, 8, 3, 1, c);
  }
}

// =====================================================================
//  CINÉMATIQUE NOUVEAU BLOC (pages autres que Cube / Doom)
//  Remplace l'ancien stroboscope orange/noir (5 flashs plein écran) :
//  flash doux -> le bloc tombe et rebondit -> onde de choc + gerbe de
//  particules -> il s'accroche à la chaîne -> hauteur / pool -> fondu.
// =====================================================================
#define NB_DUR 3300
#define NB_NP  44
struct NbPart { float x, y, vx, vy; uint16_t c; };
NbPart nbP[NB_NP];
bool   nbBurst = false;

void drawBlockAnim() {
  long e = (long)(millis() - animStart);     // signé : (e - 520) < 0 avant l'apparition
  if (e > NB_DUR) { animNewBlock = false; needRedraw = true; pageEnterMs = millis(); return; }
  char pool[32]; long btx; snapLastBlock(pool, sizeof(pool), &btx);

  if (!fxAny()) {                       // animations coupées : carte fixe, sans strobo
    gfx->fillScreen(C_BG);
    textCenter("NEW BLOCK", 100, 4, C_ORANGE);
    textCenter(String(blockHeight), 160, 3, C_WHITE);
    textCenter(pool, 210, 2, C_GREY);
    gfx->flush();
    return;
  }
  fxT = millis();
  // ---- fond : flash doux orange qui retombe (un seul, pas de stroboscope) ----
  float kf = clamp01(e / 260.0f);
  uint16_t bgc = mix565(C_BG, C_ORANGE, (uint8_t)(170 * (1 - kf) * (1 - kf)));
  gfx->fillScreen(bgc);
  const int CX = 240, CY = 118, A = 44;          // face du dessus du nouveau bloc
  const int GROUND = CY + A / 2 + A;             // bas du cube posé
  // halo derrière le bloc (sur la couleur de fond courante)
  fxHalo(CX, CY + A / 2, 70, C_ORANGE, bgc, (uint8_t)(55 + 25 * fxPulse(700)), 5);

  // ---- ondes de choc à l'impact ----
  for (int i = 0; i < 3; i++) {
    long ei = (long)e - 240 - i * 170;
    if (ei > 0) fxRing(CX, GROUND - 10, 20, 320, ei / 900.0f, i == 0 ? C_YELLOW : C_ORANGE, bgc);
  }

  // ---- blocs précédents (chaîne) qui glissent depuis la gauche ----
  float ks = easeOutCubic(e / 600.0f);
  int shift = (int)(-70 * (1 - ks));
  uint16_t dTop = mix565(C_BG, C_ORANGE, 120), dL = mix565(C_BG, C_ORANGE_D, 200), dR = mix565(C_BG, C_ORANGE_D, 120);
  fxIsoCube(CX - 240 + shift, CY, A, dTop, dL, dR);
  fxIsoCube(CX - 120 + shift, CY, A, dTop, dL, dR);
  fxChainH(CX - 240 + shift + 38, CX - 120 + shift - 38, CY + A / 2 + A / 2, C_GREY);
  // maillon qui relie le nouveau bloc (apparaît après l'impact)
  if (e > 620) {
    int x0 = CX - 120 + 38, x1 = CX - 38;
    int xr = x0 + (int)((x1 - x0) * easeOutCubic((e - 620) / 380.0f));
    fxChainH(x0, xr, CY + A / 2 + A / 2, mix565(C_GREY, C_ORANGE, u8f(1 - (e - 620) / 800.0f)));
  }

  // ---- le nouveau bloc tombe et rebondit ----
  float kd = easeOutBounce(e / 650.0f);
  int cy = -90 + (int)((CY + 90) * kd);
  fxIsoCube(CX, cy, A, mix565(C_ORANGE, C_YELLOW, 110), C_ORANGE, C_ORANGE_D);
  // petit "B" gravé sur la face gauche
  gfx->setTextSize(2); gfx->setTextColor(mix565(C_ORANGE, C_WHITE, 170));
  gfx->setCursor(CX - 26, cy + 22); gfx->print("B");

  // ---- gerbe de particules au premier impact ----
  if (e >= 236 && !nbBurst) {
    nbBurst = true;
    for (int i = 0; i < NB_NP; i++) {
      float ang = PI + (random(0, 1000) / 1000.0f) * PI;         // vers le haut
      float sp = 2.0f + random(0, 400) / 100.0f;
      nbP[i].x = CX + random(-30, 31); nbP[i].y = GROUND - 4;
      nbP[i].vx = cosf(ang) * sp * 1.3f; nbP[i].vy = sinf(ang) * sp;
      nbP[i].c = (i % 3 == 0) ? C_YELLOW : (i % 3 == 1 ? C_ORANGE : C_WHITE);
    }
  }
  if (nbBurst && e < 1800) {
    float fade = clamp01((e - 236) / 1500.0f);
    for (int i = 0; i < NB_NP; i++) {
      NbPart &p = nbP[i];
      p.x += p.vx; p.y += p.vy; p.vy += 0.22f; p.vx *= 0.985f;
      if (p.y > GROUND + 60) continue;
      gfx->fillRect((int)p.x, (int)p.y, 3, 3, mix565(p.c, C_BG, u8f(fade)));
    }
  }

  // ---- titre machine à écrire ----
  const char *title = "NEW BLOCK";
  int nch = min(9, (int)(e / 45));
  char tb[12]; strlcpy(tb, title, nch + 1);
  gfx->setTextSize(4); gfx->setTextColor(C_ORANGE);
  gfx->setCursor((SCR_W - 9 * 24) / 2, 22); gfx->print(tb);
  if (nch < 9 && (e / 90) % 2 == 0) gfx->fillRect((SCR_W - 9 * 24) / 2 + nch * 24, 22, 4, 28, C_YELLOW);
  int ulw = (int)(9 * 24 * easeOutCubic((e - 300) / 600.0f));
  if (ulw > 0) gfx->fillRect(CX - ulw / 2, 54, ulw, 2, C_YELLOW);

  // ---- hauteur (chiffres lissés) + pool, en fondu ----
  char hs[16]; snprintf(hs, sizeof(hs), "%ld", blockHeight);
  float kh = clamp01((e - 520) / 400.0f);
  if (kh > 0) {
    int hw = smoothWidth(hs);
    drawSmooth(CX - hw / 2, 202, hs, mix565(C_BG, C_WHITE, u8f(kh)), C_BG);
  }
  float kp = clamp01((e - 850) / 400.0f);
  if (kp > 0) {
    char line[48]; snprintf(line, sizeof(line), "mine par %s", pool);
    textCenter(line, 256, 2, mix565(C_BG, C_ORANGE, u8f(kp)));
    if (btx > 0) {
      snprintf(line, sizeof(line), "%s transactions", prettyNum(btx).c_str());
      textCenter(line, 280, 1, mix565(C_BG, C_GREY, u8f(kp)));
    }
  }
  // ---- fondu de sortie ----
  if (e > NB_DUR - 380) fxDimAll(u8f((e - (NB_DUR - 380)) / 380.0f));
  gfx->flush();
}

// dessine la page courante dans le canvas (sans flush)
void renderPage() {
  if (page == PG_DOOM) { drawPageDoom(); return; }     // plein écran (barre de statut du jeu)
  gfx->fillScreen(C_BG);
  if (page != PG_CUBE && page != PG_DOOM) fxDrawDust();
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
}

void drawCurrentPage() {
  fxT = millis();
  renderPage();
  gfx->flush();
}

// =====================================================================
//  TRANSITION GLISSÉE ENTRE PAGES
//  Canvas en rotation 1 : chaque COLONNE logique (x) est une ligne native
//  contiguë de 320 pixels -> un glissement horizontal = 1 memcpy par
//  colonne (~270 px, la zone de contenu). Header/footer restent fixes.
//  dir = +1 : la nouvelle page arrive par la droite ; -1 : par la gauche.
// =====================================================================
void fxSlide(int dir) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || !fxPrevFb || !fxNextFb || !fxAny()) { drawCurrentPage(); return; }
  memcpy(fxPrevFb, fb, FB_PIX * 2);          // ancien écran (dernier rendu)
  fxT = millis(); pageEnterMs = fxT;
  renderPage();                               // nouvelle page dans le canvas
  memcpy(fxNextFb, fb, FB_PIX * 2);
  const int I0 = PANEL_W - NAV_Y;             // index natif de y = NAV_Y-1
  const int CN = NAV_Y - 27;                  // lignes logiques 27..NAV_Y-1
  const unsigned long DUR = 320;
  unsigned long t0 = millis();
  for (;;) {
    float k = clamp01((millis() - t0) / (float)DUR);
    int off = (int)(easeOutCubic(k) * SCR_W);
    for (int x = 0; x < SCR_W; x++) {
      int sx = dir > 0 ? x + off : x - off;
      const uint16_t *src;
      if (sx >= 0 && sx < SCR_W) src = fxPrevFb + sx * PANEL_W;
      else src = fxNextFb + (dir > 0 ? sx - SCR_W : sx + SCR_W) * PANEL_W;
      memcpy(fb + x * PANEL_W + I0, src + I0, CN * 2);
    }
    int seam = dir > 0 ? SCR_W - off : off;   // couture lumineuse
    if (seam > 1 && seam < SCR_W - 2) {
      gfx->drawFastVLine(seam, 27, CN, C_ORANGE);
      gfx->drawFastVLine(seam - dir, 27, CN, mix565(C_BG, C_ORANGE, 110));
      gfx->drawFastVLine(seam - 2 * dir, 27, CN, mix565(C_BG, C_ORANGE, 45));
    }
    fxT = millis();
    gfx->fillRect(0, NAV_Y, SCR_W, SCR_H - NAV_Y, C_BG);   // soulignement qui glisse
    drawFooter();
    gfx->flush();
    if (k >= 1.0f) break;
  }
}

// changement de page centralisé (tap onglet, swipe, sortie Doom)
void gotoPage(int np, int dir) {
  if (np == page) return;
  int from = page;
  page = np;
  if (page == PG_POOLS) poolsReset = true;
  if (page == PG_AI || page == PG_SIG) requestFetch(REQ_KL30);
  uint16_t *fb = gfx->getFramebuffer();
  if ((np == PG_DOOM || from == PG_DOOM) && fb && fxPrevFb && fxNextFb && fxAny()) {
    // entrée / sortie de DOOM : écran qui "fond" comme dans le jeu original
    memcpy(fxPrevFb, fb, FB_PIX * 2);
    fxT = millis(); pageEnterMs = fxT;
    renderPage();
    memcpy(fxNextFb, fb, FB_PIX * 2);
    fxMeltRun();
  } else fxSlide(dir);
  pageEnterMs = millis();
  lastDrawMs = millis(); needRedraw = false;
}

// =====================================================================
//  SETUP — boot non bloquant : l'UI s'affiche tout de suite avec des
//  états "chargement...", REQ_ALL est poussé dans netTask qui fetchera.
// =====================================================================
// splash de démarrage animé (logo, halo, ondes, reflet, barre de progression)
void bootSplash(const char *msg, float prog) {
  fxT = millis();
  gfx->fillScreen(C_BG);
  const int cx = SCR_W / 2, cy = 132;
  fxHalo(cx, cy, 64, C_ORANGE, C_BG, (uint8_t)(45 + 45 * fxPulse(1600)), 4);
  for (int i = 0; i < 3; i++) fxRing(cx, cy, 52, 170, ((fxT + i * 700) % 2100) / 2100.0f, C_ORANGE, C_BG);
  gfx->draw16bitRGBBitmapWithTranColor(cx - 48, cy - 48, (uint16_t*)BTC_LOGO_96, TRANSP, 96, 96);
  fxShineBitmap(cx - 48, cy - 48, BTC_LOGO_96, 96, 96, TRANSP, (fxT % 2600) / 1300.0f, 16, 150);
  textCenter("BLOCK CLOCK", 206, 3, C_ORANGE);
  if (prog >= 0) fxBar(140, 244, 200, 6, prog, C_ORANGE, C_PANEL);
  else {                                               // indéterminé : segment qui va-et-vient
    gfx->fillRoundRect(140, 244, 200, 6, 3, C_PANEL);
    int x = 140 + (int)(150 * fxPulse(1400));
    gfx->fillRoundRect(x, 244, 50, 6, 3, C_ORANGE);
  }
  textCenter(msg, 260, 1, C_GREY);
  textCenter("V5 - by silexperience", 300, 1, C_DGREY);
  gfx->flush();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== BITCOIN BLOCK CLOCK V5 ===");

  ledcSetup(0, 5000, 10);
  ledcAttachPin(PIN_BL, 0);
  setBacklight(70);

  gfx->begin();
  // buffers des transitions glissées (2 x 300 Ko en PSRAM) ; sans eux : coupe franche
  fxPrevFb = (uint16_t*)ps_malloc(FB_PIX * 2);
  fxNextFb = (uint16_t*)ps_malloc(FB_PIX * 2);
  if (!fxPrevFb || !fxNextFb) Serial.println("[FX] PSRAM insuffisante : transitions desactivees");
  bootSplash("demarrage...", 0.05f);

  Wire.begin(TP_SDA, TP_SCL);
  Wire.setClock(400000);
  audioInit();
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);

  loadConfig();
  bootSplash("configuration chargee", 0.2f);
  // FIX : aucun WiFi enregistré -> portail tout de suite (avant : 20 s d'attente)
  if (cfg_ssid.length() == 0) startConfigPortal();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(cfg_ssid.c_str(), cfg_pass.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    char m[48]; snprintf(m, sizeof(m), "connexion a %s...", cfg_ssid.c_str());
    bootSplash(m, 0.2f + 0.8f * (millis() - t0) / 20000.0f);
    delay(20);
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
    s += "IP : <input name='nodeip' value='" + htmlEsc(cfg_nodeip) + "' style='width:100%;padding:10px;margin:6px 0;background:#161b22;color:#eee;border:1px solid #444;border-radius:8px'>";
    s += "<h2 style='color:#F7931A'>Animations</h2><select name='anim' style='width:100%;padding:10px;margin:6px 0;background:#161b22;color:#eee;border:1px solid #444;border-radius:8px'>";
    s += String("<option value='2'") + (animLevel == 2 ? " selected" : "") + ">MAX - tout est anime (~25 FPS)</option>";
    s += String("<option value='1'") + (animLevel == 1 ? " selected" : "") + ">ECO - transitions + nouveau bloc</option>";
    s += String("<option value='0'") + (animLevel == 0 ? " selected" : "") + ">OFF - statique</option></select>";
    s += "<p style='color:#888;font-size:13px'>MAX repasse en ECO automatiquement la nuit (23h-7h).</p>";
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
    if (server.hasArg("anim")) animLevel = (uint8_t)constrain((int)server.arg("anim").toInt(), 0, 2);
    saveConfig();
    latchHi = latchLo = false; needRedraw = true;
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.on("/reset", HTTP_GET, []() {
    prefs.begin("bc", false); prefs.clear(); prefs.end();
    server.send(200, "text/html", "<meta charset='utf-8'>WiFi oubli&eacute; - red&eacute;marrage&hellip;");
    delay(800); ESP.restart();
  });
  server.begin();

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");

  // ---------- tâches FreeRTOS ----------
  sndQ = xQueueCreate(12, sizeof(SndNote));
  sndTxtQ = xQueueCreate(4, sizeof(SndTxt));
  xTaskCreatePinnedToCore(sndTask, "snd", 20480, NULL, 2, NULL, 0);   // sons + TTS (TLS+MP3 = gros stack)
  xTaskCreatePinnedToCore(netTask, "net", 16384, NULL, 1, NULL, 0);   // tous les fetchs HTTP (TLS + JSON + signaux)

  // boot non bloquant : tout fetcher en tâche de fond, UI immédiate
  requestFetch(REQ_ALL);
  blockDetectedMs = millis();

  bootSplash("WiFi OK - synchronisation...", 1.0f);
  delay(250);
  pageEnterMs = millis();
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
#if DEBUG_WM
  static unsigned long tWm = 0;
  if (millis() - tWm > 5000) { tWm = millis();
    Serial.printf("[WM] loop free=%u\n", uxTaskGetStackHighWaterMark(NULL)); }
#endif
  // pendant la charge : forcer le redraw pour animer le clignotement de la jauge
  static unsigned long tBatBlink = 0;
  if (batCharging && !sleeping && millis() - tBatBlink > 800) {
    tBatBlink = millis();
    needRedraw = true;
  }

  // ---------- mode nuit + tick minute (horloge du header) ----------
  refreshClock();                      // non bloquant (cache gTm pour tout le rendu)
  struct tm t = gTm;
  if (gTmOk) {
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
      if (page == PG_DOOM) {
        // DOOM plein écran : seul le ✕ compte (les autres touches sont gérées
        // par le jeu : sticks, FIRE, MAP, armes) — pas d'onglets ni de swipe
        if (downX >= GAME_EX && downX <= GAME_EX + GAME_EW && downY >= GAME_EY && downY <= GAME_EY + GAME_EH) {
          beep(900, 60, 30); gotoPage(PG_PRICE, -1); lastActionMs = millis();
        }
      }
      // icône HP (header) : tap = cycle volume 100 -> 60 -> 30 -> muet
      else if (!touchMoved && downY < 26 && downX >= 340 && downX <= 372) {
        sndVolPct = (sndVolPct > 60) ? 60 : (sndVolPct > 30) ? 30 : (sndVolPct > 0) ? 0 : 100;
        saveConfig();
        needRedraw = true; lastActionMs = now;
        if (sndVolPct > 0) beep(1200, 60, 40);   // feedback au nouveau niveau
      }
      // barre d'onglets : tap sur une icône = accès direct à la page
      else if (!touchMoved && downY >= NAV_Y) {
        int tab = constrain((int)downX / TAB_W, 0, (int)PG_COUNT - 1);
        if (tab != page) {
          beep(1000, 50, 25);
          gotoPage(tab, tab > page ? 1 : -1);
          lastActionMs = millis();
        }
      }
      else if (abs(dx) > 50 && abs(dy) < 100) {
        // swipe horizontal : page suivante / précédente (glissement animé)
        beep(1000, 50, 25);
        if (dx < 0) gotoPage((page + 1) % PG_COUNT, 1);
        else        gotoPage((page + PG_COUNT - 1) % PG_COUNT, -1);
        lastActionMs = millis();
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
        // tap global : refresh à la demande. REQ_ALL au plus 1×/20 s
        // (sinon rafale d'appels CoinGecko -> HTTP 429 sur l'API gratuite)
        static unsigned long lastFullReq = 0;
        beep(1500, 50, 30); lastActionMs = now;
        if (lastFullReq == 0 || now - lastFullReq > 20000) { lastFullReq = now; requestFetch(REQ_ALL); }
        else requestFetch(REQ_PRICE | REQ_HEIGHT);
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
      else if (page == PG_DOOM) {                    // pas d'interruption en jeu
        snprintf(dmPopup, sizeof(dmPopup), "NEW BLOCK %ld", blockHeight); dmPopupMs = now;
      }
      else { animNewBlock = true; animStart = now; nbBurst = false; }   // cinématique
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
  if (animNewBlock) { drawBlockAnim(); delay(10); return; }

  // prix : cible, flash vert/rouge au changement, interpolation douce
  {
    static float lastTarget = 0; static uint8_t lastCur = 255;
    float target = btcPrice[curCur];
    if (curCur != lastCur) { lastCur = curCur; lastTarget = target; dispPrice = target; }
    else if (target > 0 && target != lastTarget) {
      if (lastTarget > 0) { priceFlashMs = now; priceFlashUp = target > lastTarget; }
      lastTarget = target;
    }
    if (dispPrice <= 0 && target > 0) dispPrice = target;   // 1re valeur : directe
  }

  // ---------- redraw ----------
  // Les redraws sont différés tant que le doigt est posé (sauf jeux) :
  // un swipe n'est jamais perdu pendant un flush (~60 ms).
  if (page == PG_CUBE || page == PG_DOOM) {
    // pages animées : ~30 FPS permanent (le jeu doit tourner même doigt posé)
    if (now - lastDrawMs >= 33) { lastDrawMs = now; needRedraw = false; drawCurrentPage(); }
  } else if (fxFull()) {
    // MAX : toutes les pages sont vivantes (~25 FPS)
    if (!touched && now - lastDrawMs >= FX_FRAME_MS) {
      if (page == PG_PRICE) stepDispPrice();
      lastDrawMs = now; needRedraw = false; drawCurrentPage();
    }
  } else if (page == PG_POOLS && (long)(poolsAnimUntil - now) > 0) {
    // course des barres : ~30 FPS le temps de converger
    if (!touched && now - lastDrawMs >= 33) { lastDrawMs = now; drawCurrentPage(); }
  } else if (page == PG_CHAIN || page == PG_AI) {
    // chronos "il y a Xs" / prochain bloc : tick 1 Hz
    if (!touched && (needRedraw || now - lastDrawMs >= 1000)) { lastDrawMs = now; needRedraw = false; drawCurrentPage(); }
  } else if (page == PG_PRICE) {
    // prix animé : interpolation douce vers la cible (~30 FPS pendant la transition)
    if (fabsf(dispPrice - btcPrice[curCur]) >= 0.5f || (fxAny() && now - priceFlashMs < 1400)) {
      if (!touched && now - lastDrawMs >= 33) { stepDispPrice(); lastDrawMs = now; needRedraw = false; drawCurrentPage(); }
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
