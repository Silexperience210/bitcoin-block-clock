// ============================================================
// signals_calib.h — GENERE par tools/calibrate_squeeze.py
// Donnees: BTCUSDT 1d, 2017-08-17 -> 2026-07-21 (3261 jours)
// Frequences historiques MESUREES, pas des promesses.
// Champs: {p_dir%, p_abs5%, p_abs10%, p_abs20%, med_abs_ret x1000, n}
// ============================================================
#pragma once
#include <stdint.h>

struct CalRow { uint8_t pDir, p5, p10, p20; uint16_t med; uint16_t n; };

// ---------- horizon 7j ----------
static const CalRow CAL7_BASE      = {50,46,21,5,45,3134};
static const CalRow CAL7_PCTL[4]   = {  // BBW pctl <5,<10,<20,>=20
  {50,43,24,5,39,273},
  {48,45,19,4,45,168},
  {48,41,17,4,40,303},
  {51,47,22,5,46,2390},
};
static const CalRow CAL7_MOM[3]    = {  // |mom| tercile 1..3
  {48,42,17,4,41,1044},
  {50,44,21,4,43,1043},
  {52,53,25,6,53,1047},
};
static const CalRow CAL7_CONFL     = {54,48,21,4,47,760}; // mom fort+regime
static const CalRow CAL7_STACK     = {72,38,24,10,34,29}; // squeeze D+W
static const CalRow CAL7_SQD       = {42,42,20,6,40,430};
static const CalRow CAL7_RELEASE   = {52,43,31,10,38,42};
static const float  CAL7_MOM_T1 = 3.0875f, CAL7_MOM_T2 = 7.5959f;

// ---------- horizon 30j ----------
static const CalRow CAL30_BASE      = {50,73,55,28,114,3111};
static const CalRow CAL30_PCTL[4]   = {  // BBW pctl <5,<10,<20,>=20
  {49,78,66,37,157,273},
  {60,71,60,27,125,168},
  {40,74,59,33,126,301},
  {51,72,53,27,107,2369},
};
static const CalRow CAL30_MOM[3]    = {  // |mom| tercile 1..3
  {50,74,56,30,117,1036},
  {47,72,53,27,109,1036},
  {54,73,55,28,115,1039},
};
static const CalRow CAL30_CONFL     = {55,69,52,26,107,755}; // mom fort+regime
static const CalRow CAL30_STACK     = {55,62,62,45,150,29}; // squeeze D+W
static const CalRow CAL30_SQD       = {48,78,63,30,129,430};
static const CalRow CAL30_RELEASE   = {50,71,60,33,135,42};
static const float  CAL30_MOM_T1 = 3.1059f, CAL30_MOM_T2 = 7.6419f;

