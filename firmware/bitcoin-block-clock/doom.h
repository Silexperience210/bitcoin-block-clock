// =====================================================================
//  doom.h — BTC DOOM v3 : un "vrai" Doom-like sur ESP32-S3
//  ------------------------------------------------------------------
//  • raycaster texturé plein écran : murs, SOLS et PLAFONDS texturés
//    (floor casting), ciel panoramique, éclairage par zones + fondu de
//    distance via colormaps (technique de Doom), palettes douleur / bonus
//  • portes coulissantes, porte à clé, interrupteur de sortie, boue toxique
//  • 4 monstres originaux (zombie fiat, démon inflation, slime shitcoin,
//    boss "planche à billets") : réveil à la vue ou au bruit, poursuite,
//    tir, boules de feu, corps-à-corps, douleur, mort animée
//  • pistolet + fusil à pompe (dispersion), tonneaux explosifs en chaîne,
//    3 niveaux, écran de bilan, automap, barre de statut + visage réactif,
//    transition "melt" façon Doom
//  • rendu direct dans le framebuffer PSRAM : en rotation 1, une colonne
//    verticale de l'écran = 320 pixels contigus -> tranches de murs rapides
//  Contrôles : stick gauche = avancer / pas latéral · stick droit = tourner
//  · FIRE = tirer · tap sur ARMES = changer d'arme · MAP · ✕ = quitter
// =====================================================================
#pragma once
#include "doom_assets.h"

#define DV_W    480
#define DV_H    280
#ifndef DV_COLS
#define DV_COLS 240                 // rayons : 240 = colonnes de 2 px ; 160 = 3 px (plus rapide)
#endif
#define DV_CW   (DV_W / DV_COLS)    // largeur d'une colonne de rayon
#define DSB_Y   280                 // barre de statut (40 px)
#define D_PLANE 0.80f
#define D_PROJ  (DV_W / 2 / D_PLANE)
#define D_HOR   (DV_H / 2)
#define D_MW 24
#define D_MH 20
#define GAME_EX 440                 // ✕ quitter (géré par le loop)
#define GAME_EY 2
#define GAME_EW 38
#define GAME_EH 30
#define D_FIRE_X 414
#define D_FIRE_Y 222
#define D_FIRE_R 38

// ---------------- niveaux ----------------
static const char *const D_MAPS[3][D_MH] = {
 {"TTTHTTTTTTTNTTTTTTTTTTTT",
  "T@.a..T,,,,,,,,,TCCCCCCT",
  "T.....T,m,,m,,m,T......T",
  "T..o..D,,,,z,,,,D..i..hT",
  "T.....T,m,,m,,m,T......T",
  "T..h..T,,,,,,,,,T..a...T",
  "TTTDTTTTTTTDTTTTTTTDTTTT",
  "T_____T:::::::::T,,,,,,T",
  "T__z__T::g::i:::T,,s,,,T",
  "T_____T:::::::::D,,,,,,T",
  "T__l__T:::::::::T,,,k,,T",
  "T_____T::::h::::T,,,,,,T",
  "T_____TTTTTDTTTTTTTTTTTT",
  "T_____D~~~~~~~~~T......T",
  "T_o___T~~~~~~~~~T.z..c.T",
  "T_____T~~~~r~~~~K......T",
  "T__i__T~~~~~~~~~T..o...E",
  "T_____T~~~~~~~~~T......T",
  "T_a_h_T~~~~~~~~~T.i....T",
  "TTTTTTTTTTTTTTTTTTTTTTTT"},
 {"MMMMMMMMMMMMMMMMMMMMMMMM",
  "M@..a.M::::::::::MRRRRRM",
  "M.....D:::z::z:::D,,,,,R",
  "M..h..M::::::::::M,,i,,R",
  "M.....M::M::::M::M,,,,,R",
  "MMMDMMMMMMMMMMMMMM,,s,,R",
  "M,,,,,M~~~~~~~~~~M,,,,,R",
  "M,,z,,M~~~~~~~~~~M,,h,,R",
  "M,,,,,D~~~~r~~~~~MRRDRRR",
  "M,,c,,M~~~~~~~~~~M.....M",
  "M,,,,,M~~~~~~~~~~M..i..M",
  "MMMDMHMMMHMMMMHMMM..o..M",
  "M________________M.....M",
  "M___i_____z______M..z..M",
  "M_______o________MMMKMMM",
  "M__a_____k_______M.....M",
  "M_______l________M.z.c.M",
  "M____h______z____M.....E",
  "M________________M..h..M",
  "MMMMHMMMMHMMMMHMMMMMMMMM"},
 {"SSSSSSSSSSSSSSSSSSSSSSSS",
  "S@.a.cS________________S",
  "S..h..D________________S",
  "S.....S___o________o___S",
  "SSSDSSS________________S",
  "S,,,,,S______BBBB______S",
  "S,,z,,S______B::B______S",
  "S,,,,,S______B::B______S",
  "S,,r,,S______BBBB______S",
  "S,,,,,D________________S",
  "S,i,,,S_____i____i_____S",
  "SSSSSSS________________S",
  "S:::::S________X_______S",
  "S:h:c:D________________S",
  "S:::::S___o________o___S",
  "SSSSSSS________________S",
  "S______________________S",
  "S__s___h____c_____a__s_S",
  "S______________________E",
  "SSSSSSSSSSSSSSSSSSSSSSSS"}
};
static const char *const D_LVLNAME[3] = {"BLOC 1 : LA SALLE DES MINEURS", "BLOC 2 : LA BANQUE CENTRALE", "BLOC 3 : LA PLANCHE A BILLETS"};

// ---------------- état du monde ----------------
#define W_DOOR 200
#define W_EXIT 201
#define Z_SLUDGE 1
static uint8_t dWall[D_MH][D_MW], dFloorT[D_MH][D_MW], dCeilT[D_MH][D_MW], dLightC[D_MH][D_MW], dZoneF[D_MH][D_MW];
static int8_t  dDoorIdx[D_MH][D_MW];
struct DDoor { int8_t x, y; bool vert, locked; float open; int8_t dir; unsigned long openedMs; };
static DDoor dDoor[16]; static int dNDoor = 0;
static bool dExitOn = false; static int dExitX = -1, dExitY = -1;

enum { K_NONE = 0, K_ZOMBIE, K_IMP, K_SLIME, K_BOSS, K_MED, K_SATS, K_SHELLS, K_ARMOR, K_KEY, K_SHOTGUN, K_BARREL, K_LAMP, K_RIG };
enum { ST_IDLE = 0, ST_CHASE, ST_ATK, ST_PAIN, ST_DYING, ST_DEAD, ST_GONE };
struct DThing { float x, y; uint8_t kind, st; int16_t hp; unsigned long stMs, atkMs, losMs, wanderUntil; float wx, wy; bool awake, los; };
#define D_MAXT 64
static DThing dTh[D_MAXT]; static int dNT = 0;
struct DProj { float x, y, vx, vy; uint8_t kind; bool on; unsigned long t0; };
#define D_MAXP 20
static DProj dPr[D_MAXP];
struct DFx { float x, y, h; uint8_t spr, spr2; unsigned long t0; uint16_t life; bool on; float sc; };
#define D_MAXFX 24
static DFx dFx[D_MAXFX];

enum { DS_PLAY = 0, DS_DEAD, DS_TALLY, DS_WIN };
static float dPX = 1.5f, dPY = 1.5f, dPA = 0;
static int dHealth = 100, dArmor = 0, dBullets = 50, dShells = 0;
static bool dHasSG = false, dHasKey = false, dMapOn = false;
static uint8_t dWeapon = 0, dLvl = 0, dState = DS_PLAY;
static int dKills = 0, dKillsTot = 0, dItems = 0, dItemsTot = 0;
static unsigned long dLvlStart = 0, dLvlTime = 0, dStateMs = 0, dHurtMs = 0, dPickMs = 0, dShotMs = 0,
                     dKillMs = 0, dSludgeMs = 0, dFlashMs = 0, dMsgMs = 0, dExitMs = 0;
static int dHurtAmt = 0;
static float dBobPh = 0, dBobAmp = 0;
static char dMsg[48] = "";
char dmPopup[40] = "";              // écrit par le loop (nouveau bloc)
unsigned long dmPopupMs = 0;
bool doomReset = true;              // true = nouvelle partie au prochain rendu
static float dZbuf[DV_COLS];
static float dRowDist[DV_H];

static void dMsgSet(const char *m) { strlcpy(dMsg, m, sizeof(dMsg)); dMsgMs = millis(); }
static inline float dRandF() { return (random(0, 10000)) / 10000.0f; }

// ---------------- sons (bruit blanc = freq 0, cf. sndTask) ----------------
static void dSfx(uint8_t id) {
  switch (id) {
    case 0: playNote(0, 110, 55, SND_UI); break;                                    // pistolet
    case 1: playNote(0, 260, 80, SND_UI); playNote(70, 150, 45, SND_UI); break;     // fusil
    case 2: playNote(0, 90, 28, SND_UI); break;                                     // tir ennemi
    case 3: playNote(330, 110, 30, SND_UI); break;                                  // boule de feu
    case 4: playNote(140, 240, 35, SND_UI); break;                                  // porte
    case 5: playNote(1320, 50, 30, SND_UI); playNote(1760, 70, 30, SND_UI); break;  // bonus
    case 6: playNote(190, 140, 45, SND_UI); break;                                  // aïe
    case 7: playNote(260, 90, 30, SND_UI); break;                                   // douleur monstre
    case 8: playNote(160, 280, 40, SND_UI); break;                                  // mort monstre
    case 9: playNote(110, 450, 55, SND_UI); playNote(80, 650, 55, SND_UI); break;   // mort joueur
    case 10: playNote(600, 80, 40, SND_UI); playNote(900, 140, 40, SND_UI); break;  // interrupteur
    case 11: playNote(0, 420, 90, SND_UI); playNote(55, 280, 70, SND_UI); break;    // explosion
    case 12: playNote(210, 170, 28, SND_UI); break;                                 // grognement
    case 13: playNote(880, 60, 30, SND_UI); playNote(1100, 60, 30, SND_UI); playNote(1480, 120, 35, SND_UI); break; // arme
  }
}

// ---------------- chargement d'un niveau ----------------
static bool dIsWallCh(char c) { return strchr("T#SBCHNRMDKE", c) != nullptr; }
static bool dIsFloorCh(char c) { return strchr("._,:~", c) != nullptr; }
static int dWallTex(char c) {
  switch (c) { case 'T': return TX_TECH; case '#': return TX_BRICK; case 'S': return TX_SUPPORT; case 'B': return TX_BTC;
               case 'C': return TX_COMPUTER; case 'H': return TX_HODL; case 'N': return TX_STACK; case 'R': return TX_REDBRICK;
               case 'M': return TX_MARBLE; } return TX_TECH;
}
static void dZone(int x, int y, char z) {
  switch (z) {
    case ',': dFloorT[y][x] = TX_GRATE; dCeilT[y][x] = TX_CEIL; dLightC[y][x] = 9; break;
    case ':': dFloorT[y][x] = TX_FLOOR; dCeilT[y][x] = TX_CEILLIGHT; dLightC[y][x] = 15; break;
    case '~': dFloorT[y][x] = TX_SLUDGE; dCeilT[y][x] = TX_CEIL; dLightC[y][x] = 12; dZoneF[y][x] = Z_SLUDGE; break;
    case '_': dFloorT[y][x] = TX_DIRT; dCeilT[y][x] = 255; dLightC[y][x] = 14; break;
    default:  dFloorT[y][x] = TX_FLOOR; dCeilT[y][x] = TX_CEIL; dLightC[y][x] = 11; break;
  }
}
static void dAddThing(float x, float y, uint8_t k) {
  if (dNT >= D_MAXT) return;
  DThing &t = dTh[dNT++];
  memset(&t, 0, sizeof(t));
  t.x = x; t.y = y; t.kind = k; t.st = ST_IDLE;
  t.hp = k == K_ZOMBIE ? 20 : k == K_IMP ? 60 : k == K_SLIME ? 45 : k == K_BOSS ? 700 : k == K_BARREL ? 20 : 1;
  t.atkMs = millis();
  if (k >= K_ZOMBIE && k <= K_BOSS) dKillsTot++;
  if (k >= K_MED && k <= K_SHOTGUN) dItemsTot++;
}
static void dLoadLevel(int lv) {
  memset(dWall, 0, sizeof(dWall)); memset(dZoneF, 0, sizeof(dZoneF)); memset(dDoorIdx, -1, sizeof(dDoorIdx));
  dNDoor = 0; dNT = 0; dKills = dKillsTot = dItems = dItemsTot = 0; dExitOn = false; dExitX = dExitY = -1;
  for (int i = 0; i < D_MAXP; i++) dPr[i].on = false;
  for (int i = 0; i < D_MAXFX; i++) dFx[i].on = false;
  dLvl = lv; dHasKey = false; dMapOn = false;
  const char *const *L = D_MAPS[lv];
  for (int y = 0; y < D_MH; y++)
    for (int x = 0; x < D_MW; x++) {
      char c = L[y][x];
      char z = '.';
      if (dIsFloorCh(c)) z = c;
      else if (x > 0 && dIsFloorCh(L[y][x - 1])) z = L[y][x - 1];
      else if (x < D_MW - 1 && dIsFloorCh(L[y][x + 1])) z = L[y][x + 1];
      else if (y > 0 && dIsFloorCh(L[y - 1][x])) z = L[y - 1][x];
      dZone(x, y, z);
      float cx = x + 0.5f, cy = y + 0.5f;
      switch (c) {
        case 'D': case 'K': {
          dWall[y][x] = W_DOOR;
          if (dNDoor < 16) {
            DDoor &d = dDoor[dNDoor];
            d.x = x; d.y = y; d.locked = (c == 'K'); d.open = 0; d.dir = 0; d.openedMs = 0;
            d.vert = (y > 0 && y < D_MH - 1 && dIsWallCh(L[y - 1][x]) && dIsWallCh(L[y + 1][x]));
            dDoorIdx[y][x] = dNDoor++;
          }
        } break;
        case 'E': dWall[y][x] = W_EXIT; dExitX = x; dExitY = y; break;
        case '@': dPX = cx; dPY = cy; break;
        case 'z': dAddThing(cx, cy, K_ZOMBIE); break;
        case 'i': dAddThing(cx, cy, K_IMP); break;
        case 's': dAddThing(cx, cy, K_SLIME); break;
        case 'X': dAddThing(cx, cy, K_BOSS); break;
        case 'h': dAddThing(cx, cy, K_MED); break;
        case 'a': dAddThing(cx, cy, K_SATS); break;
        case 'c': dAddThing(cx, cy, K_SHELLS); break;
        case 'r': dAddThing(cx, cy, K_ARMOR); break;
        case 'k': dAddThing(cx, cy, K_KEY); break;
        case 'g': dAddThing(cx, cy, K_SHOTGUN); break;
        case 'o': dAddThing(cx, cy, K_BARREL); break;
        case 'l': dAddThing(cx, cy, K_LAMP); dLightC[y][x] = 15; break;
        case 'm': dAddThing(cx, cy, K_RIG); break;
        default: if (dIsWallCh(c)) dWall[y][x] = dWallTex(c) + 1; break;
      }
    }
  // orientation de départ : la direction la plus dégagée
  int bestRun = -1; const float ang[4] = {0, PI / 2, PI, -PI / 2};
  for (int d = 0; d < 4; d++) {
    int run = 0, x = (int)dPX, y = (int)dPY;
    for (;;) { x += (d == 0) - (d == 2); y += (d == 1) - (d == 3); if (dWall[y][x]) break; run++; }
    if (run > bestRun) { bestRun = run; dPA = ang[d]; }
  }
  dState = DS_PLAY; dLvlStart = millis(); dMsgSet(D_LVLNAME[lv]);
}
static void dNewGame() {
  dHealth = 100; dArmor = 0; dBullets = 50; dShells = 0; dHasSG = false; dWeapon = 0;
  dLoadLevel(0);
}

// ---------------- collisions ----------------
static bool dSolidCell(int x, int y) {
  if ((unsigned)x >= D_MW || (unsigned)y >= D_MH) return true;
  uint8_t w = dWall[y][x];
  if (!w) return false;
  if (w == W_DOOR) return dDoor[dDoorIdx[y][x]].open < 0.85f;
  return true;
}
static float dThingRad(const DThing &t) {
  if (t.kind >= K_ZOMBIE && t.kind <= K_BOSS) return (t.st < ST_DYING) ? (t.kind == K_BOSS ? 0.7f : 0.32f) : 0;
  if (t.kind == K_BARREL) return t.st < ST_DYING ? 0.3f : 0;
  if (t.kind == K_LAMP || t.kind == K_RIG) return 0.3f;
  return 0;
}
// libre ? (r = rayon ; self = index de l'objet qui bouge, -1 = joueur)
static bool dFree(float ox, float oy, float x, float y, float r, int self) {
  if (dSolidCell((int)(x - r), (int)(y - r)) || dSolidCell((int)(x + r), (int)(y - r)) ||
      dSolidCell((int)(x - r), (int)(y + r)) || dSolidCell((int)(x + r), (int)(y + r))) return false;
  for (int i = 0; i < dNT; i++) {
    if (i == self) continue;
    float tr = dThingRad(dTh[i]);
    if (tr <= 0) continue;
    float dx = x - dTh[i].x, dy = y - dTh[i].y, n = dx * dx + dy * dy, lim = (r + tr) * (r + tr);
    float odx = ox - dTh[i].x, ody = oy - dTh[i].y;
    if (n < lim && n < odx * odx + ody * ody) return false;      // on peut toujours s'éloigner
  }
  if (self >= 0) {                                                 // les monstres ne traversent pas le joueur
    float dx = x - dPX, dy = y - dPY, odx = ox - dPX, ody = oy - dPY;
    if (dx * dx + dy * dy < (r + 0.28f) * (r + 0.28f) && dx * dx + dy * dy < odx * odx + ody * ody) return false;
  }
  return true;
}
// ligne de vue (échantillonnage tous les 0,2 case)
static bool dLOS(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0, dy = y1 - y0, d = sqrtf(dx * dx + dy * dy);
  int n = (int)(d / 0.2f);
  for (int i = 1; i < n; i++) { float k = i / (float)n; if (dSolidCell((int)(x0 + dx * k), (int)(y0 + dy * k))) return false; }
  return true;
}
// distance au premier mur dans une direction (tirs)
static float dTraceWall(float x, float y, float a) {
  float ca = cosf(a) * 0.05f, sa = sinf(a) * 0.05f, d = 0;
  for (int i = 0; i < 600; i++) { x += ca; y += sa; d += 0.05f; if (dSolidCell((int)x, (int)y)) return d; }
  return d;
}

// ---------------- effets / projectiles ----------------
static void dAddFx(float x, float y, float h, uint8_t spr, uint8_t spr2, uint16_t life, float sc) {
  for (int i = 0; i < D_MAXFX; i++) if (!dFx[i].on) {
    dFx[i] = {x, y, h, spr, spr2, millis(), life, true, sc}; return;
  }
}
static void dFireProj(float x, float y, float a, uint8_t kind, float sp) {
  for (int i = 0; i < D_MAXP; i++) if (!dPr[i].on) {
    dPr[i] = {x + cosf(a) * 0.4f, y + sinf(a) * 0.4f, cosf(a) * sp, sinf(a) * sp, kind, true, millis()}; return;
  }
}

// ---------------- dégâts ----------------
static void dHurtPlayer(int dmg) {
  if (dState != DS_PLAY || dmg <= 0) return;
  int sav = dArmor > 0 ? dmg / 3 : 0; if (sav > dArmor) sav = dArmor;
  dArmor -= sav; dmg -= sav;
  dHealth -= dmg; dHurtMs = millis(); dHurtAmt = dmg;
  if (dHealth <= 0) { dHealth = 0; dState = DS_DEAD; dStateMs = millis(); dSfx(9); }
  else dSfx(6);
}
static void dExplode(int i);
static void dDamage(int i, int dmg) {
  DThing &t = dTh[i];
  if (t.st >= ST_DYING) return;
  if (t.kind == K_BARREL) { t.hp -= dmg; if (t.hp <= 0) dExplode(i); return; }
  if (t.kind < K_ZOMBIE || t.kind > K_BOSS) return;
  t.hp -= dmg; t.awake = true;
  if (t.hp <= 0) {
    t.st = ST_DYING; t.stMs = millis(); dKills++; dKillMs = millis(); dSfx(t.kind == K_BOSS ? 11 : 8);
    if (t.kind == K_BOSS) { dMsgSet("LA PLANCHE A BILLETS EST DETRUITE !"); speak("Money printer destroyed", SND_UI); }
    return;
  }
  float painCh = t.kind == K_ZOMBIE ? 0.8f : t.kind == K_IMP ? 0.55f : t.kind == K_SLIME ? 0.5f : 0.08f;
  if (dRandF() < painCh) { t.st = ST_PAIN; t.stMs = millis(); dSfx(7); }
  else if (t.st == ST_IDLE) t.st = ST_CHASE;
}
static void dExplode(int i) {
  DThing &b = dTh[i];
  b.st = ST_DYING; b.stMs = millis();
  dSfx(11); dFlashMs = millis();
  for (int j = 0; j < dNT; j++) {                    // souffle : monstres + autres tonneaux (réaction en chaîne)
    if (j == i) continue;
    float dx = dTh[j].x - b.x, dy = dTh[j].y - b.y, d = sqrtf(dx * dx + dy * dy);
    if (d < 2.3f) dDamage(j, (int)(70 * (1 - d / 2.3f)) + 5);
  }
  float dx = dPX - b.x, dy = dPY - b.y, d = sqrtf(dx * dx + dy * dy);
  if (d < 2.3f) dHurtPlayer((int)(55 * (1 - d / 2.3f)));
}
// tir instantané du joueur (hitscan)
static void dHitscan(float a, int dmin, int dmax) {
  float wd = dTraceWall(dPX, dPY, a);
  float ca = cosf(a), sa = sinf(a), bd = wd;
  int best = -1;
  for (int i = 0; i < dNT; i++) {
    const DThing &t = dTh[i];
    bool shoot = ((t.kind >= K_ZOMBIE && t.kind <= K_BOSS) || t.kind == K_BARREL) && t.st < ST_DYING;
    if (!shoot) continue;
    float dx = t.x - dPX, dy = t.y - dPY, along = dx * ca + dy * sa;
    if (along <= 0.1f || along >= bd) continue;
    float perp = fabsf(-dx * sa + dy * ca), rad = t.kind == K_BOSS ? 0.75f : 0.4f;
    if (perp < rad) { bd = along; best = i; }
  }
  float hx = dPX + ca * (bd - 0.08f), hy = dPY + sa * (bd - 0.08f);
  if (best >= 0) {
    dDamage(best, dmin + random(0, dmax - dmin + 1));
    dAddFx(hx, hy, 0.45f + dRandF() * 0.2f, dTh[best].kind == K_BARREL ? SP_PUFF : SP_BLOOD, SP_BLOOD, 260, 0.3f);
  } else dAddFx(hx, hy, 0.4f + dRandF() * 0.25f, SP_PUFF, SP_PUFF, 300, 0.32f);
}
static void dShoot(unsigned long now) {
  if (dWeapon == 1 && dShells <= 0) dWeapon = 0;
  if (dWeapon == 0 && dBullets <= 0) { if (dHasSG && dShells > 0) dWeapon = 1; else { if (now - dMsgMs > 1500) dMsgSet("PLUS DE MUNITIONS"); return; } }
  unsigned long cd = dWeapon ? 950 : 380;
  if (now - dShotMs < cd) return;
  dShotMs = now; dFlashMs = now;
  if (dWeapon == 0) { dBullets--; dHitscan(dPA + (dRandF() - 0.5f) * 0.03f, 5, 15); dSfx(0); }
  else { dShells--; for (int p = 0; p < 7; p++) dHitscan(dPA + (dRandF() - 0.5f) * 0.2f, 5, 15); dSfx(1); }
}

// ---------------- portes ----------------
static void dTryOpen(int i, bool player) {
  DDoor &d = dDoor[i];
  if (d.locked) {
    if (!player) return;
    if (!dHasKey) { if (millis() - dMsgMs > 2000) dMsgSet("IL FAUT LA CLE ORANGE"); return; }
    d.locked = false; dMsgSet("PORTE DEVERROUILLEE");
  }
  if (d.open < 1.0f && d.dir <= 0) { d.dir = 1; dSfx(4); }
  if (d.open >= 1.0f) d.openedMs = millis();
}
static void dUpdateDoors(float dt, unsigned long now) {
  for (int i = 0; i < dNDoor; i++) {
    DDoor &d = dDoor[i];
    float cx = d.x + 0.5f, cy = d.y + 0.5f;
    bool busy = fabsf(dPX - cx) < 0.8f && fabsf(dPY - cy) < 0.8f;
    for (int j = 0; j < dNT && !busy; j++) if (dThingRad(dTh[j]) > 0 && fabsf(dTh[j].x - cx) < 0.8f && fabsf(dTh[j].y - cy) < 0.8f) busy = true;
    if (d.dir > 0) { d.open += dt * 1.7f; if (d.open >= 1) { d.open = 1; d.dir = 0; d.openedMs = now; } }
    else if (d.dir < 0) { if (busy) d.dir = 1; else { d.open -= dt * 1.7f; if (d.open <= 0) { d.open = 0; d.dir = 0; } } }
    else if (d.open >= 1 && now - d.openedMs > 4000) { if (busy) d.openedMs = now; else { d.dir = -1; dSfx(4); } }
  }
}

// ---------------- IA des monstres ----------------
static float dSpeedOf(uint8_t k) { return k == K_ZOMBIE ? 1.0f : k == K_IMP ? 1.3f : k == K_SLIME ? 1.8f : 0.75f; }
static void dMonAttack(DThing &t, float dist, float a, unsigned long now) {
  t.st = ST_ATK; t.stMs = now; t.atkMs = now;
  switch (t.kind) {
    case K_ZOMBIE: dSfx(2); if (dRandF() < 0.75f - dist * 0.035f) dHurtPlayer(3 + random(0, 10)); break;
    case K_IMP:    if (dist < 1.3f) dHurtPlayer(3 + random(0, 13)); else { dFireProj(t.x, t.y, a, 0, 4.2f); dSfx(3); } break;
    case K_SLIME:  dHurtPlayer(4 + random(0, 11)); break;
    case K_BOSS:
      if (dist < 2.0f) dHurtPlayer(15 + random(0, 16));
      else { for (int k = -1; k <= 1; k++) dFireProj(t.x, t.y, a + k * 0.22f, 1, 5.0f); dSfx(3); }
      break;
  }
}
static void dUpdateThings(float dt, unsigned long now) {
  for (int i = 0; i < dNT; i++) {
    DThing &t = dTh[i];
    if (t.kind == K_BARREL) { if (t.st == ST_DYING && now - t.stMs > 520) t.st = ST_GONE; continue; }
    if (t.kind < K_ZOMBIE || t.kind > K_BOSS) continue;
    if (t.st == ST_DYING) { if (now - t.stMs > 520) t.st = ST_DEAD; continue; }
    if (t.st >= ST_DEAD) continue;
    float dx = dPX - t.x, dy = dPY - t.y, dist = sqrtf(dx * dx + dy * dy), a = atan2f(dy, dx);
    if (now - t.losMs > 220) { t.losMs = now - (i % 5) * 11; t.los = dist < 16 && dLOS(t.x, t.y, dPX, dPY); }
    if (dState != DS_PLAY) { if (t.st == ST_ATK || t.st == ST_CHASE) t.st = ST_IDLE; continue; }
    switch (t.st) {
      case ST_IDLE:
        if (t.los || (now - dShotMs < 150 && dist < 11) || t.awake) { t.st = ST_CHASE; t.awake = true; t.atkMs = now - 600; if (t.los) dSfx(12); }
        break;
      case ST_PAIN: if (now - t.stMs > 240) t.st = ST_CHASE; break;
      case ST_ATK:  if (now - t.stMs > (t.kind == K_BOSS ? 520u : 380u)) t.st = ST_CHASE; break;
      case ST_CHASE: {
        unsigned long cd = t.kind == K_ZOMBIE ? 1500 : t.kind == K_IMP ? 1900 : t.kind == K_SLIME ? 800 : 1250;
        float range = t.kind == K_SLIME ? 1.1f : (t.kind == K_ZOMBIE ? 11.0f : 14.0f);
        if (t.kind == K_IMP && dist < 1.3f) { range = 1.3f; cd = 900; }
        if (t.los && dist < range && now - t.atkMs > cd + (unsigned long)(i * 37 % 400)) { dMonAttack(t, dist, a, now); break; }
        float mvx, mvy;
        if (now < t.wanderUntil) { mvx = t.wx; mvy = t.wy; }
        else if (dist > (t.kind == K_SLIME ? 0.7f : 1.2f)) { mvx = dx / dist; mvy = dy / dist; }
        else break;
        float sp = dSpeedOf(t.kind) * dt, nx = t.x + mvx * sp, ny = t.y + mvy * sp, r = t.kind == K_BOSS ? 0.55f : 0.3f;
        // porte devant ? on l'ouvre
        int cxn = (int)(t.x + mvx * 0.7f), cyn = (int)(t.y + mvy * 0.7f);
        if ((unsigned)cxn < D_MW && (unsigned)cyn < D_MH && dWall[cyn][cxn] == W_DOOR) dTryOpen(dDoorIdx[cyn][cxn], false);
        bool moved = false;
        if (dFree(t.x, t.y, nx, ny, r, i)) { t.x = nx; t.y = ny; moved = true; }
        else if (dFree(t.x, t.y, nx, t.y, r, i)) { t.x = nx; moved = true; }
        else if (dFree(t.x, t.y, t.x, ny, r, i)) { t.y = ny; moved = true; }
        if (!moved) {                                   // bloqué : contournement latéral
          float s = (random(0, 2) ? 1 : -1);
          t.wx = -mvy * s; t.wy = mvx * s; t.wanderUntil = now + 450 + random(0, 400);
        }
      } break;
    }
  }
}
static void dUpdateProj(float dt, unsigned long now) {
  for (int i = 0; i < D_MAXP; i++) {
    DProj &p = dPr[i];
    if (!p.on) continue;
    float nx = p.x + p.vx * dt, ny = p.y + p.vy * dt;
    if (dSolidCell((int)nx, (int)ny) || now - p.t0 > 5000) {
      p.on = false; dAddFx(p.x, p.y, 0.5f, SP_BOOM1, SP_PUFF, 320, 0.45f); continue;
    }
    float dx = dPX - nx, dy = dPY - ny;
    if (dx * dx + dy * dy < 0.2f && dState == DS_PLAY) {
      p.on = false; dHurtPlayer(p.kind == 0 ? 6 + random(0, 15) : 8 + random(0, 11));
      dAddFx(nx, ny, 0.5f, SP_BOOM1, SP_PUFF, 300, 0.45f); continue;
    }
    p.x = nx; p.y = ny;
  }
}

// ---------------- entrées tactiles ----------------
static struct DStick { bool on; int bx, by, kx, ky; } dJL = {false, 0, 0, 0, 0}, dJR = {false, 0, 0, 0, 0};
static bool dFireHeld = false;
static uint8_t dZonesPrev = 0;
enum { DZ_MAP = 1, DZ_ARMS = 2, DZ_ANY = 4 };
static uint8_t dTaps = 0;              // zones touchées CETTE frame (front montant)
static void dReadInput() {
  uint16_t xs[3], ys[3]; uint8_t ev[3];
  int n = readTouchMulti(xs, ys, ev, 3);
  bool seenL = false, seenR = false; uint8_t zones = 0; dFireHeld = false;
  for (int f = 0; f < n; f++) {
    if (ev[f] == 1) continue;                                         // doigt relevé
    int x = xs[f], y = ys[f];
    zones |= DZ_ANY;
    if (x >= GAME_EX && y <= GAME_EY + GAME_EH) continue;             // ✕ : géré par le loop
    if (x < 52 && y < 34) { zones |= DZ_MAP; continue; }
    if (y >= DSB_Y) { if (x >= 384) zones |= DZ_ARMS; continue; }
    int fdx = x - D_FIRE_X, fdy = y - D_FIRE_Y;
    if (fdx * fdx + fdy * fdy < (D_FIRE_R + 8) * (D_FIRE_R + 8)) { dFireHeld = true; continue; }
    DStick &s = (x < DV_W / 2) ? dJL : dJR;
    bool &seen = (x < DV_W / 2) ? seenL : seenR;
    if (!s.on) { s.on = true; s.bx = x; s.by = y; }
    s.kx = x; s.ky = y; seen = true;
  }
  if (!seenL) dJL.on = false;
  if (!seenR) dJR.on = false;
  dTaps = zones & ~dZonesPrev;
  dZonesPrev = zones;
}

// ---------------- joueur ----------------
static void dUpdatePlayer(float dt, unsigned long now) {
  if (dJR.on) { float r = constrain((dJR.kx - dJR.bx) / 60.0f, -1.0f, 1.0f); dPA += r * (0.35f + 0.65f * fabsf(r)) * 3.0f * dt; }
  float fwd = 0, str = 0;
  if (dJL.on) { fwd = constrain(-(dJL.ky - dJL.by) / 45.0f, -1.0f, 1.0f); str = constrain((dJL.kx - dJL.bx) / 50.0f, -1.0f, 1.0f); }
  float sp = 3.3f * dt, ca = cosf(dPA), sa = sinf(dPA);
  float mx = (ca * fwd - sa * str) * sp, my = (sa * fwd + ca * str) * sp;
  if (dFree(dPX, dPY, dPX + mx, dPY, 0.25f, -1)) dPX += mx;
  if (dFree(dPX, dPY, dPX, dPY + my, 0.25f, -1)) dPY += my;
  float mv = min(1.0f, fabsf(fwd) + fabsf(str));
  dBobAmp += (mv - dBobAmp) * min(1.0f, dt * 8); dBobPh += dt * 10.0f * mv;
  // portes proches : ouverture automatique
  for (int i = 0; i < dNDoor; i++) {
    float dx = dDoor[i].x + 0.5f - dPX, dy = dDoor[i].y + 0.5f - dPY;
    if (dx * dx + dy * dy < 1.3f * 1.3f && (dx * ca + dy * sa) > -0.2f) dTryOpen(i, true);
  }
  // interrupteur de sortie
  if (dExitX >= 0 && !dExitOn) {
    float dx = dExitX + 0.5f - dPX, dy = dExitY + 0.5f - dPY;
    if (dx * dx + dy * dy < 1.2f * 1.2f) {
      bool bossAlive = false;
      for (int i = 0; i < dNT; i++) if (dTh[i].kind == K_BOSS && dTh[i].st < ST_DYING) bossAlive = true;
      if (bossAlive) { if (now - dMsgMs > 2500) dMsgSet("DETRUIS D'ABORD LA PLANCHE A BILLETS"); }
      else { dExitOn = true; dExitMs = now; dSfx(10); dLvlTime = now - dLvlStart; }
    }
  }
  if (dExitOn && now - dExitMs > 700 && dState == DS_PLAY) { dState = (dLvl == 2) ? DS_WIN : DS_TALLY; dStateMs = now; }
  // ramassage
  for (int i = 0; i < dNT; i++) {
    DThing &t = dTh[i];
    if (t.kind < K_MED || t.kind > K_SHOTGUN || t.st == ST_GONE) continue;
    float dx = t.x - dPX, dy = t.y - dPY;
    if (dx * dx + dy * dy > 0.55f * 0.55f) continue;
    bool take = true;
    switch (t.kind) {
      case K_MED:    if (dHealth >= 100) take = false; else { dHealth = min(100, dHealth + 25); dMsgSet("+25 SANTE"); } break;
      case K_SATS:   if (dBullets >= 200) take = false; else { dBullets = min(200, dBullets + 20); dMsgSet("+20 SATS (munitions)"); } break;
      case K_SHELLS: if (dShells >= 50) take = false; else { dShells = min(50, dShells + 8); dMsgSet("+8 CARTOUCHES LIGHTNING"); } break;
      case K_ARMOR:  if (dArmor >= 100) take = false; else { dArmor = 100; dMsgSet("GILET ORANGE : ARMURE 100%"); } break;
      case K_KEY:    dHasKey = true; dMsgSet("CLE ORANGE !"); break;
      case K_SHOTGUN: dHasSG = true; dShells = min(50, dShells + 8); dWeapon = 1; dMsgSet("FUSIL A POMPE LIGHTNING !"); dSfx(13); break;
    }
    if (take) { t.st = ST_GONE; dItems++; dPickMs = now; if (t.kind != K_SHOTGUN) dSfx(5); }
  }
  // boue toxique
  int cx = (int)dPX, cy = (int)dPY;
  if ((dZoneF[cy][cx] & Z_SLUDGE) && now - dSludgeMs > 1000) { dSludgeMs = now; dHurtPlayer(5); }
  if (dFireHeld) dShoot(now);
}

// ---------------- rendu 3D ----------------
static int dPalette(unsigned long now) {
  if (dState == DS_DEAD) return 2;
  if (now - dHurtMs < 420) return (now - dHurtMs < 160 && dHurtAmt >= 10) ? 2 : 1;
  if (now - dPickMs < 220) return 3;
  return 0;
}
static void dRender3D(unsigned long now) {
  uint16_t *fb = gfx->getFramebuffer();
  const uint16_t *cmP = dCM + dPalette(now) * D_NL * 256;
  int flash = (now - dFlashMs < 90) ? 4 : 0;
  float dirX = cosf(dPA), dirY = sinf(dPA), plX = -dirY * D_PLANE, plY = dirX * D_PLANE;
  for (int y = 0; y < DV_H; y++) { float dy = (y >= D_HOR) ? (y - D_HOR + 0.5f) : (D_HOR - y - 0.5f); dRowDist[y] = 0.5f * D_PROJ / dy; }
  float sl = (now % 8000) * 0.008f;                         // défilement de la boue
  for (int c = 0; c < DV_COLS; c++) {
    float camX = (2.0f * c + 1.0f) / DV_COLS - 1.0f;
    float rdx = dirX + plX * camX, rdy = dirY + plY * camX;
    int mx = (int)dPX, my = (int)dPY;
    float ddx = fabsf(1.0f / (fabsf(rdx) < 1e-6f ? 1e-6f : rdx)), ddy = fabsf(1.0f / (fabsf(rdy) < 1e-6f ? 1e-6f : rdy));
    int sx = rdx < 0 ? -1 : 1, sy = rdy < 0 ? -1 : 1;
    float sdx = rdx < 0 ? (dPX - mx) * ddx : (mx + 1.0f - dPX) * ddx;
    float sdy = rdy < 0 ? (dPY - my) * ddy : (my + 1.0f - dPY) * ddy;
    int side = 0, tex = TX_TECH, lcx = mx, lcy = my; float perp = 30, u = 0; bool flip = true;
    for (int it = 0; it < 64; it++) {
      int px = mx, py = my;
      if (sdx < sdy) { sdx += ddx; mx += sx; side = 0; } else { sdy += ddy; my += sy; side = 1; }
      if ((unsigned)mx >= D_MW || (unsigned)my >= D_MH) { perp = side == 0 ? sdx - ddx : sdy - ddy; lcx = px; lcy = py; break; }
      uint8_t w = dWall[my][mx];
      if (!w) continue;
      if (w == W_DOOR) {                                    // porte : plan médian de la case
        const DDoor &d = dDoor[dDoorIdx[my][mx]];
        float t, uu;
        if (d.vert && side == 0) { t = sdx - ddx * 0.5f; uu = dPY + t * rdy; if ((int)floorf(uu) != my) continue; uu -= my; }
        else if (!d.vert && side == 1) { t = sdy - ddy * 0.5f; uu = dPX + t * rdx; if ((int)floorf(uu) != mx) continue; uu -= mx; }
        else continue;
        if (uu < d.open) continue;                          // partie ouverte : le rayon passe
        perp = t; u = uu - d.open; tex = d.locked ? TX_KEYDOOR : TX_DOOR; lcx = px; lcy = py; flip = false; break;
      }
      perp = side == 0 ? sdx - ddx : sdy - ddy;
      float wx = side == 0 ? dPY + perp * rdy : dPX + perp * rdx;
      u = wx - floorf(wx);
      tex = (w == W_EXIT) ? (dExitOn ? TX_EXIT_ON : TX_EXIT_OFF) : w - 1;
      lcx = px; lcy = py; break;
    }
    if (perp < 0.04f) perp = 0.04f;
    dZbuf[c] = perp;
    int tx = (int)(u * TEXS); if (tx > 63) tx = 63; if (tx < 0) tx = 0;
    if (flip && ((side == 0 && rdx > 0) || (side == 1 && rdy < 0))) tx = 63 - tx;
    float lineH = D_PROJ / perp, y0f = D_HOR - lineH * 0.5f;
    int ds = (int)ceilf(y0f); if (ds < 0) ds = 0;
    int de = (int)(D_HOR + lineH * 0.5f); if (de > DV_H - 1) de = DV_H - 1;
    int L = dLightC[lcy][lcx] - (int)(perp * 0.45f) + (side ? -1 : 1) + flash;
    L = constrain(L, 0, D_NL - 1);
    const uint16_t *cm = cmP + L * 256;
    const uint8_t *tc = dTex + tex * 4096 + tx * 64;
    uint16_t *top = fb + (DV_CW * c) * PANEL_W + (PANEL_W - 1);       // pointe sur y = 0
    float step = TEXS / lineH, tp = (ds - y0f) * step;
    for (int y = ds; y <= de; y++) { top[-y] = cm[tc[(int)tp & 63]]; tp += step; }
    // plafond / ciel
    int skyU = (int)((atan2f(rdy, rdx) + PI) * (SKY_W * 2 / (2 * PI))) & (SKY_W - 1);
    const uint8_t *skyCol = dSky + skyU * SKY_H;
    for (int y = 0; y < ds; y++) {
      float d = dRowDist[y], fx = dPX + rdx * d, fy = dPY + rdy * d;
      int cx = (int)fx, cy = (int)fy;
      if ((unsigned)cx >= D_MW || (unsigned)cy >= D_MH) { top[-y] = 0; continue; }
      uint8_t ct = dCeilT[cy][cx];
      if (ct == 255) { top[-y] = cmP[(D_NL - 1) * 256 + skyCol[min(SKY_H - 1, y * SKY_H / D_HOR)]]; continue; }
      int Lf = constrain(dLightC[cy][cx] - (int)(d * 0.45f) + flash, 0, D_NL - 1);
      top[-y] = cmP[Lf * 256 + dTex[ct * 4096 + (((int)(fx * 64) & 63) << 6) + ((int)(fy * 64) & 63)]];
    }
    // sol
    for (int y = de + 1; y < DV_H; y++) {
      float d = dRowDist[y], fx = dPX + rdx * d, fy = dPY + rdy * d;
      int cx = (int)fx, cy = (int)fy;
      if ((unsigned)cx >= D_MW || (unsigned)cy >= D_MH) { top[-y] = 0; continue; }
      uint8_t ft = dFloorT[cy][cx];
      float ox = (ft == TX_SLUDGE) ? sl : 0;
      int Lf = constrain(dLightC[cy][cx] - (int)(d * 0.45f) + flash, 0, D_NL - 1);
      top[-y] = cmP[Lf * 256 + dTex[ft * 4096 + (((int)((fx + ox) * 64) & 63) << 6) + ((int)(fy * 64) & 63)]];
    }
    uint16_t *c0 = fb + (DV_CW * c) * PANEL_W + (PANEL_W - DV_H);      // colonnes voisines = copies
    for (int k = 1; k < DV_CW; k++) memcpy(c0 + k * PANEL_W, c0, DV_H * 2);
  }
}

// ---------------- sprites (tri arrière -> avant, z-buffer par colonne) ----------------
struct DSprite { float x, y, dist, scale, zoff; uint16_t spr; bool full; };
static int dMonFrame(const DThing &t, int i, unsigned long now) {
  int base = t.kind == K_ZOMBIE ? SP_ZOMBIE : t.kind == K_IMP ? SP_IMP : t.kind == K_SLIME ? SP_SLIME : SP_BOSS;
  switch (t.st) {
    case ST_CHASE: return base + ((((now + i * 97) / 230) & 1) ? PO_W2 : PO_W1);
    case ST_ATK:   return base + PO_ATK;
    case ST_PAIN:  return base + PO_PAIN;
    case ST_DYING: { unsigned long e = now - t.stMs; return base + (e < 170 ? PO_D1 : e < 340 ? PO_D2 : PO_DEAD); }
    case ST_DEAD:  return base + PO_DEAD;
    default:       return base + PO_W1;
  }
}
static void dDrawSprites(unsigned long now) {
  static DSprite L[D_MAXT + D_MAXP + D_MAXFX];
  int n = 0;
  for (int i = 0; i < dNT; i++) {
    const DThing &t = dTh[i];
    if (t.st == ST_GONE) continue;
    DSprite s = {t.x, t.y, 0, 0.6f, 0, 0, false};
    switch (t.kind) {
      case K_ZOMBIE: case K_IMP: case K_SLIME: case K_BOSS:
        s.spr = dMonFrame(t, i, now);
        s.scale = t.kind == K_BOSS ? 1.8f : t.kind == K_SLIME ? 0.8f : t.kind == K_IMP ? 0.95f : 0.9f;
        s.full = (t.st == ST_ATK && (t.kind == K_IMP || t.kind == K_BOSS)) || (t.kind == K_BOSS && t.st == ST_DYING); break;
      case K_BARREL:
        if (t.st == ST_DYING) { s.spr = (now - t.stMs < 250) ? SP_BOOM1 : SP_BOOM2; s.full = true; s.scale = 1.0f; }
        else { s.spr = SP_BARREL; s.scale = 0.75f; } break;
      case K_LAMP: s.spr = SP_LAMP; s.scale = 1.1f; s.full = true; break;
      case K_RIG:  s.spr = SP_RIG; s.scale = 0.8f; break;
      default: s.spr = SP_MED + (t.kind - K_MED); s.scale = 0.55f; s.full = ((now / 400) & 1) && t.kind == K_KEY; break;
    }
    L[n++] = s;
  }
  for (int i = 0; i < D_MAXP; i++) if (dPr[i].on)
    L[n++] = {dPr[i].x, dPr[i].y, 0, 0.5f, 0.25f, (uint16_t)(dPr[i].kind == 0 ? (((now / 90) & 1) ? SP_FIRE1 : SP_FIRE2) : (((now / 110) & 1) ? SP_BILL1 : SP_BILL2)), true};
  for (int i = 0; i < D_MAXFX; i++) {
    DFx &f = dFx[i];
    if (!f.on) continue;
    unsigned long e = now - f.t0;
    if (e > f.life) { f.on = false; continue; }
    L[n++] = {f.x, f.y, 0, f.sc, f.h - f.sc * 0.5f, (uint16_t)(e < f.life / 2 ? f.spr : f.spr2), true};
  }
  for (int i = 0; i < n; i++) { float dx = L[i].x - dPX, dy = L[i].y - dPY; L[i].dist = dx * dx + dy * dy; }
  for (int i = 1; i < n; i++) { DSprite k = L[i]; int j = i - 1; while (j >= 0 && L[j].dist < k.dist) { L[j + 1] = L[j]; j--; } L[j + 1] = k; }
  uint16_t *fb = gfx->getFramebuffer();
  const uint16_t *cmP = dCM + dPalette(now) * D_NL * 256;
  int flash = (now - dFlashMs < 90) ? 4 : 0;
  float dirX = cosf(dPA), dirY = sinf(dPA), plX = -dirY * D_PLANE, plY = dirX * D_PLANE;
  float invDet = 1.0f / (plX * dirY - dirX * plY);
  for (int k = 0; k < n; k++) {
    const DSprite &s = L[k];
    float spX = s.x - dPX, spY = s.y - dPY;
    float trX = invDet * (dirY * spX - dirX * spY), trY = invDet * (-plY * spX + plX * spY);
    if (trY < 0.12f) continue;
    int scrX = (int)((DV_W / 2) * (1 + trX / trY));
    int sh = (int)(D_PROJ * s.scale / trY);
    if (sh < 2 || sh > 3000) continue;
    int bottom = (int)(D_HOR + (0.5f - s.zoff) * D_PROJ / trY);
    int y0 = bottom - sh, x0 = scrX - sh / 2;
    int cx = constrain((int)s.x, 0, D_MW - 1), cy = constrain((int)s.y, 0, D_MH - 1);
    int Ls = s.full ? D_NL - 1 : constrain(dLightC[cy][cx] - (int)(trY * 0.45f) + flash, 0, D_NL - 1);
    const uint16_t *cm = cmP + Ls * 256;
    const uint8_t *spr = dSpr + s.spr * SPR * SPR;
    int xa = max(0, x0), xb = min(DV_W, x0 + sh), ya = max(0, y0), yb = min(DV_H, y0 + sh);
    for (int sx = xa; sx < xb; sx++) {
      if (trY >= dZbuf[sx / DV_CW]) continue;
      const uint8_t *col = spr + ((sx - x0) * SPR / sh) * SPR;
      uint16_t *top = fb + sx * PANEL_W + (PANEL_W - 1);
      for (int y = ya; y < yb; y++) { uint8_t t = col[(y - y0) * SPR / sh]; if (t) top[-y] = cm[t]; }
    }
  }
}

// ---------------- arme à la 1re personne ----------------
static void dDrawWeapon(unsigned long now) {
  int w;
  unsigned long e = now - dShotMs;
  if (dWeapon == 0) w = e < 120 ? WP_PISTOL_F : WP_PISTOL;
  else w = e < 130 ? WP_SG_F : (e > 260 && e < 620) ? WP_SG_P : WP_SG;
  const int W = 160, H = 120;                                   // art 64x48 affiché x2,5
  int bx = (int)(cosf(dBobPh) * 12 * dBobAmp), by = (int)(fabsf(sinf(dBobPh)) * 10 * dBobAmp);
  int recoil = e < 90 ? 10 : 0;
  int x0 = DV_W / 2 - W / 2 + bx, y0 = DV_H - H + 16 + by + recoil;
  uint16_t *fb = gfx->getFramebuffer();
  int cx = constrain((int)dPX, 0, D_MW - 1), cy = constrain((int)dPY, 0, D_MH - 1);
  int L = (e < 120) ? D_NL - 1 : constrain(dLightC[cy][cx] + 1, 0, D_NL - 1);
  const uint16_t *cm = dCM + (dPalette(now) * D_NL + L) * 256;
  const uint8_t *wp = dWpn + w * WPW * WPH;
  for (int sx = 0; sx < W; sx++) {
    int x = x0 + sx;
    if (x < 0 || x >= DV_W) continue;
    const uint8_t *col = wp + (sx * WPW / W) * WPH;
    uint16_t *top = fb + x * PANEL_W + (PANEL_W - 1);
    for (int sy = 0; sy < H; sy++) {
      int y = y0 + sy;
      if (y < 0 || y >= DV_H) continue;
      uint8_t t = col[sy * WPH / H];
      if (t) top[-y] = cm[t];
    }
  }
}

// ---------------- HUD ----------------
static void dTintCircle(int cx, int cy, int r, uint16_t col, uint8_t amt) {
  for (int y = -r; y <= r; y++) {
    int yy = cy + y; if (yy < 0 || yy >= DV_H) continue;
    int w = (int)sqrtf((float)(r * r - y * y));
    for (int x = -w; x <= w; x++) { int xx = cx + x; if (xx < 0 || xx >= DV_W) continue; uint16_t *p = fxPix(xx, yy); *p = mix565(*p, col, amt); }
  }
}
static void dTextShadow(const char *s, int x, int y, uint8_t size, uint16_t col) {
  gfx->setTextSize(size);
  gfx->setTextColor(0x0000); gfx->setCursor(x + size, y + size); gfx->print(s);
  gfx->setTextColor(col); gfx->setCursor(x, y); gfx->print(s);
}
static void dTextCenterShadow(const char *s, int y, uint8_t size, uint16_t col) {
  dTextShadow(s, (SCR_W - (int)strlen(s) * 6 * size) / 2, y, size, col);
}
static void dDrawOverlay(unsigned long now) {
  // viseur
  gfx->drawFastHLine(233, D_HOR, 5, C_WHITE); gfx->drawFastHLine(243, D_HOR, 5, C_WHITE);
  gfx->drawFastVLine(240, D_HOR - 7, 5, C_WHITE); gfx->drawFastVLine(240, D_HOR + 3, 5, C_WHITE);
  // sticks : verre teinté
  if (dJL.on) { dTintCircle(dJL.bx, dJL.by, 40, C_WHITE, 40); dTintCircle(dJL.kx, dJL.ky, 16, C_ORANGE, 150); }
  else { gfx->drawCircle(80, 214, 40, mix565(C_BG, C_WHITE, 70)); gfx->drawCircle(80, 214, 16, mix565(C_BG, C_WHITE, 70)); }
  if (dJR.on) { dTintCircle(dJR.bx, dJR.by, 40, C_WHITE, 40); dTintCircle(dJR.kx, dJR.ky, 16, C_ORANGE, 150); }
  else gfx->drawCircle(300, 214, 40, mix565(C_BG, C_WHITE, 70));
  dTintCircle(D_FIRE_X, D_FIRE_Y, D_FIRE_R, dFireHeld ? C_YELLOW : C_RED, dFireHeld ? 170 : 110);
  gfx->drawCircle(D_FIRE_X, D_FIRE_Y, D_FIRE_R, C_RED);
  dTextShadow("FIRE", D_FIRE_X - 23, D_FIRE_Y - 7, 2, C_WHITE);
  // boutons MAP / ✕
  gfx->fillRoundRect(4, 3, 46, 28, 6, dMapOn ? C_ORANGE : 0x2104);
  gfx->setTextSize(2); gfx->setTextColor(dMapOn ? C_BG : C_WHITE); gfx->setCursor(10, 10); gfx->print("MAP");
  gfx->fillRoundRect(GAME_EX, GAME_EY + 1, GAME_EW - 2, GAME_EH - 2, 6, 0x2104);
  gfx->drawLine(GAME_EX + 11, GAME_EY + 8, GAME_EX + 25, GAME_EY + 22, C_WHITE);
  gfx->drawLine(GAME_EX + 25, GAME_EY + 8, GAME_EX + 11, GAME_EY + 22, C_WHITE);
  gfx->drawLine(GAME_EX + 12, GAME_EY + 8, GAME_EX + 26, GAME_EY + 22, C_WHITE);
  gfx->drawLine(GAME_EX + 26, GAME_EY + 8, GAME_EX + 12, GAME_EY + 22, C_WHITE);
  // message + popup nouveau bloc
  if (dMsg[0] && now - dMsgMs < 2800) dTextCenterShadow(dMsg, 40, 2, C_YELLOW);
  if (dmPopup[0] && now - dmPopupMs < 3000) {
    int w = (int)strlen(dmPopup) * 12 + 24;
    gfx->fillRoundRect(240 - w / 2, 62, w, 26, 6, C_ORANGE);
    gfx->setTextSize(2); gfx->setTextColor(C_BG); gfx->setCursor(240 - w / 2 + 12, 68); gfx->print(dmPopup);
  }
  if (dState == DS_DEAD) {
    dTextCenterShadow("VOUS ETES MORT", 100, 3, C_RED);
    if (now - dStateMs > 1500 && (now / 500) % 2) dTextCenterShadow("touchez pour recommencer", 140, 2, C_WHITE);
  }
}
static void dDrawAutomap() {
  gfx->fillRect(0, 0, DV_W, DV_H, 0x0000);
  const int S = 11, ox = (DV_W - D_MW * S) / 2, oy = (DV_H - D_MH * S) / 2 + 6;
  for (int y = 0; y < D_MH; y++)
    for (int x = 0; x < D_MW; x++) {
      uint8_t w = dWall[y][x];
      if (!w) { if (dZoneF[y][x] & Z_SLUDGE) gfx->fillRect(ox + x * S + 4, oy + y * S + 4, 3, 3, 0x2320); continue; }
      uint16_t c = w == W_EXIT ? C_GREEN : w == W_DOOR ? (dDoor[dDoorIdx[y][x]].locked ? C_ORANGE : C_YELLOW) : C_RED_D;
      int px = ox + x * S, py = oy + y * S;
      if (w == W_DOOR || w == W_EXIT) { gfx->fillRect(px + 2, py + 2, S - 4, S - 4, c); continue; }
      if (y > 0 && !dWall[y - 1][x]) gfx->drawFastHLine(px, py, S, c);
      if (y < D_MH - 1 && !dWall[y + 1][x]) gfx->drawFastHLine(px, py + S - 1, S, c);
      if (x > 0 && !dWall[y][x - 1]) gfx->drawFastVLine(px, py, S, c);
      if (x < D_MW - 1 && !dWall[y][x + 1]) gfx->drawFastVLine(px + S - 1, py, S, c);
    }
  for (int i = 0; i < dNT; i++) if (dTh[i].kind == K_KEY && dTh[i].st != ST_GONE) gfx->fillCircle(ox + (int)(dTh[i].x * S), oy + (int)(dTh[i].y * S), 3, C_ORANGE);
  int px = ox + (int)(dPX * S), py = oy + (int)(dPY * S);
  float ca = cosf(dPA), sa = sinf(dPA);
  gfx->fillTriangle(px + (int)(ca * 8), py + (int)(sa * 8), px + (int)(cosf(dPA + 2.5f) * 6), py + (int)(sinf(dPA + 2.5f) * 6),
                    px + (int)(cosf(dPA - 2.5f) * 6), py + (int)(sinf(dPA - 2.5f) * 6), C_WHITE);
  dTextCenterShadow(D_LVLNAME[dLvl], 6, 1, C_ORANGE);
}
// visage réactif (mascotte pièce ₿, création originale)
static void dDrawFace(int cx, int cy, unsigned long now) {
  gfx->fillRect(cx - 26, DSB_Y + 2, 52, 37, 0x0841);
  bool dead = dHealth <= 0, pain = now - dHurtMs < 450, grin = !pain && now - dKillMs < 900;
  gfx->fillCircle(cx, cy, 16, dead ? C_DGREY : C_ORANGE);
  gfx->drawCircle(cx, cy, 16, C_ORANGE_D); gfx->drawCircle(cx, cy, 13, mix565(C_ORANGE, C_YELLOW, 120));
  int look = pain ? 0 : (int)((now / 1400) % 3) - 1;
  if (dead) {
    gfx->drawLine(cx - 9, cy - 7, cx - 3, cy - 1, C_BG); gfx->drawLine(cx - 3, cy - 7, cx - 9, cy - 1, C_BG);
    gfx->drawLine(cx + 3, cy - 7, cx + 9, cy - 1, C_BG); gfx->drawLine(cx + 9, cy - 7, cx + 3, cy - 1, C_BG);
    gfx->fillCircle(cx, cy + 7, 3, C_BG);
    return;
  }
  if (pain) {
    gfx->drawLine(cx - 9, cy - 7, cx - 4, cy - 4, C_BG); gfx->drawLine(cx - 4, cy - 4, cx - 9, cy - 1, C_BG);
    gfx->drawLine(cx + 9, cy - 7, cx + 4, cy - 4, C_BG); gfx->drawLine(cx + 4, cy - 4, cx + 9, cy - 1, C_BG);
  } else {
    gfx->fillCircle(cx - 6, cy - 4, 4, C_WHITE); gfx->fillCircle(cx + 6, cy - 4, 4, C_WHITE);
    gfx->fillCircle(cx - 6 + look * 2, cy - 4, 2, C_BG); gfx->fillCircle(cx + 6 + look * 2, cy - 4, 2, C_BG);
  }
  if (grin) { gfx->fillRect(cx - 8, cy + 4, 17, 6, C_WHITE); gfx->drawRect(cx - 8, cy + 4, 17, 6, C_BG); for (int k = -4; k <= 4; k += 4) gfx->drawFastVLine(cx + k, cy + 4, 6, C_BG); }
  else if (dHealth > 60) { for (int k = -7; k <= 7; k++) { int yy = cy + 9 - (k * k) / 14; gfx->drawPixel(cx + k, yy, C_BG); gfx->drawPixel(cx + k, yy + 1, C_BG); } }   // sourire
  else if (dHealth > 30) gfx->fillRect(cx - 6, cy + 7, 13, 2, C_BG);
  else { for (int k = -7; k <= 7; k++) { int yy = cy + 6 + (k * k) / 14; gfx->drawPixel(cx + k, yy, C_BG); gfx->drawPixel(cx + k, yy + 1, C_BG); } }       // grimace
  if (dHealth < 50) { gfx->fillCircle(cx - 10, cy + 2, 2, C_RED); gfx->drawPixel(cx + 11, cy - 9, C_RED); }
  if (dHealth < 25) { gfx->fillRect(cx + 7, cy - 12, 3, 5, C_RED); gfx->fillCircle(cx - 4, cy + 12, 2, C_RED); }
}
static void dStatNum(int x, int w, const char *v, const char *lbl) {
  gfx->setTextSize(3);
  int tx = x + (w - (int)strlen(v) * 18) / 2;
  gfx->setTextColor(0x3000); gfx->setCursor(tx + 2, DSB_Y + 6); gfx->print(v);
  gfx->setTextColor(0xE8A4); gfx->setCursor(tx, DSB_Y + 4); gfx->print(v);
  gfx->setTextSize(1); gfx->setTextColor(C_GREY);
  gfx->setCursor(x + (w - (int)strlen(lbl) * 6) / 2, DSB_Y + 30); gfx->print(lbl);
}
static void dDrawStatusBar(unsigned long now) {
  for (int y = DSB_Y; y < SCR_H; y++) gfx->drawFastHLine(0, y, SCR_W, mix565(0x4228, 0x18E3, (uint8_t)((y - DSB_Y) * 6)));
  gfx->drawFastHLine(0, DSB_Y, SCR_W, C_ORANGE_D);
  const int sep[4] = {96, 206, 274, 384};
  for (int i = 0; i < 4; i++) { gfx->drawFastVLine(sep[i], DSB_Y + 2, 37, 0x10A2); gfx->drawFastVLine(sep[i] + 1, DSB_Y + 2, 37, 0x632C); }
  char b[12];
  snprintf(b, sizeof(b), "%d", dWeapon ? dShells : dBullets); dStatNum(0, 96, b, dWeapon ? "CARTOUCHES" : "SATS");
  snprintf(b, sizeof(b), "%d%%", dHealth); dStatNum(98, 108, b, "SANTE");
  dDrawFace(240, DSB_Y + 20, now);
  snprintf(b, sizeof(b), "%d%%", dArmor); dStatNum(276, 108, b, "ARMURE");
  // ARMES + clé + heure
  gfx->setTextSize(2);
  for (int k = 0; k < 2; k++) {
    bool own = k == 0 || dHasSG;
    int x = 394 + k * 22;
    if (k == dWeapon) gfx->drawRect(x - 3, DSB_Y + 4, 18, 20, C_YELLOW);
    gfx->setTextColor(own ? C_YELLOW : C_DGREY); gfx->setCursor(x + 1, DSB_Y + 7); gfx->print(k + 2);
  }
  if (dHasKey) { gfx->fillRect(446, DSB_Y + 6, 12, 16, C_ORANGE); gfx->fillRect(446, DSB_Y + 10, 12, 2, C_BG); }
  gfx->setTextSize(1); gfx->setTextColor(C_GREY); gfx->setCursor(392, DSB_Y + 30);
  if (gTmOk) gfx->printf("B%d  %02d:%02d", dLvl + 1, gTm.tm_hour, gTm.tm_min); else gfx->printf("BLOC %d", dLvl + 1);
}
// écran de bilan (fond de briques assombri, compteurs qui défilent)
static void dDrawTally(unsigned long now) {
  uint16_t *fb = gfx->getFramebuffer();
  const uint16_t *cm = dCM + 5 * 256;
  for (int x = 0; x < DV_W; x++) {
    uint16_t *top = fb + x * PANEL_W + (PANEL_W - 1);
    const uint8_t *tc = dTex + (dState == DS_WIN ? TX_BTC : TX_BRICK) * 4096 + ((x >> 1) & 63) * 64;
    for (int y = 0; y < DV_H; y++) top[-y] = cm[tc[(y >> 1) & 63]];
  }
  unsigned long e = now - dStateMs;
  if (dState == DS_WIN) {
    dTextCenterShadow("VICTOIRE !", 30, 4, C_ORANGE);
    dTextCenterShadow("LA PLANCHE A BILLETS EST DETRUITE", 84, 2, C_WHITE);
    dTextCenterShadow("21 000 000. Pas un de plus.", 112, 2, C_YELLOW);
  } else {
    dTextCenterShadow(D_LVLNAME[dLvl], 30, 2, C_ORANGE);
    dTextCenterShadow("TERMINE", 58, 4, C_WHITE);
  }
  float k = clamp01(e / 1600.0f);
  int kp = dKillsTot ? (int)(dKills * 100 * k / dKillsTot) : 100, ip = dItemsTot ? (int)(dItems * 100 * k / dItemsTot) : 100;
  char b[32];
  snprintf(b, sizeof(b), "ENNEMIS   %3d%%", kp); dTextShadow(b, 120, 150, 3, 0xE8A4);
  snprintf(b, sizeof(b), "OBJETS    %3d%%", ip); dTextShadow(b, 120, 184, 3, 0xE8A4);
  unsigned long ts = (unsigned long)(dLvlTime * k / 1000);
  snprintf(b, sizeof(b), "TEMPS    %2lu:%02lu", ts / 60, ts % 60); dTextShadow(b, 120, 218, 3, 0xE8A4);
  if (e > 2000 && (now / 500) % 2) dTextCenterShadow(dState == DS_WIN ? "touchez pour rejouer" : "touchez pour continuer", 254, 2, C_WHITE);
}

// ---------------- transition "melt" (colonnes qui fondent, façon Doom) ----------------
// fxPrevFb = ancien écran, fxNextFb = nouvel écran (préparés par l'appelant)
void fxMeltRun() {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || !fxPrevFb || !fxNextFb) return;
  const int NC = SCR_W / 3;
  static int16_t dly[SCR_W / 3];
  dly[0] = random(0, 120);
  for (int i = 1; i < NC; i++) dly[i] = constrain(dly[i - 1] + (int)random(-1, 2) * 20, 0, 240);
  unsigned long t0 = millis();
  for (;;) {
    long t = (long)(millis() - t0);
    bool done = true;
    for (int x = 0; x < SCR_W; x++) {
      long tt = t - dly[x / 3];
      int off = tt <= 0 ? 0 : (int)(PANEL_W * (tt / 620.0f) * (tt / 620.0f) + tt * 0.08f);
      if (off >= PANEL_W) off = PANEL_W; else done = false;
      uint16_t *dst = fb + x * PANEL_W;
      const uint16_t *pv = fxPrevFb + x * PANEL_W, *nx = fxNextFb + x * PANEL_W;
      if (off < PANEL_W) memcpy(dst, pv + off, (PANEL_W - off) * 2);
      if (off > 0) memcpy(dst + PANEL_W - off, nx + PANEL_W - off, off * 2);
    }
    gfx->flush();
    if (done) break;
  }
}

// ---------------- boucle du jeu (appelée par drawCurrentPage) ----------------
static void dRenderFrame(unsigned long now) {
  if (dState == DS_TALLY || dState == DS_WIN) dDrawTally(now);
  else {
    if (dMapOn) dDrawAutomap();
    else { dRender3D(now); dDrawSprites(now); if (dState == DS_PLAY) dDrawWeapon(now); }
    dDrawOverlay(now);
  }
  dDrawStatusBar(now);
}
// changement de niveau / restart avec melt
static void dMeltTo(int lv, bool newGame) {
  uint16_t *fb = gfx->getFramebuffer();
  bool can = fb && fxPrevFb && fxNextFb && animLevel >= 1;
  if (can) memcpy(fxPrevFb, fb, FB_PIX * 2);
  if (newGame) dNewGame(); else dLoadLevel(lv);
  if (!can) return;
  dRenderFrame(millis());
  memcpy(fxNextFb, fb, FB_PIX * 2);
  fxMeltRun();
}

void drawPageDoom() {
  unsigned long now = millis();
  if (!dmGenAssets()) { gfx->fillScreen(C_BG); textCenter("DOOM : PSRAM insuffisante", 150, 2, C_RED); return; }
  if (doomReset) { doomReset = false; dNewGame(); }
  static unsigned long last = 0;
  float dt = (last && now > last) ? min(0.1f, (now - last) / 1000.0f) : 0.033f;
  last = now;
  dReadInput();
  if (dTaps & DZ_MAP) dMapOn = !dMapOn;
  if ((dTaps & DZ_ARMS) && dHasSG) { dWeapon ^= 1; dSfx(13); }
  if (dState == DS_PLAY) dUpdatePlayer(dt, now);
  else if (dState == DS_DEAD) {
    if (now - dStateMs > 1500 && (dTaps & DZ_ANY)) { int lv = dLvl; dHealth = 100; dArmor = 0; dBullets = 50; dShells = 0; dHasSG = false; dWeapon = 0; dMeltTo(lv, false); now = millis(); }
  } else if (now - dStateMs > 2000 && (dTaps & DZ_ANY)) {
    if (dState == DS_WIN) dMeltTo(0, true); else dMeltTo(dLvl + 1, false);
    now = millis();
  }
  dUpdateDoors(dt, now);
  dUpdateThings(dt, now);
  dUpdateProj(dt, now);
  dRenderFrame(now);
}
