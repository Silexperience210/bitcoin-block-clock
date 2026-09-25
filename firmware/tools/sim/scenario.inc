#include "mockdata.inc"
void seedStrip() {
  time_t nowT = time(nullptr);
  const char *pn[4] = {"Foundry USA", "AntPool", "ViaBTC", "F2Pool"};
  float fees[4] = {14.2f, 6.1f, 3.2f, 22.7f}; long txs[4] = {3412, 2987, 4102, 1877};
  for (int i = 0; i < 4; i++) { stripH[i] = blockHeight - i; stripFee[i] = fees[i]; stripTx[i] = txs[i];
    stripTs[i] = (long)nowT - 252 - i * 560; strlcpy(stripPool[i], pn[i], sizeof(stripPool[i])); }
  stripN = 4; lambdaEpoch = 571;
  for (int i = 0; i < 6; i++) blkTs[i] = stripTs[0] - i * 560;
}
void rec(const char *dir) {
  g_rec = dir != nullptr;
  if (dir) { strcpy(g_recDir, dir); g_recN = 0; char c[300]; snprintf(c, sizeof(c), "mkdir -p %s && rm -f %s/*.ppm", dir, dir); system(c); }
}
// fait tourner loop() jusqu'à n frames rendues
void runFrames(int n) { int f0 = harness_flushes(); int guard = 0; while (harness_flushes() - f0 < n && guard++ < n * 60) loop(); }
void runMs(unsigned long ms) { unsigned long t0 = millis(); while (millis() - t0 < ms) loop(); }
void newBlock(const char *pool, float fee, long tx) {
  for (int i = STRIP_N - 1; i > 0; i--) { stripH[i] = stripH[i-1]; stripFee[i] = stripFee[i-1]; stripTx[i] = stripTx[i-1]; stripTs[i] = stripTs[i-1]; strcpy(stripPool[i], stripPool[i-1]); }
  blockHeight++; stripH[0] = blockHeight; stripFee[0] = fee; stripTx[0] = tx; stripTs[0] = time(nullptr);
  strlcpy(stripPool[0], pool, 14); strlcpy(lastPool, pool, sizeof(lastPool)); lastBlockTx = tx;
  for (int i = 5; i > 0; i--) blkTs[i] = blkTs[i-1]; blkTs[0] = stripTs[0];
  evNewBlock = true;
}
void swipe(int x0, int x1, int y) {             // geste réel via le pilote tactile simulé
  for (int k = 0; k <= 6; k++) { harness_touch(1, x0 + (x1 - x0) * k / 6, y); loop(); g_ms += 20; }
  harness_touch(0); loop();
}
void tap(int x, int y) { harness_touch(1, x, y); loop(); g_ms += 60; loop(); harness_touch(0); loop(); }
void run_scenario(int argc, char **argv) {
  const char *out = argc > 1 ? argv[1] : "out";
  const char *mode = argc > 2 ? argv[2] : "pages";
  char p[300];
  g_epoch = 1790000000 - 3 * 3600;                // ~18h locales
  if (!strcmp(mode, "boot")) {
    gfx->begin();
    rec(out);
    for (int i = 0; i < 60; i++) { char m[64]; snprintf(m, sizeof(m), "connexion a MaBox..."); bootSplash(m, 0.2f + 0.8f * i / 60.0f); g_ms += 5; }
    rec(nullptr); return;
  }
  setup();
  seedData(); seedStrip(); dispPrice = 0;
  refreshClock();
  if (!strcmp(mode, "pages")) {
    { char c[300]; snprintf(c, sizeof(c), "mkdir -p %s", out); system(c); }
    for (int pg = 0; pg < PG_COUNT; pg++) {
      page = pg; doomReset = true; poolsReset = true; pageEnterMs = millis();
      runFrames(45);
      snprintf(p, sizeof(p), "%s/page%d.ppm", out, pg); harness_save(p);
    }
  } else if (!strcmp(mode, "tour")) {            // démo : prix -> swipe -> on-chain -> nouveau bloc
    page = PG_PRICE; pageEnterMs = millis(); chartAnimMs = millis();
    rec(out);
    runFrames(30);
    btcPrice[CUR_EUR] = 97810; runFrames(20);     // tick de prix : flash vert
    swipe(400, 150, 160);                          // -> ON-CHAIN (glissement)
    runFrames(40);
    newBlock("MARA Pool", 9.4f, 3876);             // cinématique nouveau bloc
    runMs(NB_DUR + 100);
    runFrames(45);                                 // la frise glisse
    rec(nullptr);
  } else if (!strcmp(mode, "pagesanim")) {         // chaque page, ~1,5 s d'animation
    rec(out);
    for (int pg = 2; pg < PG_COUNT - 1; pg++) {
      if (pg != 2) tap(pg * TAB_W + TAB_W / 2, 310);
      else { page = pg; pageEnterMs = millis(); }
      runFrames(pg == 2 ? 50 : 40);
    }
    rec(nullptr);
  } else if (!strcmp(mode, "cube")) {
    page = PG_CUBE; rec(out);
    runFrames(40);
    newBlock("Foundry USA", 11.0f, 3021); mempoolCount = 2600;
    runFrames(90);
    rec(nullptr);
  } else if (!strcmp(mode, "eco") || !strcmp(mode, "off")) {   // niveaux réduits : rendu + event
    animLevel = !strcmp(mode, "eco") ? 1 : 0;
    rec(out);
    for (int pg = 0; pg < PG_COUNT; pg++) { if (pg) tap(pg * TAB_W + TAB_W / 2, 310); else { page = 0; needRedraw = true; } runMs(1500); }
    tap(TAB_W / 2, 310); newBlock("ViaBTC", 5.0f, 2011); runMs(NB_DUR + 600);
    rec(nullptr);
    animLevel = 2;
  } else if (!strcmp(mode, "doom")) {
    page = PG_DOOM; doomReset = true; rec(out);
    runFrames(10);
    for (int k = 0; k < 40; k++) { harness_touch(2, 70, 230 - k / 2, 300 + k, 250); loop(); }
    for (int k = 0; k < 30; k++) { harness_touch(2, 70, 200, 330, 250); loop(); if (k % 6 == 0) { harness_touch(2, 70, 200, 412, 254); loop(); } }
    harness_touch(0); runFrames(15);
    rec(nullptr);
  }
  fprintf(stderr, "flushes=%d t=%lu\n", harness_flushes(), millis());
}
