// ============================================================
// signals_calib.h — GENERE par tools/calibrate_squeeze.py
// Donnees: BTCUSDT 1d, 2017-08-17 -> 2026-09-24 (3326 jours)
// Frequences historiques MESUREES, pas des promesses.
// Champs: {p_dir%, p_abs5%, p_abs10%, p_abs20%, med_abs_ret x1000, n,
//          n_eff (evenements independants), IC95 Wilson bas%, haut%}
// ============================================================
#pragma once
#include <stdint.h>

struct CalRow { uint8_t pDir, p5, p10, p20; uint16_t med; uint16_t n; uint16_t nEff; uint8_t ciLo, ciHi; };

// ---------- horizon 7j ----------
static const CalRow CAL7_BASE      = {50,46,21,5,44,3199,457,45,54};
static const CalRow CAL7_PCTL[4]   = {  // BBW pctl <5,<10,<20,>=20
  {48,41,23,6,34,300,65,37,60},
  {48,45,20,5,45,172,67,36,59},
  {48,41,17,4,40,305,101,38,57},
  {50,47,21,5,46,2422,378,45,55},
};
static const CalRow CAL7_MOM[3]    = {  // |mom| tercile 1..3
  {48,42,18,4,41,1065,215,41,54},
  {50,44,21,4,42,1065,250,44,56},
  {52,52,25,6,52,1069,195,45,59},
};
static const CalRow CAL7_CONFL     = {54,47,21,4,45,778,146,46,62}; // mom fort+regime
static const CalRow CAL7_STACK     = {72,38,24,10,34,29,6,34,93}; // squeeze D+W
static const CalRow CAL7_SQD       = {41,41,20,7,38,455,103,32,51};
static const CalRow CAL7_RELEASE   = {55,43,32,9,38,44,44,40,68};
static const float  CAL7_MOM_T1 = 3.0567f, CAL7_MOM_T2 = 7.5301f;

// ---------- horizon 30j ----------
static const CalRow CAL30_BASE      = {50,73,55,29,114,3176,106,41,60};
static const CalRow CAL30_PCTL[4]   = {  // BBW pctl <5,<10,<20,>=20
  {48,80,69,41,173,300,36,33,64},
  {58,71,60,27,129,171,39,43,72},
  {41,74,59,33,125,303,54,29,54},
  {51,72,52,27,105,2402,100,41,61},
};
static const CalRow CAL30_MOM[3]    = {  // |mom| tercile 1..3
  {50,74,57,31,119,1058,82,40,61},
  {47,71,52,27,106,1057,89,37,57},
  {54,73,56,28,115,1061,69,42,65},
};
static const CalRow CAL30_CONFL     = {55,69,52,26,107,770,57,42,67}; // mom fort+regime
static const CalRow CAL30_STACK     = {55,62,62,45,150,29,2,11,92}; // squeeze D+W
static const CalRow CAL30_SQD       = {47,79,65,32,135,455,47,34,61};
static const CalRow CAL30_RELEASE   = {50,73,61,34,146,44,32,34,66};
static const float  CAL30_MOM_T1 = 3.0490f, CAL30_MOM_T2 = 7.5101f;

// ---------- amplitude : volatilite EWMA + quantiles empiriques ----------
// fourchette(H) = prix * exp(q * sigma_jour * sqrt(H))
// H=2j : couverture hors-echantillon 80% -> 79%, 50% -> 49%
// H=7j : couverture hors-echantillon 80% -> 82%, 50% -> 50%
// H=30j : couverture hors-echantillon 80% -> 82%, 50% -> 55%
static const float VOL_LAMBDA = 0.94f;
static const uint8_t VOL_H[3] = {2, 7, 30};
static const float VOL_Q[3][5] = {   // q10, q25, q50, q75, q90
  {-1.1175f, -0.5021f, 0.0457f, 0.5787f, 1.1972f},
  {-1.1602f, -0.4872f, 0.0599f, 0.6498f, 1.3640f},
  {-1.3999f, -0.5226f, 0.0895f, 0.8213f, 1.8593f},
};
