// Banc de rendu desktop : exécute le VRAI code de dessin du sketch,
// capture les frames flushées et les écrit en PPM (paysage 480x320).
#include <Arduino.h>
#include <vector>
#include <string>
extern unsigned long g_ms; extern time_t g_epoch; extern bool g_quietSerial;
extern int g_nTouch; extern int g_tx[2], g_ty[2];
static std::vector<uint16_t> g_last(480 * 320);
static int g_flushes = 0;
bool g_rec = false; char g_recDir[256] = "."; int g_recN = 0;
void harness_save_to(const char *path);
void harness_on_flush(const uint16_t *fb, int w, int h) {
  // natif portrait 320x480 -> paysage : logique (x,y) = fb[x*320 + 319 - y]
  for (int x = 0; x < 480; x++)
    for (int y = 0; y < 320; y++)
      g_last[y * 480 + x] = fb[x * w + (w - 1 - y)];
  g_flushes++;
  g_ms += 45;                      // coût réaliste d'un flush QSPI plein écran
  if (g_rec) { char p[300]; snprintf(p, sizeof(p), "%s/f%05d.ppm", g_recDir, g_recN++); harness_save_to(p); }
  (void)h;
}
void harness_save_to(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) { fprintf(stderr, "impossible d'ecrire %s\n", path); return; }
  fprintf(f, "P6\n480 320\n255\n");
  for (int i = 0; i < 480 * 320; i++) {
    uint16_t c = g_last[i];
    uint8_t r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
    fputc(r, f); fputc(g, f); fputc(b, f);
  }
  fclose(f);
}
void harness_save(const char *path) { harness_save_to(path); }
int harness_flushes() { return g_flushes; }
void harness_touch(int n, int x0 = 0, int y0 = 0, int x1 = 0, int y1 = 0) { g_nTouch = n; g_tx[0] = x0; g_ty[0] = y0; g_tx[1] = x1; g_ty[1] = y1; }
// le sketch prétraité (avec prototypes) est inclus ici
#include "sketch_pre.cpp"
// scénario (même unité de compilation : accès direct aux globals du sketch)
#include "scenario.inc"
int main(int argc, char **argv) {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); tzset();
  run_scenario(argc, argv);
  return 0;
}
