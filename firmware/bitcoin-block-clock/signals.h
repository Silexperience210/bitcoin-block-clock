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
static uint32_t sgLastDay = 0;     // jour UTC (epoch/86400) de la derniere bougie (en cours)

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
  float volD;             // volatilite journaliere EWMA(0.94) (log-rendement)
  float closeL;           // derniere cloture journaliere (USDT)
};
// ---- suivi de ses propres previsions (fourchette 80 % a 7 j) ----------
struct VolPred { uint32_t day; float lo, hi; };
#define VP_N 8
static VolPred vpQ[VP_N]; static int vpN = 0;
static uint16_t vpHit = 0, vpTot = 0; static uint32_t vpLastDay = 0;
static void vpLoad() {
  Preferences p;
  if (!p.begin("vp", true)) return;
  vpN = constrain((int)p.getUChar("n", 0), 0, VP_N);
  if (p.getBytes("q", vpQ, sizeof(vpQ)) != sizeof(vpQ)) vpN = 0;
  vpHit = p.getUShort("hit", 0); vpTot = p.getUShort("tot", 0); vpLastDay = p.getULong("last", 0);
  p.end();
}
static void vpSave() {                     // NVS thread-safe ; handle local (pas le prefs global)
  Preferences p;
  if (!p.begin("vp", false)) return;
  p.putUChar("n", (uint8_t)vpN); p.putBytes("q", vpQ, sizeof(vpQ));
  p.putUShort("hit", vpHit); p.putUShort("tot", vpTot); p.putULong("last", vpLastDay);
  p.end();
}
static SigState sig = {};

// ======================================================================
//  FETCH — streaming. Klines Binance = [[t,"o","h","l","c","v",...],...]
//  On extrait les champs 2,3,4 (h,l,c) de chaque kline, en flux.
// ======================================================================
static bool fetchKlinesStream(const char *interval, int limit,
                              float *H, float *L, float *C, int *N, int cap, double *lastOpenMs = nullptr) {
  WiFiClientSecure cl; cl.setInsecure(); cl.setTimeout(12000);
  if (!cl.connect("data-api.binance.vision", 443)) return false;
  // HTTP/1.0 : jamais de réponse "chunked" -> les tailles de chunk en hexa
  // ne peuvent pas s'intercaler dans les nombres lus par le parseur streaming
  cl.printf("GET /api/v3/klines?symbol=BTCUSDT&interval=%s&limit=%d HTTP/1.0\r\n"
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
  float hh = 0, ll = 0, cc = 0; double ot = 0, lastOt = 0;
  auto flush = [&]() {
    if (ni == 0) return;
    num[ni] = 0; float v = atof(num);
    if (field == 0) ot = strtod(num, nullptr);         // double : 13 chiffres (ms)
    ni = 0;
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
          if (n < cap && cc > 0) { H[n] = hh; L[n] = ll; C[n] = cc; n++; lastOt = ot; } }
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
  if (lastOpenMs) *lastOpenMs = lastOt;
  return true;
}

static bool fetchSignals() {                       // appeler depuis netTask
  // static : ~4,6 Ko hors de la pile de netTask (appelée uniquement par elle)
  static float tH[SIG_ND], tL[SIG_ND], tC[SIG_ND];
  static float uH[SIG_NW], uL[SIG_NW], uC[SIG_NW];
  int tn = 0, un = 0;
  double lastOpen = 0;
  bool ok1 = fetchKlinesStream("1d", SIG_ND, tH, tL, tC, &tn, SIG_ND, &lastOpen);
  bool ok2 = fetchKlinesStream("1w", SIG_NW, uH, uL, uC, &un, SIG_NW);
  if (!ok1 || !ok2) return false;
  portENTER_CRITICAL(&dataMux);
  memcpy(sgH, tH, tn * 4); memcpy(sgL, tL, tn * 4); memcpy(sgC, tC, tn * 4); sgN = tn;
  memcpy(swH, uH, un * 4); memcpy(swL, uL, un * 4); memcpy(swC, uC, un * 4); swN = un;
  sgLastDay = (uint32_t)(lastOpen / 86400000.0);          // jour UTC de la bougie EN COURS
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
  // momentum TTM : linreg 20 de la SÉRIE (c - (midDonchian+SMA)/2).
  // FIX parité : le milieu est recalculé pour CHAQUE barre j (comme la
  // calibration Python / Pine) ; l'ancienne version le figeait à la barre i
  // -> même tercile de force que la calibration seulement 81 % du temps.
  float yv[20];
  for (int k = 0; k < 20; k++) {
    int j = i - 19 + k;
    if (j < 19) { yv[k] = 0; continue; }
    float mj = 0, hh = h[j - 19], ll = l[j - 19];
    for (int q = j - 19; q <= j; q++) { mj += c[q]; if (h[q] > hh) hh = h[q]; if (l[q] < ll) ll = l[q]; }
    yv[k] = c[j] - ((hh + ll) / 2.0f + mj / 20.0f) / 2.0f;
  }
  float xm = 9.5f, ym = 0, num = 0, den = 0;
  for (int k = 0; k < 20; k++) ym += yv[k];
  ym /= 20.0f;
  for (int k = 0; k < 20; k++) {
    float x = k - xm, y = yv[k] - ym;
    num += x * y; den += x * x;
  }
  float b = num / den, a0 = ym - b * xm;
  *momOut = (a0 + b * 19.0f) / c[i] * 100.0f;
}

static void computeSignals() {
  // static : sinon ~4,6 Ko de plus empilés PAR-DESSUS fetchSignals()
  // (~9 Ko au total sur une pile netTask de 12 Ko -> débordement)
  static float h[SIG_ND], l[SIG_ND], c[SIG_ND];
  static float wh[SIG_NW], wl[SIG_NW], wc[SIG_NW];
  int n, wn;
  portENTER_CRITICAL(&dataMux);
  n = sgN; memcpy(h, sgH, n * 4); memcpy(l, sgL, n * 4); memcpy(c, sgC, n * 4);
  wn = swN; memcpy(wh, swH, wn * 4); memcpy(wl, swL, wn * 4); memcpy(wc, swC, wn * 4);
  portEXIT_CRITICAL(&dataMux);
  if (n < 222 || wn < 30) { sig.ready = false; return; }
  // FIX parité : la calibration n'utilise que des bougies CLOSES ; la dernière
  // bougie Binance est le jour EN COURS -> on travaille sur L = n - 2
  const int L = n - 2;

  SigState s = {};
  bool dummySq; float dummyM;
  // percentile : BBW des 120 jours precedents vs BBW courant
  float bbwNow; ttmCore(h, l, c, n, L, &s.sqD, &bbwNow, &s.momPct);
  s.bbw = bbwNow;
  int below = 0, tot = 0;
  for (int i = L - 120; i < L; i++) {
    float bw; ttmCore(h, l, c, n, i, &dummySq, &bw, &dummyM);
    if (bw < bbwNow) below++;
    tot++;
  }
  s.bbwPctl = tot ? 100.0f * below / tot : 50.0f;
  // duree du squeeze daily
  s.durD = s.sqD ? 1 : 0;
  for (int i = L - 1; s.durD && i > 60; i--) {
    bool q; float bw, mm; ttmCore(h, l, c, n, i, &q, &bw, &mm);
    if (q) s.durD++; else break;
  }
  // squeeze weekly : sur la DERNIERE SEMAINE CLOSE (parite calibration,
  // la semaine en cours n'est pas terminee -> pas de lookahead)
  float bwW, momW;
  ttmCore(wh, wl, wc, wn, wn - 2, &s.sqW, &bwW, &momW);
  // regime 90j
  float roc90 = c[L] / c[L - 90] - 1.0f;
  s.regimeOk = (s.momPct >= 0) == (roc90 >= 0);
  // tercile momentum (seuils calibres 30j — vue large)
  float am = fabsf(s.momPct);
  s.momTier = am >= CAL30_MOM_T2 ? 2 : (am >= CAL30_MOM_T1 ? 1 : 0);

  // ---- choix des lignes calibrees (du contexte le plus specifique au moins)
  int pb = s.bbwPctl < 5 ? 0 : s.bbwPctl < 10 ? 1 : s.bbwPctl < 20 ? 2 : 3;
  s.exp30 = CAL30_PCTL[pb];
  // (le contexte "squeeze D+W" n'a que 6 cas indépendants à 7 j : on ne
  //  l'utilise pour la direction que s'il atteint 20 cas)
  if (s.sqD && s.sqW && CAL7_STACK.nEff >= 20) { s.dir7 = CAL7_STACK;  s.dir30 = CAL30_STACK;  s.dirCtx = "squeeze D+W"; }
  else if (s.momTier == 2 && s.regimeOk) { s.dir7 = CAL7_CONFL;  s.dir30 = CAL30_CONFL;  s.dirCtx = "mom fort + regime"; }
  else                                   { s.dir7 = CAL7_MOM[s.momTier];
                                           s.dir30 = CAL30_MOM[s.momTier]; s.dirCtx = "momentum"; }
  // ---- volatilité EWMA(0.94) des log-rendements journaliers (parité calibration)
  {
    double v = 0;
    for (int t = 1; t <= 30; t++) { double r = log((double)c[t] / c[t - 1]); v += r * r; }
    v /= 30.0;
    for (int t = 31; t <= L; t++) { double r = log((double)c[t] / c[t - 1]); v = VOL_LAMBDA * v + (1.0 - VOL_LAMBDA) * r * r; }
    s.volD = (float)sqrt(v);
  }
  s.closeL = c[L];
  s.ready = true;
  // ---- suivi : chaque jour, on note la fourchette 80 % à 7 j ; à J+7 on vérifie
  uint32_t dayL = sgLastDay > 0 ? sgLastDay - 1 : 0;       // jour de la bougie L
  if (dayL > 19000) {                                       // horodatage valide (> 2022)
    bool dirty = false;
    for (int k = 0; k < vpN; ) {
      uint32_t tgt = vpQ[k].day + 7;
      if (tgt <= dayL) {
        int idx = L - (int)(dayL - tgt);
        if (idx >= 0 && idx <= L) { vpTot++; if (c[idx] >= vpQ[k].lo && c[idx] <= vpQ[k].hi) vpHit++; }
        vpQ[k] = vpQ[--vpN]; dirty = true;
      } else k++;
    }
    if (dayL > vpLastDay && vpN < VP_N) {
      float sH = s.volD * sqrtf(7.0f);
      vpQ[vpN++] = {dayL, c[L] * expf(VOL_Q[1][0] * sH), c[L] * expf(VOL_Q[1][4] * sH)};
      vpLastDay = dayL; dirty = true;
    }
    if (dirty) vpSave();
  }
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
  gfx->setTextSize(1); gfx->setTextColor(C_GREY); gfx->setCursor(X + 10, Y + 8); gfx->print("COMPRESSION");
  SigState s; portENTER_CRITICAL(&dataMux); s = sig; portEXIT_CRITICAL(&dataMux);
  if (!s.ready) { gfx->setCursor(X + 10, Y + 52); gfx->print("calcul..."); return; }
  bool deep = s.bbwPctl < 10;
  char p[12]; snprintf(p, sizeof(p), "%.0f", s.bbwPctl);
  // (V5 : chiffres remontés, ils chevauchaient la jauge)
  drawSmooth(X + 10, Y + 14, p, deep ? C_ORANGE : C_WHITE, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 14 + smoothWidth(p), Y + 40); gfx->print("pctl 120j");
  // jauge percentile (inversee : gauche = comprime)
  gfx->fillRoundRect(X + 10, Y + 62, 132, 8, 4, C_BG);
  int bw = (int)(132 * (100.0f - s.bbwPctl) / 100.0f * fxEnter(900));
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
  gfx->printf("P(>10%%/30j) %d%% %dcas", s.exp30.p10, s.exp30.nEff);   // cas INDÉPENDANTS
}

static void drawPanelDirectionCal(int X, int Y) {        // 460 x 64
  gfx->fillRoundRect(X, Y, 460, 64, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY); gfx->setCursor(X + 10, Y + 8);
  gfx->print("DIRECTION (2017-, intervalle de confiance 95%)");
  SigState s; portENTER_CRITICAL(&dataMux); s = sig; portEXIT_CRITICAL(&dataMux);
  if (!s.ready) { gfx->setCursor(X + 10, Y + 34); gfx->print("calcul..."); return; }
  bool up = s.momPct >= 0;
  uint16_t cm = up ? C_GREEN : C_RED;
  drawArrow(X + 12, Y + 24, up, cm);
  char m[16]; snprintf(m, sizeof(m), "%s%.1f%%", up ? "+" : "", s.momPct);
  gfx->setTextSize(2); gfx->setTextColor(cm);
  gfx->setCursor(X + 30, Y + 22); gfx->print(m);
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 36 + (int)strlen(m) * 12, Y + 26); gfx->print("mom TTM");
  gfx->setCursor(X + 10, Y + 44); gfx->print(s.dirCtx);
  if (s.regimeOk) { gfx->setCursor(X + 10, Y + 54); gfx->print("regime 90j aligne"); }
  // Pour chaque horizon : l'INTERVALLE DE CONFIANCE est dessiné sur une
  // échelle 0-100 %. S'il traverse la ligne des 50 % -> pas d'avantage mesurable.
  const CalRow *rows[2] = { &s.dir7, &s.dir30 };
  const char *labs[2] = { "7j", "30j" };
  for (int i = 0; i < 2; i++) {
    const CalRow &r = *rows[i];
    int bx = X + 190 + i * 135;
    const int TX = bx + 24, TW = 100;                       // piste 0..100 %
    gfx->setTextColor(C_GREY); gfx->setCursor(bx, Y + 20); gfx->print(labs[i]);
    bool signif = r.ciLo > 50 || r.ciHi < 50;
    bool follow = r.pDir >= 50;                              // le prix suit-il le momentum ?
    bool dirUp = follow ? up : !up;
    int pp = follow ? r.pDir : 100 - r.pDir;
    uint16_t col = signif ? (dirUp ? C_GREEN : C_RED) : C_DGREY;
    gfx->fillRoundRect(TX, Y + 21, TW, 8, 3, C_BG);
    int x0 = TX + r.ciLo * TW / 100, x1 = TX + r.ciHi * TW / 100;
    int w = max(3, (int)((x1 - x0) * fxEnter(900)));
    gfx->fillRoundRect(x0, Y + 22, w, 6, 3, signif ? col : mix565(C_BG, C_GREY, 120));
    gfx->fillCircle(TX + r.pDir * TW / 100, Y + 25, 3, signif ? C_WHITE : C_GREY);
    gfx->drawFastVLine(TX + TW / 2, Y + 17, 16, C_ORANGE);  // hasard (50 %)
    gfx->setCursor(TX, Y + 36);
    if (!signif) { gfx->setTextColor(C_GREY); gfx->print("pas d'avantage"); }
    else { gfx->setTextColor(col); gfx->printf("%d%% %s", pp, dirUp ? "hausse" : "baisse"); }
    gfx->setTextColor(C_DGREY);
    gfx->setCursor(TX, Y + 48); gfx->printf("%d-%d%% %dcas", r.ciLo, r.ciHi, r.nEff);
  }
}

// Amplitude prévue : volatilité EWMA + quantiles EMPIRIQUES (queues épaisses,
// asymétrie) calibrés hors-échantillon ; fourchettes 50 % et 80 % à 7 / 30 j
static void volBand(const SigState &s, int hi, float *out5) {   // out5 = q10..q90 en % de variation
  float sH = s.volD * sqrtf((float)VOL_H[hi]);
  for (int k = 0; k < 5; k++) out5[k] = (expf(VOL_Q[hi][k] * sH) - 1.0f) * 100.0f;
}
static void drawPanelAmplitude(int X, int Y) {           // 296 x 116
  gfx->fillRoundRect(X, Y, 296, 116, 8, C_PANEL);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY); gfx->setCursor(X + 10, Y + 8);
  gfx->print("AMPLITUDE PREVUE");
  gfx->setTextColor(C_DGREY); gfx->setCursor(X + 112, Y + 8); gfx->print("fourchettes 50% / 80%");
  SigState s; portENTER_CRITICAL(&dataMux); s = sig; portEXIT_CRITICAL(&dataMux);
  if (!s.ready || s.volD <= 0) { gfx->setCursor(X + 10, Y + 52); gfx->print("calcul..."); return; }
  float b7[5], b30[5];
  volBand(s, 1, b7); volBand(s, 2, b30);
  float R = max(fabsf(b30[0]), fabsf(b30[4])) * 1.12f;      // échelle commune
  if (R < 5) R = 5;
  const int TX = X + 54, TW = 232, CX = TX + TW / 2;
  auto px = [&](float pct) { return CX + (int)(pct / R * (TW / 2)); };
  const float *rowsB[2] = { b7, b30 };
  const char *lab[2] = { "7j", "30j" };
  float ke = fxEnter(1000);
  for (int i = 0; i < 2; i++) {
    const float *b = rowsB[i];
    int y = Y + 26 + i * 34;
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE); gfx->setCursor(X + 10, y - 2); gfx->print(lab[i]);
    gfx->fillRoundRect(TX, y, TW, 12, 4, C_BG);
    int a80 = px(b[0] * ke), z80 = px(b[4] * ke), a50 = px(b[1] * ke), z50 = px(b[3] * ke);
    gfx->fillRoundRect(a80, y + 1, max(2, z80 - a80), 10, 4, mix565(C_PANEL, C_ORANGE, 90));
    gfx->fillRoundRect(a50, y + 1, max(2, z50 - a50), 10, 4, C_ORANGE);
    gfx->drawFastVLine(CX, y - 3, 18, C_GREY);                             // aujourd'hui (0 %)
    gfx->fillRect(px(b[2] * ke), y + 1, 2, 10, C_WHITE);                  // médiane
    gfx->setTextSize(1); gfx->setTextColor(C_RED);
    gfx->setCursor(max(TX, a80 - 12), y + 15); gfx->printf("%.0f%%", b[0]);
    gfx->setTextColor(C_GREEN);
    char hs[10]; snprintf(hs, sizeof(hs), "+%.0f%%", b[4]);
    gfx->setCursor(min(TX + TW - (int)strlen(hs) * 6, z80 - 6), y + 15); gfx->print(hs);
  }
  gfx->setTextSize(1); gfx->setTextColor(C_DGREY);
  gfx->setCursor(X + 10, Y + 94);
  gfx->printf("vol. annualisee %.0f%%  (EWMA, base USDT)", s.volD * sqrtf(365.0f) * 100.0f);
  gfx->setCursor(X + 10, Y + 105);
  if (vpTot > 0) {
    gfx->setTextColor(vpHit * 100 >= vpTot * 70 ? C_GREY : C_ORANGE);
    gfx->printf("fourchette 7j tenue %u fois sur %u (vise 80%%)", vpHit, vpTot);
  } else gfx->printf("auto-controle : 1re verification a J+7");
}
