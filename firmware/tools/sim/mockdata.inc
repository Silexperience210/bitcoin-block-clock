// Données d'exemple réalistes (proches des mockups du dépôt)
static float rw(float &v, float vol) { v *= 1.0f + ((random(-1000, 1001) / 1000.0f) * vol); return v; }
void seedData() {
  randomSeed(42);
  btcPrice[CUR_EUR] = 97432; btcPrice[CUR_USD] = 105880; btcPrice[CUR_CHF] = 91210;
  btcChg24 = 2.37f; dispPrice = 0;
  blockHeight = 912345; mempoolCount = 12450;
  feeFast = 18; feeHalf = 12; feeHour = 9; feeEco = 4;
  time_t nowT = time(nullptr);
  for (int i = 0; i < 6; i++) blkTs[i] = (long)nowT - 252 - i * 560;
  blkTsN = 6;
  strcpy(lastPool, "Foundry USA"); lastBlockTx = 3412;
  whaleBtc = 312.5f; strcpy(whaleTxid, "a3f9c2d1");
  fngValue = 72; strcpy(fngLabel, "Greed");
  diffRemaining = 1034; diffChange = 3.42f;
  const char *pn[6] = {"Foundry USA", "AntPool", "ViaBTC", "F2Pool", "SpiderPool", "MARA Pool"};
  long pb[6] = {312, 187, 141, 98, 81, 57};
  poolsN = 6; poolsTotal = 1008; for (int i = 0; i < 6; i++) { strcpy(poolNames[i], pn[i]); poolBlocks[i] = pb[i]; }
  poolsDataAt = millis();
  lnCapBtc = 4987.4f; lnChannels = 49231; lnNodes = 13542; lnAvgCap = 10131540; lnAvgFeePpm = 612;
  nodeOnline = true;
  // klines : marche aléatoire par (devise, timeframe)
  for (int c = 0; c < CUR_COUNT; c++) for (int t = 0; t < TF_COUNT; t++) {
    int n = TF_TARGET[t]; float v = btcPrice[c] * (t == TF_30J ? 0.86f : t == TF_7J ? 0.95f : 0.985f);
    float mn = 1e12, mx = 0;
    for (int i = 0; i < n; i++) {
      float drift = (btcPrice[c] - v) / (n - i + 1) * 0.6f;
      v += drift; rw(v, t == TF_30J ? 0.012f : 0.004f);
      closesC[c][t][i] = v; tsC[c][t][i] = (long)nowT - (long)(n - i) * (t == TF_1H ? 300 : t == TF_24H ? 1440 : t == TF_7J ? 7200 : 28800);
      mn = fminf(mn, v); mx = fmaxf(mx, v);
    }
    closesC[c][t][n - 1] = btcPrice[c];
    nPtsC[c][t] = n; mnC[c][t] = mn; mxC[c][t] = fmaxf(mx, btcPrice[c]);
  }
  chartCacheLoad();
  for (int i = 0; i < 32; i++) feeHist[i] = 14 + (i % 7) + i / 6;
  feeHistN = 32; feeHistIdx = 0;
  const float day[24] = {0.30,0.25,0.22,0.20,0.22,0.30,0.45,0.60,0.75,0.85,0.90,0.95,1.0,0.97,0.92,0.88,0.84,0.8,0.76,0.7,0.62,0.55,0.45,0.36};
  for (int d = 0; d < 7; d++) for (int h = 0; h < 24; h++) feeBkt[d * 24 + h] = 30 * day[h] * (0.9f + 0.03f * d);
  feeSamples = 812;
  // signaux calibrés : séries synthétiques (300 j + 84 sem) -> computeSignals()
  { float v = 60000; for (int i = 0; i < SIG_ND; i++) { float o = v; rw(v, 0.022f); v *= 1.0012f;
      if (i > 250) v = v * 0.997f + 97000 * 0.003f;          // compression finale
      sgC[i] = v; sgH[i] = fmaxf(o, v) * (1 + random(0, 150) / 10000.0f); sgL[i] = fminf(o, v) * (1 - random(0, 150) / 10000.0f); }
    sgN = SIG_ND;
    float w = 30000; for (int i = 0; i < SIG_NW; i++) { float o = w; rw(w, 0.05f); w *= 1.01f; swC[i] = w; swH[i] = fmaxf(o, w) * 1.02f; swL[i] = fminf(o, w) * 0.98f; }
    swN = SIG_NW; computeSignals(); }
  anomMemAvg = 11000; anomMemVar = 400000; anomMemZ = 1.2f; anomFeeAvg = 15; anomFeeVar = 9; anomFeeZ = 0.8f;
}
