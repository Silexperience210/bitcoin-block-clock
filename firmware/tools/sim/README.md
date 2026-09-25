# 🖥️ Simulateur desktop — Bitcoin Block Clock

Compile **le vrai code de dessin du sketch** pour Linux/macOS, avec la **vraie**
lib `GFX Library for Arduino` (algorithmes + `Arduino_Canvas`). Le panel QSPI,
le tactile AXS15231B, le WiFi/HTTP, l'I2S, FreeRTOS et la NVS sont simulés
(`shim/`, `stubs.cpp`). Chaque `flush()` du canvas devient une image.

Usages :
- voir les 9 pages et les animations **sans flasher** ;
- produire les captures / vidéos du README ;
- **détecter les bugs mémoire** avec AddressSanitizer (c'est comme ça que le
  débordement du texte GFX dans BTC DOOM a été trouvé sur le code V4).

## Prérequis
- `arduino-cli` + core `esp32:esp32@2.0.14` + libs du README
  (`GFX Library for Arduino` 1.4.9, `ArduinoJson` 7, `ESP8266Audio` 1.9.7)
  — le prétraitement Arduino (génération des prototypes) passe par arduino-cli ;
- `g++` / `gcc` ; `ffmpeg` pour les vidéos.

## Utilisation
```bash
cd firmware/tools/sim
./build.sh --asan                 # binaire build/sim avec ASan + UBSan
./build/sim out/pages pages       # 9 captures (PPM 480x320)
./build/sim out/tour tour         # séquence : prix -> swipe -> on-chain -> nouveau bloc
./make_media.sh                   # PNG + demo.mp4 + new-block.gif dans out/
```
Modes : `pages`, `tour`, `pagesanim`, `cube`, `doom` (partie scriptée), `doomzoo`
(galerie des monstres), `boot`, `eco`, `off`,
`sigcheck` (parité du calcul embarqué des signaux : `build/sim out.txt sigcheck daily.txt`,
fichier `h l c` par ligne, à comparer avec `calibrate_squeeze.compute_states`).
Données d'exemple : `mockdata.inc` ; scénarios : `scenario.inc`.
Variables : `SKETCH=` (dossier du sketch), `ARDUINO_LIBS=` (dossier des libs).

## Limites
- Le temps est virtuel (+45 ms par flush) : les FPS réels se mesurent sur la carte.
- Le réseau n'est pas simulé (les fetchs échouent proprement) : les données
  viennent de `mockdata.inc`.
- UBSan signale des décalages de valeurs négatives *dans la lib GFX*
  (`Arduino_GFX.cpp`, helpers d'ellipse) : bénins sur ESP32, ignorés.
