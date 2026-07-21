// =====================================================================
//  signals.h — Momentum & direction CALIBRES (vue large 7j/30j)
//  ------------------------------------------------------------------
//  Donnees : data-api.binance.vision (miroir public officiel Binance,
//            sans cle, PAS geo-bloque — api.binance.com renvoie 451
//            dans certains pays). 2 requetes : 1d x300, 1w x84.
//  Parseur : streaming char-a-char (zero gros buffer, zero ArduinoJson
//            pour ces payloads ~55KB — on n'alloue que 3x300 floats).
//  Calcul  : TTM Squeeze (Bollinger 20/2 DANS Keltner 20/1.5xATR),
//            percentile BBW 120j, duree, momentum TTM (linreg 20j de
//            close - (midDonchian+SMA)/2), regime ROC90, squeeze hebdo.
//  Sortie  : frequences historiques via signals_calib.h (genere par
//            tools/calibrate_squeeze.py). Pas de ML, pas de promesse :
//            "sur N cas similaires depuis 2017, X% ont fait ceci".
//
//  Integration (voir PATCH-SIGNAUX.md) :
//    netTask : if (req & REQ_SIGNALS) fetchSignals();   // toutes les 4h
//    UI      : drawPanelSqueezeCal() + drawPanelDirectionCal()
// =====================================================================
#pragma once
#include <WiFiClientSecure.h>
#include "signals_calib.h"

// prototypes (signals.h est un header : pas d'auto-prototypage Arduino)
static void computeSignals();
static bool fetchSignals();

#define SIG_ND 300          // jours  (>= 120 pctl + 20 warmup + 90 roc)
#define SIG_NW 84           // semaines (>= 20 warmup + marge)

// ---- stockage brut (rempli par netTask, protege par dataMux) ----------
static float sgH[SIG_ND], sgL[SIG_ND], sgC[SIG_ND]; static int sgN = 0;
static float swH[SIG_NW], swL[SIG_NW], swC[SIG_NW]; static int swN = 0;

// ---- etat calcule (snapshot pour l'UI) --------------------------------
struct SigState {
  bool  ready;
  float bbw;        // largeur Bollinger courante (%)
  float bbwPctl;    // percentile 0..100 sur 120j (plus bas = plus comprime)
  bool  sqD, sqW;   // TTM squeeze on : daily / weekly (semaine close)
  int   durD;       // jours consecutifs en squeeze daily
  float momPct;     // momentum TTM en % du prix (signe = biais)
  int   momTier;    // 0 faible / 1 moyen / 2 fort (seuils calibres)
  bool  regimeOk;   // signe(mom) == signe(ROC90)
  // lignes calibrees choisies pour l'etat courant :
  CalRow dir7, dir30;     // meilleure ligne DIRECTION applicable
  CalRow exp30;           // ligne EXPANSION (percentile BBW)
  const char *dirCtx;     // label du contexte utilise ("stack", "mom+regime"...)
};
static SigState sig = {};

// ======================================================================
//  FETCH — streaming. Klines Binance = [[t,"o","h","l","c","v",...],...]
//  On extrait les champs 2,3,4 (h,l,c) de chaque kline, en flux.
// ======================================================================
static bool fetchKlinesStream(const char *interval, int limit,
                              float *H, float *L, float *C, int *N, int cap) {
  WiFiClientSecure cl; cl.setInsecure(); cl.setTimeout(12000);
  if (!cl.connect("data-api.binance.vision", 443)) return false;
  cl.printf("GET /api/v3/klines?symbol=BTCUSDT&interval=%s&limit=%d HTTP/1.1\r\n"
            "Host: data-api.binance.vision\r\nConnection: close\r\n\r\n",
            interval, limit);
  // saute les headers HTTP
  unsigned long t0 = millis();
  while (cl.connected() && millis() - t0 < 12000) {
    String h = cl.readStringUntil('\n');
    if (h == "\r" || h.length() <= 1) break;
  }
  // parseur : depth 1=tableau racine, 2=une kline ; field = index de champ
  int depth = 0, field = 0, n = 0;
  char num[24]; int ni = 0; bool inStr = false;
  float hh = 0, ll = 0, cc = 0;
  auto flush = [&]() {
    if (ni == 0) return;
    num[ni] = 0; float v = atof(num); ni = 0;
    if      (field == 2) hh = v;
    else if (field == 3) ll = v;
    else if (field == 4) cc = v;
  };
  t0 = millis();
  while ((cl.connected() || cl.available()) && millis() - t0 < 20000) {
    if (!cl.available()) { delay(2); continue; }
    char ch = cl.read();
    if (inStr) {
      if (ch == '"') { inStr = false; flush(); }
      else if (ni < 23) num[ni++] = ch;
      continue;
    }
    switch (ch) {
      case '"': inStr = true; break;
      case '[': depth++; if (depth == 2) { field = 0; ni = 0; hh = ll = cc = 0; } break;
      case ']':
        if (depth == 2) { flush();
          if (n < cap && cc > 0) { H[n] = hh; L[n] = ll; C[n] = cc; n++; } }
        depth--; if (depth <= 0) goto done;
        break;
      case ',': flush(); if (depth == 2) field++; break;
      default:
        if (depth == 2 && ni < 23 &&
            ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' || ch == 'e' || ch == 'E'))
          num[ni++] = ch;
    }
  }
done:
  cl.stop();
  if (n < 30) return false;
  *N = n;
  return true;
}

static bool fetchSignals() {                       // appeler depuis netTask
  float tH[SIG_ND], tL[SIG_ND], tC[SIG_ND]; int tn = 0;
  float uH[SIG_NW], uL[SIG_NW], uC[SIG_NW]; int un = 0;
  bool ok1 = fetchKlinesStream("1d", SIG_ND, tH, tL, tC, &tn, SIG_ND);
  bool ok2 = fetchKlinesStream("1w", SIG_NW, uH, uL, uC, &un, SIG_NW);
  if (!ok1 || !ok2) return false;
  portENTER_CRITICAL(&dataMux);
  memcpy(sgH, tH, tn * 4); memcpy(sgL, tL, tn * 4); memcpy(sgC, tC, tn * 4); sgN = tn;
  memcpy(swH, uH, un * 4); memcpy(swL, uL, un * 4); memcpy(swC, uC, un * 4); swN = un;
  portEXIT_CRITICAL(&dataMux);
  computeSignals();
  return true;
}

// ======================================================================
//  CALCUL — memes formules que tools/calibrate_squeeze.py (parite exacte)
// ======================================================================
static void ttmCore(const float *h, const float *l, const float *c, int n, int i,
                    bool *sqOn, float *bbwOut, float *momOut) {
  // fenetres 20 finissant en i (i >= 20 requis pour l'ATR "warm")
  float ma = 0; for (int k = i - 19; k <= i; k++) ma += c[k]; ma /= 20.0f;
  float s2 = 0; for (int k = i - 19; k <= i; k++) s2 += (c[k] - ma) * (c[k] - ma);
  float sd = sqrtf(s2 / 20.0f);
  // EMA20 + ATR20 (EMA du TR) initialisees 40 barres avant pour converger
  int st = i - 60 < 1 ? 1 : i - 60;
  float e = c[st - 1], a = h[st - 1] - l[st - 1];
  const float al = 2.0f / 21.0f;
  for (int k = st; k <= i; k++) {
    float tr = h[k] - l[k];
    float d1 = fabsf(h[k] - c[k - 1]); if (d1 > tr) tr = d1;
    float d2 = fabsf(l[k] - c[k - 1]); if (d2 > tr) tr = d2;
    e = al * c[k] + (1 - al) * e;
    a = al * tr   + (1 - al) * a;
  }
  *sqOn  = (ma + 2 * sd < e + 1.5f * a) && (ma - 2 * sd > e - 1.5f * a);
  *bbwOut = 4.0f * sd / ma * 100.0f;
  // momentum TTM : linreg 20 de (c - (midDonchian+SMA)/2), valeur au dernier pt
  float hh = h[i - 19], ll = l[i - 19];
  for (int k = i - 19; k <= i; k++) { if (h[k] > hh) hh = h[k]; if (l[k] < ll) ll = l[k]; }
  float mid = ((hh + ll) / 2.0f + ma) / 2.0f;
  float xm = 9.5f, ym = 0, num = 0, den = 0;
  for (int k = 0; k < 20; k++) ym += c[i - 19 + k] - mid;
  ym /= 20.0f;
  for (int k = 0; k < 20; k++) {
    float x = k - xm, y = (c[i - 19 + k] - mid) - ym;
    num += x * y; den += x * x;
  }
  float b = num / den, a0 = ym - b * xm;
  *momOut = (a0 + b * 19.0f) / c[i] * 100.0f;
}

static void computeSignals() {
  float h[SIG_ND], l[SIG_ND], c[SIG_ND]; int n;
  float wh[SIG_NW], wl[SIG_NW], wc[SIG_NW]; int wn;
  portENTER_CRITICAL(&dataMux);
  n = sgN; memcpy(h, sgH, n * 4); memcpy(l, sgL, n * 4); memcpy(c, sgC, n * 4);
  wn = swN; memcpy(wh, swH, wn * 4); memcpy(wl, swL, wn * 4); memcpy(wc, swC, wn * 4);
  portEXIT_CRITICAL(&dataMux);
  if (n < 220 || wn < 30) { sig.ready = false; return; }

  SigState s = {};
  bool dummySq; float dummyM;
  // percentile : BBW des 120 jours precedents vs BBW courant
  float bbwNow; ttmCore(h, l, c, n, n - 1, &s.sqD, &bbwNow, &s.momPct);
  s.bbw = bbwNow;
  int below = 0, tot = 0;
  for (int i = n - 121; i < n - 1; i++) {
    float bw; ttmCore(h, l, c, n, i, &dummySq, &bw, &dummyM);
    if (bw < bbwNow) below++;
    tot++;
  }
  s.bbwPctl = tot ? 100.0f * below / tot : 50.0f;
  // duree du squeeze daily
  s.durD = s.sqD ? 1 : 0;
  for (int i = n - 2; s.durD && i > 60; i--) {
    bool q; float bw, mm; ttmCore(h, l, c, n, i, &q, &bw, &mm);
    if (q) s.durD++; else break;
  }
  // squeeze weekly : sur la DERNIERE SEMAINE CLOSE (parite calibration,
  // la semaine en cours n'est pas terminee -> pas de lookahead)
  float bwW, momW;
  ttmCore(wh, wl, wc, wn, wn - 2, &s.sqW, &bwW, &momW);
  // regime 90j
  float roc90 = c[n - 1] / c[n - 91] - 1.0f;
  s.regimeOk = (s.momPct >= 0) == (roc90 >= 0);
  // tercile momentum (seuils calibres 30j — vue large)
  float am = fabsf(s.momPct);
  s.momTier = am >= CAL30_MOM_T2 ? 2 : (am >= CAL30_MOM_T1 ? 1 : 0);

  // ---- choix des lignes calibrees (du contexte le plus specifique au moins)
  int pb = s.bbwPctl < 5 ? 0 : s.bbwPctl < 10 ? 1 : s.bbwPctl < 20 ? 2 : 3;
  s.exp30 = CAL30_PCTL[pb];
  if (s.sqD && s.sqW)                    { s.dir7 = CAL7_STACK;  s.dir30 = CAL30_STACK;  s.dirCtx = "squeeze D+W"; }
  else if (s.momTier == 2 && s.regimeOk) { s.dir7 = CAL7_CONFL;  s.dir30 = CAL30_CONFL;  s.dirCtx = "mom fort + regime"; }
  else                                   { s.dir7 = CAL7_MOM[s.momTier];
                                           s.dir30 = CAL30_MOM[s.momTier]; s.dirCtx = "momentum"; }
  s.ready = true;
  portENTER_CRITICAL(&dataMux);
  sig = s;
  portEXIT_CRITICAL(&dataMux);
}

// ======================================================================
//  UI — deux panneaux prets pour la page Signaux (style maison).
//  Remplacent "SQUEEZE 30J" et completent "INDICATEUR TECHNIQUE".
// ======================================================================
static void drawPanelSqueezeCal(int X, int Y) {          // 152 x 116
  gfx->fillRoundRect(X, Y, 152, 116, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(X + 10, Y + 8); gfx->print("COMPRESSION");
  SigState s; portENTER_CRITICAL(&dataMux); s = sig; portEXIT_CRITICAL(&dataMux);
  if (!s.ready) { gfx->setCursor(X + 10, Y + 52); gfx->print("calcul..."); return; }
  bool deep = s.bbwPctl < 10;
  char p[12]; snprintf(p, sizeof(p), "%.0f", s.bbwPctl);
  drawSmooth(X + 10, Y + 22, p, deep ? C_ORANGE : C_WHITE, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 14 + smoothWidth(p), Y + 48); gfx->print("pctl 120j");
  // jauge percentile (inversee : gauche = comprime)
  gfx->fillRoundRect(X + 10, Y + 62, 132, 8, 4, C_BG);
  int bw = (int)(132 * (100.0f - s.bbwPctl) / 100.0f);
  if (bw > 5) gfx->fillRoundRect(X + 10, Y + 62, bw, 8, 4, deep ? C_ORANGE : C_DGREY);
  // pastilles stack D / W + duree
  gfx->fillCircle(X + 16, Y + 84, 5, s.sqD ? C_ORANGE : C_BG);
  gfx->fillCircle(X + 34, Y + 84, 5, s.sqW ? C_ORANGE : C_BG);
  gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 12, Y + 94); gfx->print("D");
  gfx->setCursor(X + 30, Y + 94); gfx->print("W");
  gfx->setTextColor(s.sqD ? C_ORANGE : C_GREY);
  gfx->setCursor(X + 52, Y + 82);
  if (s.sqD) gfx->printf("%dj de squeeze", s.durD);
  else gfx->print("pas de squeeze");
  gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 10, Y + 104);
  gfx->printf("P(>10%%/30j) %d%% n=%d", s.exp30.p10, s.exp30.n);
}

static void drawPanelDirectionCal(int X, int Y) {        // 460 x 64
  gfx->fillRoundRect(X, Y, 460, 64, 8, C_PANEL);
  gfx->setTextColor(C_GREY); gfx->setCursor(X + 10, Y + 8); gfx->print("DIRECTION (frequences historiques 2017-, pas une promesse)");
  SigState s; portENTER_CRITICAL(&dataMux); s = sig; portEXIT_CRITICAL(&dataMux);
  if (!s.ready) { gfx->setCursor(X + 10, Y + 34); gfx->print("calcul..."); return; }
  bool up = s.momPct >= 0;
  uint16_t cd = up ? C_GREEN : C_RED;
  // fleche + momentum
  drawArrow(X + 14, Y + 30, up, cd);
  char m[16]; snprintf(m, sizeof(m), "%s%.1f%%", up ? "+" : "", s.momPct);
  int mw = drawSmooth(X + 34, Y + 20, m, cd, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 38 + mw, Y + 46); gfx->print("mom TTM");
  // probas calibrees 7j / 30j : barres + n
  const CalRow *rows[2] = { &s.dir7, &s.dir30 };
  const char *labs[2] = { "7j", "30j" };
  for (int i = 0; i < 2; i++) {
    int bx = X + 190 + i * 135;
    gfx->setTextColor(C_GREY);  gfx->setCursor(bx, Y + 20); gfx->print(labs[i]);
    int pd = rows[i]->pDir;
    gfx->fillRoundRect(bx + 24, Y + 20, 80, 10, 4, C_BG);
    // 50% = neutre : on ne remplit que l'EDGE au-dela du hasard
    int e = pd - 50; if (e < 0) e = 0;
    if (e > 1) gfx->fillRoundRect(bx + 24 + 40, Y + 20, (int)(40 * e / 25.0f), 10, 4, cd);
    gfx->drawFastVLine(bx + 64, Y + 18, 14, C_DGREY);   // repere 50%
    if (pd <= 52 && pd >= 48) {                          // zone de bruit : neutre
      gfx->setTextColor(C_GREY);
      gfx->setCursor(bx + 24, Y + 36); gfx->print("neutre");
    } else {
      gfx->setTextColor(cd);
      gfx->setCursor(bx + 24, Y + 36); gfx->printf("%d%% %s", pd, up ? "hausse" : "baisse");
    }
    gfx->setTextColor(C_DGREY);
    gfx->setCursor(bx + 24, Y + 48); gfx->printf("n=%d", rows[i]->n);
  }
  gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 10, Y + 56); gfx->printf("contexte: %s%s", s.dirCtx, s.regimeOk ? " (regime 90j aligne)" : "");
}
