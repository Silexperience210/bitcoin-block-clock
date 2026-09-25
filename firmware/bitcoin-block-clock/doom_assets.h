// =====================================================================
//  doom_assets.h — BTC DOOM v3 : assets 100 % procéduraux
//  ------------------------------------------------------------------
//  Aucun fichier externe, aucun asset d'id Software : palette 256
//  couleurs, colormaps d'éclairage (façon COLORMAP de Doom), textures
//  64x64, ciel, sprites des monstres / objets / armes sont GÉNÉRÉS au
//  premier lancement du jeu dans la PSRAM (~260 Ko).
//  Tout est en 8 bits indexé : index 0 = transparent (sprites).
// =====================================================================
#pragma once

// police 5x7 (glcdfont Adafruit, BSD) : ASCII 32..90, 5 colonnes/car, bit0 = haut
static const uint8_t DM_FONT[59 * 5] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x5F, 0x00, 0x00,
  0x00, 0x07, 0x00, 0x07, 0x00,
  0x14, 0x7F, 0x14, 0x7F, 0x14,
  0x24, 0x2A, 0x7F, 0x2A, 0x12,
  0x23, 0x13, 0x08, 0x64, 0x62,
  0x36, 0x49, 0x56, 0x20, 0x50,
  0x00, 0x08, 0x07, 0x03, 0x00,
  0x00, 0x1C, 0x22, 0x41, 0x00,
  0x00, 0x41, 0x22, 0x1C, 0x00,
  0x2A, 0x1C, 0x7F, 0x1C, 0x2A,
  0x08, 0x08, 0x3E, 0x08, 0x08,
  0x00, 0x80, 0x70, 0x30, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x08,
  0x00, 0x00, 0x60, 0x60, 0x00,
  0x20, 0x10, 0x08, 0x04, 0x02,
  0x3E, 0x51, 0x49, 0x45, 0x3E,
  0x00, 0x42, 0x7F, 0x40, 0x00,
  0x72, 0x49, 0x49, 0x49, 0x46,
  0x21, 0x41, 0x49, 0x4D, 0x33,
  0x18, 0x14, 0x12, 0x7F, 0x10,
  0x27, 0x45, 0x45, 0x45, 0x39,
  0x3C, 0x4A, 0x49, 0x49, 0x31,
  0x41, 0x21, 0x11, 0x09, 0x07,
  0x36, 0x49, 0x49, 0x49, 0x36,
  0x46, 0x49, 0x49, 0x29, 0x1E,
  0x00, 0x00, 0x14, 0x00, 0x00,
  0x00, 0x40, 0x34, 0x00, 0x00,
  0x00, 0x08, 0x14, 0x22, 0x41,
  0x14, 0x14, 0x14, 0x14, 0x14,
  0x00, 0x41, 0x22, 0x14, 0x08,
  0x02, 0x01, 0x59, 0x09, 0x06,
  0x3E, 0x41, 0x5D, 0x59, 0x4E,
  0x7C, 0x12, 0x11, 0x12, 0x7C,
  0x7F, 0x49, 0x49, 0x49, 0x36,
  0x3E, 0x41, 0x41, 0x41, 0x22,
  0x7F, 0x41, 0x41, 0x41, 0x3E,
  0x7F, 0x49, 0x49, 0x49, 0x41,
  0x7F, 0x09, 0x09, 0x09, 0x01,
  0x3E, 0x41, 0x41, 0x51, 0x73,
  0x7F, 0x08, 0x08, 0x08, 0x7F,
  0x00, 0x41, 0x7F, 0x41, 0x00,
  0x20, 0x40, 0x41, 0x3F, 0x01,
  0x7F, 0x08, 0x14, 0x22, 0x41,
  0x7F, 0x40, 0x40, 0x40, 0x40,
  0x7F, 0x02, 0x1C, 0x02, 0x7F,
  0x7F, 0x04, 0x08, 0x10, 0x7F,
  0x3E, 0x41, 0x41, 0x41, 0x3E,
  0x7F, 0x09, 0x09, 0x09, 0x06,
  0x3E, 0x41, 0x51, 0x21, 0x5E,
  0x7F, 0x09, 0x19, 0x29, 0x46,
  0x26, 0x49, 0x49, 0x49, 0x32,
  0x03, 0x01, 0x7F, 0x01, 0x03,
  0x3F, 0x40, 0x40, 0x40, 0x3F,
  0x1F, 0x20, 0x40, 0x20, 0x1F,
  0x3F, 0x40, 0x38, 0x40, 0x3F,
  0x63, 0x14, 0x08, 0x14, 0x63,
  0x03, 0x04, 0x78, 0x04, 0x03,
  0x61, 0x59, 0x49, 0x4D, 0x43
};


// ---------------- palette : 16 rampes x 16 nuances ----------------
enum { RP_GREY = 0, RP_STEEL, RP_BRICK, RP_RUST, RP_ORANGE, RP_YELLOW, RP_RED, RP_MONEY,
       RP_ZOMBIE, RP_SKIN, RP_SCREEN, RP_PURPLE, RP_TOXIC, RP_BONE, RP_WHITE, RP_FIRE };
static const uint8_t DM_RAMP[16][3] = {
  {205, 205, 205}, {150, 168, 190}, {176, 100, 58}, {112, 72, 44}, {247, 147, 26}, {255, 218, 70},
  {225, 38, 30},   {70, 190, 90},   {150, 172, 128}, {232, 170, 128}, {70, 145, 255}, {160, 84, 210},
  {140, 255, 60},  {222, 204, 162}, {255, 255, 255}, {255, 110, 16}
};
#define PX(r, s) ((uint8_t)((r) * 16 + ((s) < 1 ? 1 : ((s) > 15 ? 15 : (s)))))
#define TRANS 0

#define D_NL   16                      // niveaux de lumière
#define D_NPAL 4                       // 0 normal, 1-2 douleur (rouge), 3 bonus (or)
#define TEXS   64
enum { TX_BRICK = 0, TX_TECH, TX_SUPPORT, TX_BTC, TX_DOOR, TX_EXIT_OFF, TX_COMPUTER, TX_HODL, TX_STACK,
       TX_REDBRICK, TX_MARBLE, TX_EXIT_ON, TX_KEYDOOR, TX_FLOOR, TX_DIRT, TX_SLUDGE, TX_CEIL, TX_CEILLIGHT,
       TX_GRATE, TX_COUNT };
#define SKY_W 256
#define SKY_H 150
#define SPR   48                       // sprites 48x48
#define WPW   64                       // armes 64x48 (affichées x2)
#define WPH   48
enum { SP_ZOMBIE = 0, SP_IMP = 7, SP_SLIME = 14, SP_BOSS = 21,
       SP_MED = 28, SP_SATS, SP_SHELLS, SP_ARMOR, SP_KEY, SP_SHOTGUN, SP_BARREL, SP_BOOM1, SP_LAMP, SP_RIG,
       SP_FIRE1, SP_FIRE2, SP_BILL1, SP_BILL2, SP_PUFF, SP_BLOOD, SP_BOOM2, SP_COUNT };
enum { PO_W1 = 0, PO_W2, PO_ATK, PO_PAIN, PO_D1, PO_D2, PO_DEAD };          // 7 poses par monstre
enum { WP_PISTOL = 0, WP_PISTOL_F, WP_SG, WP_SG_F, WP_SG_P, WP_COUNT };

static uint16_t *dCM   = nullptr;      // [D_NPAL][D_NL][256] RGB565
static uint8_t  *dTex  = nullptr;      // [TX_COUNT][64*64] colonne par colonne (x*64 + y)
static uint8_t  *dSky  = nullptr;      // [SKY_W][SKY_H]
static uint8_t  *dSpr  = nullptr;      // [SP_COUNT][48*48] colonne par colonne
static uint8_t  *dWpn  = nullptr;      // [WP_COUNT][64*48]
static uint32_t  dSeed = 1;
static inline uint32_t dRnd() { dSeed = dSeed * 1664525u + 1013904223u; return dSeed >> 8; }
static inline int dRi(int n) { return n > 0 ? (int)(dRnd() % (uint32_t)n) : 0; }

// ---------------- couleurs / colormaps ----------------
static void dmBuildColormaps() {
  for (int v = 0; v < D_NPAL; v++)
    for (int L = 0; L < D_NL; L++) {
      float lf = powf((L + 1) / (float)D_NL, 1.35f);
      for (int i = 0; i < 256; i++) {
        int rp = i >> 4, sh = i & 15;
        float f = powf((sh + 1) / 16.0f, 1.1f);
        float r = DM_RAMP[rp][0] * f, g = DM_RAMP[rp][1] * f, b = DM_RAMP[rp][2] * f;
        if (sh >= 13) { float w = (sh - 12) * 0.12f; r += (255 - r) * w; g += (255 - g) * w; b += (255 - b) * w; }
        r *= lf; g *= lf; b *= lf;
        if (v == 1) { r = r * 0.72f + 255 * 0.28f; g *= 0.72f; b *= 0.72f; }
        if (v == 2) { r = r * 0.5f + 255 * 0.5f; g *= 0.5f; b *= 0.5f; }
        if (v == 3) { r = r * 0.78f + 255 * 0.22f; g = g * 0.78f + 200 * 0.22f; b *= 0.78f; }
        uint16_t c = ((uint16_t)(min(255.0f, r) * 31 / 255) << 11) | ((uint16_t)(min(255.0f, g) * 63 / 255) << 5) | (uint16_t)(min(255.0f, b) * 31 / 255);
        dCM[(v * D_NL + L) * 256 + i] = c;
      }
    }
}

// ---------------- petit peintre 8 bits (colonne par colonne) ----------------
static uint8_t *pB = nullptr; static int pW = 0, pH = 0;
static inline void pTarget(uint8_t *b, int w, int h) { pB = b; pW = w; pH = h; memset(b, TRANS, w * h); }
static inline void pp(int x, int y, uint8_t c) { if ((unsigned)x < (unsigned)pW && (unsigned)y < (unsigned)pH) pB[x * pH + y] = c; }
static inline uint8_t pg(int x, int y) { return ((unsigned)x < (unsigned)pW && (unsigned)y < (unsigned)pH) ? pB[x * pH + y] : TRANS; }
// rectangle en dégradé vertical (nuance s0 en haut -> s1 en bas)
static void pRect(int x0, int y0, int w, int h, int rp, int s0, int s1) {
  for (int y = 0; y < h; y++) {
    int s = h > 1 ? s0 + (s1 - s0) * y / (h - 1) : s0;
    for (int x = 0; x < w; x++) pp(x0 + x, y0 + y, PX(rp, s));
  }
}
// ellipse pleine : dégradé vertical + reflet haut-gauche
static void pEll(int cx, int cy, int rx, int ry, int rp, int s0, int s1) {
  if (rx < 1) rx = 1; if (ry < 1) ry = 1;
  for (int y = -ry; y <= ry; y++)
    for (int x = -rx; x <= rx; x++) {
      float d = (x * x) / (float)(rx * rx) + (y * y) / (float)(ry * ry);
      if (d > 1.0f) continue;
      int s = s0 + (s1 - s0) * (y + ry) / (2 * ry);
      if (x < 0 && y < 0 && d < 0.35f) s += 1;          // reflet
      if (d > 0.8f) s -= 1;                              // bord ombré
      pp(cx + x, cy + y, PX(rp, s));
    }
}
static void pLine(int x0, int y0, int x1, int y1, uint8_t c, int th = 1) {
  int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (;;) {
    for (int a = 0; a < th; a++) for (int b = 0; b < th; b++) pp(x0 + a - th / 2, y0 + b - th / 2, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
// contour sombre d'1 px autour de la forme (lisibilité à distance)
static void pOutline(uint8_t c) {
  static uint8_t tmp[SPR * SPR];
  int n = pW * pH; if (n > SPR * SPR) return;
  memcpy(tmp, pB, n);
  for (int x = 0; x < pW; x++)
    for (int y = 0; y < pH; y++) {
      if (tmp[x * pH + y] != TRANS) continue;
      bool nb = (x > 0 && tmp[(x - 1) * pH + y]) || (x < pW - 1 && tmp[(x + 1) * pH + y]) ||
                (y > 0 && tmp[x * pH + y - 1]) || (y < pH - 1 && tmp[x * pH + y + 1]);
      if (nb) pB[x * pH + y] = c;
    }
}
static void pGlyph(char ch, int x, int y, int sx, int sy, uint8_t c) {
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  if (ch < 32 || ch > 90) return;
  for (int i = 0; i < 5; i++) {
    uint8_t line = pgm_read_byte(&DM_FONT[(ch - 32) * 5 + i]);
    for (int j = 0; j < 8; j++, line >>= 1)
      if (line & 1)
        for (int a = 0; a < sx; a++) for (int b = 0; b < sy; b++) pp(x + i * sx + a, y + j * sy + b, c);
  }
}
static void pText(const char *s, int x, int y, int sx, int sy, uint8_t c) { for (; *s; s++, x += 6 * sx) pGlyph(*s, x, y, sx, sy, c); }
static int pTextW(const char *s, int sx) { return (int)strlen(s) * 6 * sx - sx; }

// ---------------- textures murs / sols (64x64) ----------------
static uint8_t *dT(int id) { return dTex + id * TEXS * TEXS; }
static void texBricks(int rp, int base, int mortarRp, int mortarS) {
  for (int by = 0; by < 8; by++)
    for (int bx = 0; bx < 5; bx++) {
      int x0 = bx * 16 - ((by & 1) ? 8 : 0), s = base + dRi(3) - 1;
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++) {
          int px = x0 + x, py = by * 8 + y;
          if (px < 0 || px >= 64) continue;
          int sh = s + dRi(3) - 1;
          if (y == 0) sh += 2; if (y == 6) sh -= 2;
          pp(px, py, (y == 7 || x == 15) ? PX(mortarRp, mortarS) : PX(rp, sh));
        }
    }
}
static void texPanels(int rp, int base) {
  for (int x = 0; x < 64; x++)
    for (int y = 0; y < 64; y++) {
      int s = base + dRi(2);
      int px = x & 31, py = y & 31;
      if (px == 0 || py == 0) s = base + 4;
      else if (px == 31 || py == 31) s = base - 3;
      pp(x, y, PX(rp, s));
    }
  for (int p = 0; p < 4; p++) {                      // rivets
    int ox = (p & 1) * 32, oy = (p >> 1) * 32;
    const int rv[4][2] = {{4, 4}, {27, 4}, {4, 27}, {27, 27}};
    for (int r = 0; r < 4; r++) { pp(ox + rv[r][0], oy + rv[r][1], PX(rp, base + 6)); pp(ox + rv[r][0] + 1, oy + rv[r][1] + 1, PX(rp, base - 3)); }
  }
}
static void dmGenTextures() {
  dSeed = 12345;
  // BRIQUES
  pTarget(dT(TX_BRICK), 64, 64); texBricks(RP_BRICK, 7, RP_GREY, 3);
  // PANNEAU TECH + bande orange
  pTarget(dT(TX_TECH), 64, 64); texPanels(RP_STEEL, 6);
  for (int x = 0; x < 64; x++) { for (int y = 44; y < 48; y++) pp(x, y, PX(RP_ORANGE, y == 44 ? 12 : 9)); pp(x, 43, PX(RP_GREY, 2)); pp(x, 48, PX(RP_GREY, 2)); }
  // POUTRE + rayures de danger
  pTarget(dT(TX_SUPPORT), 64, 64);
  for (int x = 0; x < 64; x++)
    for (int y = 0; y < 64; y++) {
      uint8_t c = PX(RP_STEEL, 4 + ((x % 8) == 0 ? -2 : dRi(2)));
      if (x >= 20 && x < 44) c = (((x + y) / 6) & 1) ? PX(RP_YELLOW, 11) : PX(RP_GREY, 2);
      if (x == 19 || x == 44) c = PX(RP_STEEL, 11);
      pp(x, y, c);
    }
  // MUR BITCOIN : carrelage orange + emblème ₿
  pTarget(dT(TX_BTC), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) pp(x, y, ((x & 15) == 15 || (y & 15) == 15) ? PX(RP_RUST, 3) : PX(RP_ORANGE, 6 + dRi(2)));
  pEll(32, 32, 19, 19, RP_ORANGE, 13, 10);
  for (int a = 0; a < 90; a++) { float t = a * 2 * PI / 90; pp(32 + (int)(cosf(t) * 19), 32 + (int)(sinf(t) * 19), PX(RP_YELLOW, 14)); }
  pGlyph('B', 25, 21, 3, 3, PX(RP_WHITE, 15));
  pRect(28, 16, 2, 5, RP_WHITE, 15, 15); pRect(33, 16, 2, 5, RP_WHITE, 15, 15);
  pRect(28, 44, 2, 5, RP_WHITE, 15, 15); pRect(33, 44, 2, 5, RP_WHITE, 15, 15);
  // PORTE : lattes + bande lumineuse
  pTarget(dT(TX_DOOR), 64, 64);
  for (int x = 0; x < 64; x++)
    for (int y = 0; y < 64; y++) {
      int s = 7 + dRi(2);
      if ((y & 7) == 0) s = 10; else if ((y & 7) == 7) s = 3;
      uint8_t c = PX(RP_STEEL, s);
      if (x < 3 || x > 60) c = PX(RP_STEEL, 3);
      if (x >= 30 && x <= 33) c = PX(RP_ORANGE, x == 30 || x == 33 ? 12 : 15);
      pp(x, y, c);
    }
  // PORTE À CLÉ : cadre orange + badge ₿
  pTarget(dT(TX_KEYDOOR), 64, 64);
  memcpy(dT(TX_KEYDOOR), dT(TX_DOOR), 4096);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) if (x < 5 || x > 58 || y < 4 || y > 59) pp(x, y, PX(RP_ORANGE, (x + y) & 4 ? 12 : 8));
  pEll(32, 32, 9, 9, RP_ORANGE, 14, 11); pGlyph('B', 30, 28, 1, 1, PX(RP_WHITE, 15));
  // SORTIE (interrupteur bas / haut)
  for (int k = 0; k < 2; k++) {
    int id = k ? TX_EXIT_ON : TX_EXIT_OFF;
    pTarget(dT(id), 64, 64); texPanels(RP_STEEL, 6);
    pRect(18, 16, 28, 34, RP_GREY, 3, 2);
    for (int x = 18; x < 46; x++) { pp(x, 16, PX(RP_STEEL, 11)); pp(x, 49, PX(RP_STEEL, 3)); }
    pText("EXIT", 20, 5, 1, 1, PX(RP_RED, 15));
    pRect(29, 30, 6, 4, RP_GREY, 8, 6);
    if (k) { pRect(30, 19, 4, 12, RP_GREY, 12, 9); pEll(40, 22, 2, 2, RP_MONEY, 15, 15); }
    else   { pRect(30, 33, 4, 12, RP_GREY, 12, 9); pEll(40, 22, 2, 2, RP_RED, 15, 15); }
  }
  // ORDINATEURS : graphiques en chandeliers
  pTarget(dT(TX_COMPUTER), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) pp(x, y, PX(RP_GREY, 3 + dRi(2)));
  for (int sc = 0; sc < 2; sc++) {
    int y0 = sc ? 34 : 6;
    pRect(5, y0, 54, 23, RP_SCREEN, 2, 1);
    int lvl = 12;
    for (int c = 0; c < 9; c++) {
      int d = dRi(9) - 4, top = y0 + 3 + min(lvl, lvl + d), h = abs(d) + 2;
      bool up = d <= 0;
      pRect(8 + c * 6, top, 3, h, up ? RP_MONEY : RP_RED, 13, 11);
      pp(9 + c * 6, top - 2, PX(up ? RP_MONEY : RP_RED, 9)); pp(9 + c * 6, top + h + 1, PX(up ? RP_MONEY : RP_RED, 9));
      lvl = constrain(lvl + d, 3, 14);
    }
  }
  for (int i = 0; i < 6; i++) pp(8 + i * 4, 60, PX(i & 1 ? RP_MONEY : RP_ORANGE, 15));
  // AFFICHES
  pTarget(dT(TX_HODL), 64, 64); texBricks(RP_BRICK, 6, RP_GREY, 3);
  pRect(5, 6, 54, 52, RP_BONE, 13, 11);
  for (int x = 5; x < 59; x++) { pp(x, 6, PX(RP_ORANGE, 12)); pp(x, 57, PX(RP_ORANGE, 9)); }
  pText("HODL", 32 - pTextW("HODL", 2) / 2, 11, 2, 2, PX(RP_ORANGE, 11));
  pEll(32, 43, 9, 9, RP_ORANGE, 13, 10); pGlyph('B', 30, 39, 1, 1, PX(RP_WHITE, 15));
  pTarget(dT(TX_STACK), 64, 64); texBricks(RP_BRICK, 6, RP_GREY, 3);
  pRect(6, 8, 52, 48, RP_ORANGE, 12, 10);
  pText("STACK", 32 - pTextW("STACK", 1) / 2, 14, 1, 1, PX(RP_GREY, 2));
  pText("SATS", 32 - pTextW("SATS", 2) / 2, 26, 2, 2, PX(RP_WHITE, 15));
  pText("21M", 32 - pTextW("21M", 1) / 2, 45, 1, 1, PX(RP_GREY, 2));
  // BRIQUE ROUGE (banque) + MARBRE
  pTarget(dT(TX_REDBRICK), 64, 64); texBricks(RP_RED, 5, RP_BONE, 6);
  pTarget(dT(TX_MARBLE), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) pp(x, y, PX(RP_BONE, 11 + dRi(2)));
  for (int v = 0; v < 5; v++) { int x = dRi(64), y = 0; while (y < 64) { pp(x, y, PX(RP_BONE, 7)); x = (x + dRi(3) - 1) & 63; y++; } }
  for (int x = 0; x < 64; x++) { pp(x, 2, PX(RP_YELLOW, 12)); pp(x, 61, PX(RP_YELLOW, 10)); }
  // SOLS / PLAFONDS
  pTarget(dT(TX_FLOOR), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) {
    int s = 6 + ((x >> 4) + (y >> 4)) % 2 + dRi(2);
    if ((x & 15) == 0 || (y & 15) == 0) s = 3;
    pp(x, y, PX(RP_GREY, s));
  }
  pTarget(dT(TX_DIRT), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) pp(x, y, PX(RP_RUST, 5 + dRi(3)));
  for (int i = 0; i < 40; i++) pp(dRi(64), dRi(64), PX(RP_BONE, 8));
  pTarget(dT(TX_SLUDGE), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) {
    float v = sinf(x * 0.2f) + sinf(y * 0.25f + x * 0.1f) + sinf((x + y) * 0.15f);
    pp(x, y, PX(RP_TOXIC, 8 + (int)(v * 1.6f)));
  }
  for (int i = 0; i < 18; i++) pEll(dRi(60) + 2, dRi(60) + 2, 1, 1, RP_TOXIC, 14, 13);
  pTarget(dT(TX_CEIL), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) pp(x, y, PX(RP_STEEL, ((x & 31) == 0 || (y & 31) == 0) ? 2 : 4 + dRi(2)));
  pTarget(dT(TX_CEILLIGHT), 64, 64);
  memcpy(dT(TX_CEILLIGHT), dT(TX_CEIL), 4096);
  pRect(14, 22, 36, 20, RP_WHITE, 15, 14);
  for (int x = 14; x < 50; x++) { pp(x, 21, PX(RP_STEEL, 9)); pp(x, 42, PX(RP_STEEL, 9)); }
  pTarget(dT(TX_GRATE), 64, 64);
  for (int x = 0; x < 64; x++) for (int y = 0; y < 64; y++) pp(x, y, ((x & 7) < 5 && (y & 7) < 5) ? PX(RP_STEEL, 2) : PX(RP_STEEL, 5 + dRi(2)));
}

// ---------------- ciel : dégradé violet -> orange, étoiles, skyline ----------------
static void dmGenSky() {
  dSeed = 777;
  pTarget(dSky, SKY_W, SKY_H);
  for (int y = 0; y < SKY_H; y++) {
    float k = y / (float)(SKY_H - 1);
    for (int x = 0; x < SKY_W; x++) {
      uint8_t c;
      if (k < 0.55f) c = PX(RP_PURPLE, 1 + (int)(k * 9));
      else c = PX(RP_ORANGE, 3 + (int)((k - 0.55f) * 18));
      pp(x, y, c);
    }
  }
  for (int i = 0; i < 90; i++) pp(dRi(SKY_W), dRi(SKY_H / 2), PX(RP_WHITE, 9 + dRi(7)));
  // skyline (périodique en x)
  int x = 0;
  while (x < SKY_W) {
    int w = 8 + dRi(18), h = 14 + dRi(46), top = SKY_H - h;
    for (int xx = x; xx < min(SKY_W, x + w); xx++)
      for (int y = top; y < SKY_H; y++) {
        bool win = ((xx - x) % 4 == 2) && ((y - top) % 5 == 2) && dRi(3) == 0;
        pp(xx, y, win ? PX(RP_YELLOW, 11) : PX(RP_GREY, 1));
      }
    x += w + dRi(3);
  }
}

// ---------------- sprites des monstres (48x48, pieds en y = 47) ----------------
static void drZombie(int po) {
  uint8_t dark = PX(RP_GREY, 1);
  if (po >= PO_D1) {
    pEll(26, 46, 15, 2, RP_RED, 5, 3);                                  // flaque
    if (po == PO_D1) {
      pRect(17, 36, 5, 11, RP_GREY, 4, 3); pRect(26, 38, 5, 9, RP_GREY, 4, 3);
      pRect(15, 24, 18, 14, RP_STEEL, 7, 4); pEll(28, 20, 6, 6, RP_ZOMBIE, 9, 6);
      pp(26, 20, PX(RP_RED, 12));
    } else {
      pEll(24, 42, 15, 4, RP_STEEL, 6, 3); pEll(9, 41, 5, 4, RP_ZOMBIE, 8, 5);
      pRect(30, 41, 12, 3, RP_GREY, 4, 3);
      if (po == PO_D2) pEll(24, 38, 4, 2, RP_RED, 9, 7);
    }
    pOutline(dark); return;
  }
  bool w2 = (po == PO_W2);
  pRect(w2 ? 15 : 17, 32, 5, w2 ? 14 : 15, RP_GREY, 5, 3);          // jambes
  pRect(w2 ? 28 : 26, w2 ? 33 : 32, 5, w2 ? 14 : 15, RP_GREY, 5, 3);
  pRect(w2 ? 14 : 16, 45, 7, 2, RP_GREY, 1, 1); pRect(w2 ? 27 : 25, 45, 7, 2, RP_GREY, 1, 1);
  pRect(14, 16, 20, 17, RP_STEEL, 8, 5);                               // costume
  pRect(22, 16, 4, 9, RP_WHITE, 13, 12);                               // chemise
  pRect(23, 17, 2, 10, RP_RED, 10, 8);                                 // cravate
  pRect(10, 17, 4, 13, RP_STEEL, 7, 5); pEll(12, 31, 2, 2, RP_ZOMBIE, 9, 7);
  if (po == PO_ATK) {
    pRect(30, 20, 10, 4, RP_STEEL, 8, 6); pRect(38, 19, 7, 3, RP_GREY, 5, 3);
    pEll(45, 20, 3, 3, RP_YELLOW, 15, 13);
  } else {
    pRect(34, 17, 4, 13, RP_STEEL, 7, 5); pEll(36, 31, 2, 2, RP_ZOMBIE, 9, 7); pRect(35, 32, 3, 5, RP_GREY, 4, 3);
  }
  int hx = 24 + (po == PO_PAIN ? 2 : 0);
  pEll(hx, 9, 6, 7, RP_ZOMBIE, 10, 7);
  pRect(hx - 6, 2, 12, 3, RP_RUST, 3, 2);
  pp(hx - 3, 8, PX(RP_GREY, 1)); pp(hx + 3, 8, PX(RP_GREY, 1));
  uint8_t eye = po == PO_PAIN ? PX(RP_WHITE, 15) : PX(RP_RED, 15);
  pp(hx - 2, 9, eye); pp(hx + 2, 9, eye);
  pLine(hx - 2, 13, hx + 2, 13, PX(RP_GREY, 2));
  if (po == PO_PAIN) for (int i = 0; i < 7; i++) pp(18 + dRi(12), 18 + dRi(12), PX(RP_RED, 10));
  pOutline(dark);
}
static void drImp(int po) {
  uint8_t dark = PX(RP_GREY, 1);
  if (po >= PO_D1) {
    pEll(24, 46, 17, 2, RP_TOXIC, 7, 5);
    if (po == PO_D1) { pEll(24, 36, 11, 9, RP_MONEY, 8, 4); pEll(18, 26, 6, 5, RP_MONEY, 9, 6); pLine(14, 23, 10, 17, PX(RP_BONE, 10), 2); }
    else { pEll(24, 42, 14, 5, RP_MONEY, 6, 3); if (po == PO_D2) pLine(12, 40, 8, 35, PX(RP_BONE, 10), 2); }
    pOutline(dark); return;
  }
  bool w2 = (po == PO_W2);
  int lo = w2 ? -2 : 2;
  pEll(18 + lo, 37, 4, 6, RP_MONEY, 7, 4); pEll(30 - lo, 37, 4, 6, RP_MONEY, 7, 4);  // cuisses
  pLine(18 + lo, 42, 16 + lo, 47, PX(RP_MONEY, 4), 3); pLine(30 - lo, 42, 32 - lo, 47, PX(RP_MONEY, 4), 3);
  pp(14 + lo, 47, PX(RP_BONE, 12)); pp(34 - lo, 47, PX(RP_BONE, 12));
  pEll(24, 26, 11, 11, RP_MONEY, 9, 5);                                // torse
  pEll(24, 29, 6, 7, RP_MONEY, 12, 9);
  pGlyph('$', 22, 25, 1, 1, PX(RP_YELLOW, 14));
  pLine(14, 18, 11, 14, PX(RP_BONE, 12), 2); pLine(34, 18, 37, 14, PX(RP_BONE, 12), 2);   // épines
  if (po == PO_ATK) {
    pLine(15, 20, 10, 5, PX(RP_MONEY, 7), 3); pLine(33, 20, 38, 5, PX(RP_MONEY, 7), 3);
    pEll(24, 4, 6, 4, RP_TOXIC, 15, 11);
  } else {
    pLine(14, 21, 9, 34, PX(RP_MONEY, 6), 3); pLine(34, 21, 39, 34, PX(RP_MONEY, 6), 3);
    pp(8, 35, PX(RP_BONE, 13)); pp(40, 35, PX(RP_BONE, 13));
  }
  int hx = 24 + (po == PO_PAIN ? -2 : 0);
  pEll(hx, 11, 7, 6, RP_MONEY, po == PO_PAIN ? 13 : 9, 6);
  pLine(hx - 5, 7, hx - 9, 0, PX(RP_BONE, 12), 2); pLine(hx + 5, 7, hx + 9, 0, PX(RP_BONE, 12), 2);
  pp(hx - 3, 10, PX(RP_YELLOW, 15)); pp(hx - 2, 10, PX(RP_YELLOW, 15)); pp(hx + 2, 10, PX(RP_YELLOW, 15)); pp(hx + 3, 10, PX(RP_YELLOW, 15));
  pRect(hx - 3, 14, 7, 2, RP_GREY, 1, 1); pp(hx - 2, 14, PX(RP_WHITE, 15)); pp(hx, 14, PX(RP_WHITE, 15)); pp(hx + 2, 14, PX(RP_WHITE, 15));
  pOutline(dark);
}
static void drSlime(int po) {
  uint8_t dark = PX(RP_GREY, 1);
  if (po >= PO_D1) {
    pEll(24, 44, po == PO_D1 ? 18 : 21, po == PO_D1 ? 6 : 3, RP_RUST, 7, 4);
    pEll(18, 43, 3, 2, RP_WHITE, 12, 10); pp(18, 43, dark);
    pOutline(dark); return;
  }
  bool w2 = (po == PO_W2);
  int rx = w2 ? 18 : 16, ry = w2 ? 11 : 13;
  pEll(24, 47 - ry, rx, ry, RP_RUST, 9, 4);
  pEll(24, 47 - ry - 3, rx - 5, ry - 6, RP_MONEY, po == PO_PAIN ? 13 : 8, 5);
  for (int d = 0; d < 4; d++) pRect(10 + d * 9, 45, 2, 2, RP_RUST, 3, 3);   // gouttes
  const int ex[5] = {15, 22, 30, 19, 33}, ey[5] = {0, -3, -1, 5, 4};
  for (int e = 0; e < 5; e++) {
    int yy = 47 - ry * 2 + 9 + ey[e];
    pEll(ex[e], yy, 3, 3, RP_WHITE, 14, 12); pp(ex[e] + (po == PO_PAIN ? 0 : 1), yy, dark);
  }
  if (po == PO_ATK) { pEll(24, 40, 9, 5, RP_GREY, 1, 1); for (int t = 0; t < 5; t++) pp(18 + t * 3, 36, PX(RP_WHITE, 15)); }
  else pLine(18, 40, 30, 40, PX(RP_RUST, 2));
  pGlyph('S', 36, 33, 1, 1, PX(RP_YELLOW, 13));
  pOutline(dark);
}
static void drBoss(int po) {                         // LA PLANCHE À BILLETS
  uint8_t dark = PX(RP_GREY, 1);
  if (po >= PO_D1) {
    if (po == PO_DEAD) {
      pRect(6, 34, 36, 13, RP_GREY, 5, 2);
      for (int i = 0; i < 9; i++) pRect(dRi(40) + 2, 38 + dRi(8), 6, 3, RP_MONEY, 11, 9);
    } else {
      pRect(6, 12, 36, 29, RP_STEEL, 7, 3);
      int r = po == PO_D1 ? 12 : 18;
      pEll(24, 24, r, r, RP_FIRE, 15, 9); pEll(20, 20, r / 2, r / 2, RP_YELLOW, 15, 13);
    }
    pOutline(dark); return;
  }
  bool w2 = (po == PO_W2);
  pRect(12, w2 ? 34 : 36, 6, w2 ? 12 : 11, RP_GREY, 6, 3); pRect(30, w2 ? 36 : 34, 6, w2 ? 11 : 12, RP_GREY, 6, 3);
  pRect(9, 45, 12, 3, RP_GREY, 4, 2); pRect(27, 45, 12, 3, RP_GREY, 4, 2);
  pRect(6, 8, 36, 29, RP_STEEL, po == PO_PAIN ? 13 : 10, 5);          // caisse
  pRect(9, 3, 30, 6, RP_STEEL, 12, 9);
  for (int i = 0; i < 4; i++) pp(12 + i * 7, 5, PX(i & 1 ? RP_MONEY : RP_RED, 15));
  uint8_t lamp = po == PO_ATK ? RP_YELLOW : RP_RED;
  pEll(13, 13, 3, 3, lamp, 15, 12); pEll(35, 13, 3, 3, lamp, 15, 12);
  pText("BRRR", 13, 17, 1, 1, PX(RP_YELLOW, 14));
  pRect(11, 26, 26, 2, RP_GREY, 1, 1);                                 // fente
  int bills = po == PO_ATK ? 10 : 5;
  pRect(13, 28, 22, bills, RP_MONEY, 12, 9);
  for (int x = 15; x < 33; x += 6) pp(x + 2, 29, PX(RP_WHITE, 14));
  if (po == PO_PAIN) for (int i = 0; i < 10; i++) pp(6 + dRi(36), 8 + dRi(28), PX(RP_YELLOW, 15));
  pOutline(dark);
}
// ---------------- objets / effets ----------------
static void drItem(int id) {
  uint8_t dark = PX(RP_GREY, 1);
  switch (id) {
    case SP_MED:     pRect(14, 35, 20, 12, RP_WHITE, 14, 11); pRect(22, 37, 4, 8, RP_MONEY, 12, 11); pRect(19, 39, 10, 4, RP_MONEY, 12, 11); break;
    case SP_SATS:    for (int i = 0; i < 4; i++) pEll(24, 44 - i * 3, 9, 3, RP_ORANGE, 14 - i, 9);
                     pGlyph('B', 22, 32, 1, 1, PX(RP_WHITE, 15)); break;
    case SP_SHELLS:  pRect(15, 39, 18, 8, RP_YELLOW, 11, 8); for (int i = 0; i < 4; i++) pRect(17 + i * 4, 33, 3, 6, RP_RED, 12, 9);
                     pLine(22, 40, 25, 43, PX(RP_GREY, 2)); pLine(25, 43, 23, 45, PX(RP_GREY, 2)); break;
    case SP_ARMOR:   pEll(24, 37, 12, 10, RP_ORANGE, 12, 7); pEll(24, 29, 4, 3, RP_GREY, 1, 1); pRect(23, 33, 2, 12, RP_RUST, 5, 4);
                     pGlyph('B', 16, 36, 1, 1, PX(RP_WHITE, 14)); break;
    case SP_KEY:     pRect(17, 36, 14, 10, RP_ORANGE, 14, 10); pRect(17, 38, 14, 2, RP_GREY, 2, 2); pEll(24, 43, 2, 2, RP_YELLOW, 15, 14); break;
    case SP_SHOTGUN: pRect(6, 40, 30, 3, RP_GREY, 7, 4); pRect(32, 40, 10, 6, RP_BRICK, 9, 6); pRect(12, 43, 9, 3, RP_BRICK, 10, 7);
                     pLine(20, 39, 24, 41, PX(RP_YELLOW, 14)); break;
    case SP_BARREL:  pRect(15, 22, 18, 25, RP_MONEY, 8, 4); pRect(15, 27, 18, 2, RP_GREY, 3, 3); pRect(15, 41, 18, 2, RP_GREY, 3, 3);
                     pEll(24, 22, 9, 2, RP_TOXIC, 13, 11); pGlyph('$', 22, 31, 1, 1, PX(RP_YELLOW, 13)); break;
    case SP_BOOM1:                                                   // boule de feu irrégulière
      for (int b = 0; b < 7; b++) pEll(24 + dRi(17) - 8, 30 + dRi(13) - 6, 5 + dRi(6), 4 + dRi(6), RP_FIRE, 15, 9);
      for (int b = 0; b < 4; b++) pEll(24 + dRi(11) - 5, 29 + dRi(9) - 4, 2 + dRi(4), 2 + dRi(3), RP_YELLOW, 15, 14);
      pEll(24, 30, 3, 3, RP_WHITE, 15, 15); break;
    case SP_BOOM2:                                                   // flammes + fumée
      for (int b = 0; b < 8; b++) pEll(24 + dRi(25) - 12, 26 + dRi(19) - 9, 4 + dRi(6), 4 + dRi(6), RP_GREY, 6, 3);
      for (int b = 0; b < 6; b++) pEll(24 + dRi(19) - 9, 30 + dRi(13) - 6, 4 + dRi(5), 3 + dRi(5), RP_FIRE, 14, 8);
      for (int b = 0; b < 3; b++) pEll(24 + dRi(9) - 4, 31 + dRi(7) - 3, 2 + dRi(3), 2 + dRi(2), RP_YELLOW, 15, 13); break;
    case SP_LAMP:    pRect(22, 12, 4, 35, RP_GREY, 8, 4); pRect(18, 44, 12, 3, RP_GREY, 5, 3); pEll(24, 8, 6, 6, RP_YELLOW, 15, 13); break;
    case SP_RIG:     pRect(7, 22, 34, 25, RP_STEEL, 8, 4);
                     for (int f = 0; f < 3; f++) { pEll(13 + f * 11, 33, 4, 4, RP_GREY, 2, 1); pLine(10 + f * 11, 33, 16 + f * 11, 33, PX(RP_GREY, 5)); }
                     pText("ASIC", 12, 40, 1, 1, PX(RP_ORANGE, 13)); for (int i = 0; i < 5; i++) pp(10 + i * 6, 25, PX(i & 1 ? RP_MONEY : RP_ORANGE, 15)); break;
    case SP_FIRE1:   pEll(24, 24, 7, 7, RP_TOXIC, 15, 10); pEll(23, 23, 3, 3, RP_WHITE, 15, 15); break;
    case SP_FIRE2:   pEll(24, 24, 8, 6, RP_TOXIC, 14, 9); pEll(24, 24, 3, 2, RP_WHITE, 15, 15); break;
    case SP_BILL1:   pRect(14, 19, 20, 10, RP_MONEY, 12, 9); pEll(24, 24, 4, 3, RP_MONEY, 14, 13); pGlyph('$', 22, 21, 1, 1, PX(RP_MONEY, 5)); break;
    case SP_BILL2:   pRect(20, 17, 8, 14, RP_MONEY, 12, 9); pRect(22, 22, 4, 4, RP_MONEY, 14, 13); break;
    case SP_PUFF:    pEll(24, 24, 4, 4, RP_GREY, 13, 9); pEll(27, 21, 2, 2, RP_GREY, 14, 12); break;
    case SP_BLOOD:   pEll(24, 24, 4, 3, RP_RED, 12, 8); pp(20, 20, PX(RP_RED, 10)); pp(28, 21, PX(RP_RED, 10)); break;
  }
  if (id != SP_FIRE1 && id != SP_FIRE2 && id != SP_BOOM1 && id != SP_BOOM2 && id != SP_PUFF) pOutline(dark);
}
// ---------------- armes (64x48, vue à la 1re personne) ----------------
static void drWeapon(int w) {
  uint8_t dark = PX(RP_GREY, 1);
  if (w == WP_PISTOL || w == WP_PISTOL_F) {
    int k = (w == WP_PISTOL_F) ? 3 : 0;                       // recul de la culasse
    if (k) { pEll(32, 7, 10, 7, RP_FIRE, 15, 12); pEll(32, 7, 5, 4, RP_YELLOW, 15, 15);
             pLine(32, 0, 32, 14, PX(RP_WHITE, 15)); pLine(21, 7, 43, 7, PX(RP_YELLOW, 15)); }
    pRect(26, 32, 12, 14, RP_ORANGE, 11, 6);                  // crosse orange
    pGlyph('B', 29, 35, 1, 1, PX(RP_WHITE, 14));
    pRect(23, 13 + k, 18, 21, RP_GREY, 9, 3);                 // culasse vue de dos
    pRect(23, 13 + k, 18, 2, RP_GREY, 12, 11);
    for (int y = 20; y < 30; y += 3) pLine(24, y + k, 27, y + k, PX(RP_GREY, 2));   // stries
    pRect(24, 10 + k, 4, 3, RP_GREY, 5, 4); pRect(36, 10 + k, 4, 3, RP_GREY, 5, 4); // hausse
    pRect(31, 9 + k, 2, 3, RP_ORANGE, 14, 14);                // guidon
    pEll(38, 42, 13, 8, RP_SKIN, 11, 6);                      // main + doigts
    pEll(23, 36, 5, 7, RP_SKIN, 12, 7);
    for (int f = 0; f < 3; f++) pLine(28, 37 + f * 3, 36, 37 + f * 3, PX(RP_SKIN, 5));
  } else {
    int down = (w == WP_SG_P) ? 5 : 0, pump = (w == WP_SG_P) ? 8 : 0;
    if (w == WP_SG_F) { pEll(32, 5, 13, 7, RP_YELLOW, 15, 14); pLine(20, 5, 44, 5, PX(RP_FIRE, 15), 2); pLine(32, 0, 32, 11, PX(RP_WHITE, 15), 2); }
    pRect(25, 6 + down, 14, 38, RP_GREY, 8, 3); pRect(26, 6 + down, 3, 38, RP_GREY, 12, 9);   // canon large
    pRect(29, 4 + down, 6, 3, RP_GREY, 2, 2);
    pLine(33, 14 + down, 30, 20 + down, PX(RP_YELLOW, 14)); pLine(30, 20 + down, 34, 22 + down, PX(RP_YELLOW, 14)); pLine(34, 22 + down, 31, 28 + down, PX(RP_YELLOW, 14));
    pRect(25, 26 + down + pump, 14, 9, RP_BRICK, 10, 6);
    pEll(24, 32 + down + pump, 6, 5, RP_SKIN, 12, 8);
    pEll(42, 45, 10, 6, RP_SKIN, 11, 7);
  }
  pOutline(dark);
}

static bool dmGenAssets() {
  if (dCM) return true;
  dCM  = (uint16_t*)ps_malloc(D_NPAL * D_NL * 256 * 2);
  dTex = (uint8_t*)ps_malloc(TX_COUNT * TEXS * TEXS);
  dSky = (uint8_t*)ps_malloc(SKY_W * SKY_H);
  dSpr = (uint8_t*)ps_malloc(SP_COUNT * SPR * SPR);
  dWpn = (uint8_t*)ps_malloc(WP_COUNT * WPW * WPH);
  if (!dCM || !dTex || !dSky || !dSpr || !dWpn) { Serial.println("[DOOM] PSRAM insuffisante"); return false; }
  unsigned long t0 = millis();
  dmBuildColormaps();
  dmGenTextures();
  dmGenSky();
  dSeed = 4242;
  for (int po = 0; po < 7; po++) {
    pTarget(dSpr + (SP_ZOMBIE + po) * SPR * SPR, SPR, SPR); drZombie(po);
    pTarget(dSpr + (SP_IMP + po) * SPR * SPR, SPR, SPR);    drImp(po);
    pTarget(dSpr + (SP_SLIME + po) * SPR * SPR, SPR, SPR);  drSlime(po);
    pTarget(dSpr + (SP_BOSS + po) * SPR * SPR, SPR, SPR);   drBoss(po);
  }
  for (int id = SP_MED; id < SP_COUNT; id++) { pTarget(dSpr + id * SPR * SPR, SPR, SPR); drItem(id); }
  for (int w = 0; w < WP_COUNT; w++) { pTarget(dWpn + w * WPW * WPH, WPW, WPH); drWeapon(w); }
  Serial.printf("[DOOM] assets generes en %lu ms\n", millis() - t0);
  return true;
}
