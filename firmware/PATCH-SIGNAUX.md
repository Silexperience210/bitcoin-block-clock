# PATCH SIGNAUX v2 — momentum & direction calibrés (vue large)

## Fichiers ajoutés
- `firmware/bitcoin-block-clock/signals.h` — fetch Binance streaming + calcul TTM + panneaux UI
- `firmware/bitcoin-block-clock/signals_calib.h` — tables de fréquences (GÉNÉRÉ, ne pas éditer)
- `firmware/tools/calibrate_squeeze.py` — régénère signals_calib.h (relancer ~1×/trimestre)

Parité C↔Python vérifiée : 130/130 états squeeze identiques, BBW exact.

## Intégration dans bitcoin-block-clock.ino — 5 points

### 1. Include (après les autres includes, AVANT l'usage de dataMux)
`signals.h` référence `dataMux`, `gfx`, `drawSmooth`, `smoothWidth`, `drawArrow`,
les couleurs C_* : l'inclure APRÈS leurs déclarations (juste avant drawPageAI) :
```cpp
#include "signals.h"
```
Ajouter les prototypes en haut si nécessaire : `static void computeSignals();`

### 2. Nouveau bit de requête réseau
```cpp
#define REQ_SIGNALS 0x800          // à côté des REQ_ existants
// et REQ_ALL passe de 0x7FF à 0xFFF
```

### 3. Dans netTask : cadence 4 h
```cpp
static unsigned long tSig = 0;
if ((req & REQ_SIGNALS) || tSig == 0 || millis() - tSig > 4UL*3600*1000) {
  if (fetchSignals()) tSig = millis();
}
```
(2 requêtes ~55 KB total en streaming — aucun gros buffer, ~4 KB de floats.)

### 4. Page Signaux : remplacer le panneau "SQUEEZE 30J"
Dans `drawPageAI()`, remplacer tout le bloc `// ---------- panneau 2 : squeeze
Bollinger ----------` (les ~28 lignes avec `bwOf`) par :
```cpp
drawPanelSqueezeCal(318, 48);
```
Et remplacer le panneau "INDICATEUR TECHNIQUE" (ou l'ajouter dessous) par :
```cpp
drawPanelDirectionCal(10, 172);
```
Le score composite RSI/mom/pos30j actuel peut rester sur une autre ligne si
la place le permet — mais la ligne DIRECTION calibrée le remplace
avantageusement (fréquences mesurées vs heuristique).

### 5. FIX POISSON (précision ×5) — indépendant mais recommandé
Le λ actuel est moyenné sur 6 blocs (écart-type ~45 %). mempool.space fournit
le rythme réel de l'époque de difficulté (~2016 blocs) en un appel :
```cpp
// dans netTask, avec les autres fetchs mempool.space (toutes les ~10 min) :
static long lambdaEpoch = 600;      // secondes/bloc, ajusté époque
void fetchDiffAdj() {
  String r = httpGet("https://mempool.space/api/v1/difficulty-adjustment");
  if (r.length() < 10) return;
  JsonDocument d;
  if (deserializeJson(d, r)) return;
  double tAvg = d["timeAvg"] | 600000.0;     // ms/bloc mesuré sur l'époque
  portENTER_CRITICAL(&dataMux);
  lambdaEpoch = constrain((long)(tAvg / 1000.0), 300, 1200);
  portEXIT_CRITICAL(&dataMux);
}
```
Puis dans `drawPageAI()` panneau PROCHAIN BLOC :
```cpp
long lambda = lambdaEpoch;                       // remplace (ts[0]-ts[n-1])/(n-1)
// et afficher la MÉDIANE plutôt que la moyenne :
long medWait = (long)(lambda * 0.6931f);         // λ·ln2 ≈ 6,9 min si λ=10
```
Garder l'affichage "min retard" existant — il est déjà honnête vis-à-vis de la
propriété sans mémoire du processus de Poisson.

## Ce que la page affichera (données réelles au 2026-07-21)
- COMPRESSION : percentile **0**/120j, squeeze D ●  W ○, durée, "P(>10%/30j) 66% n=273"
- DIRECTION : mom TTM **+2,8 %**, contexte "momentum fort + régime",
  7j : 54 % hausse (n=760) · 30j : 55 % hausse (n=755)

## Honnêteté embarquée (à ne pas retirer)
- Chaque proba est affichée AVEC son n (taille d'échantillon).
- La jauge direction ne remplit que l'ÉCART au-delà de 50 % (le hasard).
- Baseline mesurée : dir 50 %, P(|move|>10 %/30j) 55 % — l'edge du squeeze est
  sur l'EXPANSION (66 % / 37 % >20 % / médiane 15,7 %), l'edge directionnel
  (52-55 %, 72 % pour le stack mais n=29) est petit et affiché comme tel.
- Aucun réseau de neurones. Fréquences historiques uniquement, recalculables
  par n'importe qui avec le script.

## Maintenance
```bash
cd firmware/tools && python3 calibrate_squeeze.py   # regénère signals_calib.h
```
Endpoint : data-api.binance.vision (miroir public officiel — PAS géo-bloqué,
contrairement à api.binance.com qui renvoie 451 dans certains pays).
