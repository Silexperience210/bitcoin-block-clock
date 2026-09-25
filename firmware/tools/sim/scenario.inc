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
float dist2(float x, float y) { return (x - dPX) * (x - dPX) + (y - dPY) * (y - dPY); }
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
    { char c2[300]; snprintf(c2, sizeof(c2), "mkdir -p %s", out); system(c2); }
    // vue 30J pour montrer le cône de volatilité
    page = PG_PRICE; curTf = TF_30J; chartCacheLoad(); pageEnterMs = millis(); chartAnimMs = 0; runFrames(45);
    snprintf(p, sizeof(p), "%s/page0_30j.ppm", out); harness_save(p);
    curTf = TF_7J; chartCacheLoad(); runFrames(45);
    snprintf(p, sizeof(p), "%s/page0_7j.ppm", out); harness_save(p);
    curTf = TF_24H; chartCacheLoad();
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
  } else if (!strcmp(mode, "sigcheck")) {           // parité ttmCore / EWMA vs calibration Python
    static float H[5000], Lw[5000], C[5000]; int N = 0;
    FILE *f = fopen(argc > 3 ? argv[3] : "daily.txt", "r");
    while (f && N < 5000 && fscanf(f, "%f %f %f", &H[N], &Lw[N], &C[N]) == 3) N++;
    if (f) fclose(f);
    FILE *o = fopen(out, "w");
    for (int i = 60; i < N; i++) { bool q; float bw, mm; ttmCore(H, Lw, C, N, i, &q, &bw, &mm); fprintf(o, "%d %d %.6f %.6f\n", i, q ? 1 : 0, bw, mm); }
    fclose(o);
    // EWMA firmware sur les 300 derniers jours (comme sur la carte)
    int st = N - 300; double v = 0;
    for (int t = 1; t <= 30; t++) { double r = log((double)C[st + t] / C[st + t - 1]); v += r * r; }
    v /= 30;
    for (int t = 31; t < 300; t++) { double r = log((double)C[st + t] / C[st + t - 1]); v = 0.94 * v + 0.06 * r * r; }
    fprintf(stderr, "volD_fw=%.6f\n", sqrt(v));
    return;
  } else if (!strcmp(mode, "doomzoo")) {             // galerie : monstres / objets en pleine lumière
    page = PG_DOOM; runFrames(2);
    dLoadLevel(0); dNT = 0;
    dAddThing(11.0f, 7.6f, K_ZOMBIE); dAddThing(11.2f, 8.8f, K_IMP); dAddThing(11.0f, 10.2f, K_SLIME);
    dAddThing(9.2f, 7.5f, K_MED); dAddThing(9.2f, 8.3f, K_SATS); dAddThing(9.2f, 9.1f, K_SHELLS); dAddThing(9.2f, 9.9f, K_ARMOR); dAddThing(9.2f, 10.7f, K_KEY);
    dAddThing(13.5f, 7.6f, K_BARREL); dAddThing(13.5f, 10.4f, K_RIG);
    dPX = 7.4f; dPY = 9.1f; dPA = 0; dState = DS_PLAY;
    for (int i = 0; i < dNT; i++) dTh[i].st = ST_IDLE;
    { unsigned long t = millis(); for (int i = 0; i < dNT; i++) dTh[i].losMs = t + 100000; }
    runFrames(3); harness_save((std::string(out) + "_1.ppm").c_str());
    dTh[0].st = ST_ATK; dTh[0].stMs = millis(); dTh[1].st = ST_ATK; dTh[1].stMs = millis(); dTh[2].st = ST_ATK; dTh[2].stMs = millis();
    dHasSG = true; dWeapon = 1; dShells = 20; dShotMs = millis();
    loop(); harness_save((std::string(out) + "_2.ppm").c_str());
    dLoadLevel(2); dPX = 10.0f; dPY = 12.5f; dPA = 0; dWeapon = 0; runFrames(2);
    harness_save((std::string(out) + "_3.ppm").c_str());
    dLoadLevel(1); dPX = 8.0f; dPY = 2.5f; dPA = 0; runFrames(2);
    harness_save((std::string(out) + "_4.ppm").c_str());
  } else if (!strcmp(mode, "doom")) {
    page = PG_PRICE; needRedraw = true; runFrames(8);
    rec(out);
    runFrames(4);
    tap(8 * TAB_W + TAB_W / 2, 310);                       // onglet DOOM -> melt
    runFrames(22);
    // avance (stick gauche)
    harness_touch(1, 80, 214); loop();
    for (int k = 0; k < 16; k++) { harness_touch(1, 80, 172); loop(); }
    harness_touch(0); runFrames(3);
    // porte de la salle des mineurs + zombie
    dPX = 4.5f; dPY = 3.5f; dPA = 0.0f;
    harness_touch(1, 80, 214); loop();
    for (int k = 0; k < 10; k++) { harness_touch(1, 80, 190); loop(); }
    for (int k = 0; k < 34; k++) { harness_touch(1, D_FIRE_X, D_FIRE_Y); loop(); }
    harness_touch(0); runFrames(8);
    // fusil à pompe + démon inflation
    dPX = 11.5f; dPY = 5.2f; dPA = PI / 2;
    for (int k = 0; k < 16; k++) loop();
    dPX = 9.5f; dPY = 7.4f; dPA = PI / 2;
    harness_touch(1, 80, 214); loop();
    for (int k = 0; k < 8; k++) { harness_touch(1, 80, 180); loop(); }
    harness_touch(0); runFrames(6);
    runFrames(14);
    { // vise le démon (1er monstre vivant de type IMP)
      for (int i = 0; i < dNT; i++) if (dTh[i].kind == K_IMP && dTh[i].st < ST_DYING) { dPA = atan2f(dTh[i].y - dPY, dTh[i].x - dPX); break; }
    }
    for (int k = 0; k < 40; k++) {
      for (int i = 0; i < dNT; i++) if (dTh[i].kind == K_IMP && dTh[i].st < ST_DYING && dist2(dTh[i].x, dTh[i].y) < 30) { dPA = atan2f(dTh[i].y - dPY, dTh[i].x - dPX); break; }
      harness_touch(1, D_FIRE_X, D_FIRE_Y); loop();
    }
    harness_touch(0); runFrames(10);
    // automap
    tap(20, 15); runFrames(16); tap(20, 15); runFrames(4);
    // tonneau explosif
    dPX = 1.6f; dPY = 3.5f; dPA = 0.0f; dWeapon = 0;
    runFrames(6);
    for (int k = 0; k < 14; k++) { harness_touch(1, D_FIRE_X, D_FIRE_Y); loop(); }
    harness_touch(0); runFrames(14);
    // sortie -> bilan -> melt vers le niveau 2
    dHasKey = true; dPX = 21.4f; dPY = 16.5f; dPA = 0.0f;
    harness_touch(1, 80, 214); loop();
    for (int k = 0; k < 10; k++) { harness_touch(1, 80, 180); loop(); }
    harness_touch(0);
    runMs(3000);
    tap(240, 150); runFrames(25);
    // niveau 3 : le boss
    dLoadLevel(2); dHasSG = true; dShells = 40; dWeapon = 1; dHealth = 100; dArmor = 100;
    dPX = 9.5f; dPY = 12.5f; dPA = 0.0f;
    runFrames(10);
    for (int k = 0; k < 60; k++) {
      for (int i = 0; i < dNT; i++) if (dTh[i].kind == K_BOSS) { dPA = atan2f(dTh[i].y - dPY, dTh[i].x - dPX); break; }
      harness_touch(1, D_FIRE_X, D_FIRE_Y); loop();
    }
    harness_touch(0);
    dHealth = 8; dArmor = 0; runFrames(60);
    rec(nullptr);
    fprintf(stderr, "etat=%d vie=%d kills=%d/%d niveau=%d\n", dState, dHealth, dKills, dKillsTot, dLvl);
  }
  fprintf(stderr, "flushes=%d t=%lu\n", harness_flushes(), millis());
}
