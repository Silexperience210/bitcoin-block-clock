# 📋 MÉMO PROJET — Bitcoin Block Clock (Guition JC3248W535)
*(état au 18/07/2026 — conversation Kimi CLI — firmware V4)*

## 🎯 Le projet
Horloge Bitcoin vitrine sur carte Guition JC3248W535 (ESP32-S3 N16R8 + écran 3.5" tactile) : prix, graphes, blocs, mempool, alertes sonores — haut-parleur I2S, batterie, config WiFi par portail web.

## 📂 Fichiers
- Firmware : `C:\Users\Silex\bitcoin-block-clock\bitcoin-block-clock.ino` (~1700 lignes, 56 Ko, compile 34% flash / 16% RAM)
- Logo bitmap RGB565 : `btc_logo.h` (généré depuis l'asset officiel CoinGecko 250px)
- Font chiffres lissés : `smooth_font.h` (alpha 4 bpp, Arial Bold 46px) + générateur `gen_smooth_font.py` (Pillow, relancer : `python gen_smooth_font.py`)
- Sources logo : `btc_logo_src.png` (CoinGecko), script de conversion inline (Pillow → RGB565, magenta 0xF81F = transparent)
- Doc constructeur carte : `C:\Users\Silex\Downloads\JC3248W535EN (1)\JC3248W535EN\` (schémas pinout PNG, specs PDF, démo LVGL)

## 🧩 HARDWARE JC3248W535 — fiche validée
| Composant | Détail |
|---|---|
| MCU | ESP32-S3 **N16R8** (16 Mo flash, 8 Mo PSRAM OPI), WiFi + BLE 5 |
| Écran | 3.5" IPS 320×480, **AXS15231B en QSPI** : CS=45, PCLK=47, D0=21, D1=48, D2=40, D3=39, RST=NC, TE=38 |
| Backlight | PWM **IO1** (ledc canal 0) |
| Tactile | Capacitif intégré AXS15231B, **I2C 0x3B** : SDA=4, SCL=8 |
| Ampli audio | NS4168 (clone MAX98357) : **BCLK=42, LRCLK=2, DIN=41** → connecteur JST 1.25 2P (haut-parleur externe) |
| Slot SD | SPI : CS=10, MOSI=11, CLK=12, MISO=13 |
| Batterie | Circuit charge LiPo + mesure tension **IO5** (diviseur 33K/100K → ×1.33) |
| GPIO libres | P2 : IO5,6,7,9,14,15,16,46 · P3/P4 : IO17, IO18 (+3.3V/GND) |
| Boutons | BOOT + RESET (petits trous à l'arrière) |
| ❌ Absents | **PAS de LED RGB, PAS de micro** (confirmé par l'utilisateur) |

## ⚙️ Détails techniques critiques (NE PAS RÉINTRODUIRE LES BUGS)
1. **Panel ignore les fenêtres d'adressage partielles** → OBLIGATOIRE : `Arduino_Canvas` (framebuffer PSRAM 320×480×2 = 307 Ko) + `flush()` full-frame. L'écriture directe = pixels épars. *(leçon du driver communautaire me-processware : "REQUIRED for QSPI!")*
2. **Inversion couleurs** : `ips=false` au constructeur (sinon écran blanc inversé)
3. **Rotation** : panel natif portrait 320×480 ; paysage 480×320 via **rotation canvas=1** (software, fiable — rotation hardware MADCTL capricieuse sur ce chip)
4. **Mapping tactile paysage** : `x_log = ry_raw ; y_log = 319 - rx_raw` (protocole : écrire `{0xB5,0xAB,0xA5,0x5A,0,0,0,0x08}` puis lire 8 bytes ; data[0]≠0 ou data[1]==0 → pas de touch ; x=(d2&0xF)<<8|d3, y=(d4&0xF)<<8|d5)
5. **Librairie écran** : `GFX Library for Arduino` **v1.4.9 EXACTE** (la 1.6.x exige core ≥2.0.17 via esp32-hal-periman.h ; core figé à 2.0.14 pour les autres projets ESP32-CAM)
6. **Audio sans lib** : dong synthétisé I2S legacy (driver/i2s.h du core 2.0.14) — sinusoïde + partiel 2.76×, enveloppe exp, 22050 Hz stéréo
7. **Après flash : reset physique obligatoire** (la carte reste en DOWNLOAD boot si un programme tient le port COM — ouvrir COM42 avec DTR actif = boot download)
8. **FQBN** : `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,USBMode=hwcdc,CDCOnBoot=cdc`
9. **Port flash** : COM42 (CH340, MAC 20:6e:f1:98:cd:88) — ports variables selon branchements
10. **API alternative.me (F&G) renvoie `"value"` en CHAÎNE** (`"25"`) : l'opérateur `doc[...] | -1` d'ArduinoJson v7 ne parse PAS les chaînes → renvoie -1 en permanence (jauge bloquée au milieu à 50). Toujours parser explicitement : `is<int>() ? as<int>() : atoi(as<const char*>())`. *(bug prouvé par test natif g++, corrigé le 18/07/2026)*

## ✅ Features implémentées (V4 — 7 pages + architecture FreeRTOS)
**Architecture V4 (refonte perf — slides/swipes autrefois bloqués plusieurs secondes par les fetchs synchrones) :**
- **`netTask` FreeRTOS** (core 0, prio 1, stack 12 Ko) : fait TOUS les appels HTTP + check TCP nœud, avec ses propres timers (height 20 s, price 30 s, whale 60 s, klines 300 s, mempool+fees 120 s, difficulty 600 s, F&G 3600 s, node 60 s, pools 600 s, lightning 600 s). Le `loop()` ne fait plus AUCUN appel réseau → tactile toujours réactif (`delay(10)`).
- **Requêtes à la demande** : bitmask `volatile uint32_t fetchReq` (REQ_PRICE/HEIGHT/KLINES/MEMPOOL/POOLS/LN/WHALE/FNG/DIFF/NODE, REQ_ALL) poussé par l'UI (tap refresh, switch devise/timeframe) via `requestFetch()`. Timeouts HTTP : 5 s (8 s klines & pools).
- **Événements net→UI** : flags `evNewBlock` / `evWhale` consommés par le loop (animations + sons).
- **`sndTask` FreeRTOS** (core 0, prio 2, stack 4 Ko) + queue de notes {freq, durMs, vol, kind} (capacité 8) : `beep`/`playStart`/`playAlarm`/`playWhale`/`playBellQ` poussent et retournent immédiatement (plus de blocage `i2s_write` dans le loop). Notes `SND_EVENT` filtrées si `nightMode`/`sleeping` ; bips UI (`SND_UI`) toujours joués.
- **drawChart optimisé** : polyline en `drawLine()` natif (2 passes) + fill dégradé en `drawFastVLine()` tous les 2 px (≈ ×10).
- **Redraw intelligent** : redraw forcé 5 s supprimé. Pages statiques = needRedraw ou tick minute ; On-chain = 1 Hz (chrono) ; Cube/Doom = ~30 FPS ; Pools = 30 FPS ~1,5 s après entrée/nouvelles données. **Redraws des pages statiques différés tant que le doigt est posé** (un swipe n'est jamais perdu pendant un flush ~60 ms) + `delay(5)` dans le loop.
- **Boot non bloquant** : plus de fetchs en série dans `setup()` ; `REQ_ALL` poussé dans netTask, UI immédiate avec états "chargement...".
- **Partage netTask→UI** : scalaires lus directement (atomiques) ; chaînes (lastPool, fngLabel, whaleTxid), tableau pools et closes/tsOf copiés sous `portENTER_CRITICAL` (`dataMux`).

**Pages (swipe ou barre d'onglets, 9 dots) :** 0 PRIX · 1 ON-CHAIN · 2 CUBE · 3 POOLS WAR · 4 LIGHTNING · 5 NŒUD & SENTIMENT · 6 IA LOCALE · 7 SIGNAUX · 8 BTC DOOM
- **IA LOCALE** (page 6) : moteur prédictif **100 % on-device**, aucune donnée ne sort. ① **Prochain bloc** : processus de Poisson (modèle mathématique exact du minage) — λ = intervalle moyen des 6 derniers blocs (`blkTs` capturés dans `fetchLastBlock`), estimation mm:ss + barre de progression + **probabilités P(bloc) à 1/5/10 min** (`1-exp(-t/λ)`). ② **Fees tendance** : régression linéaire sur ring buffer local de 32 échantillons feeFast (1 éch./2 min) → pente sat/vB/h + flèche + conseil d'envoi. ③ **Cycles de fees APPRIS en continu** : 168 créneaux (24 h × 7 jours) en EMA α=0,15, **persistés en NVS** (`feebkt`/`feesmp`, sauvegarde max 1×/30 min anti-usure flash) → "créneau ±X % vs norme", "creux probable dans ~Xh" (scan 48 h) + bar chart 24 h du jour en cours.
- **SIGNAUX** (page 7) : ① **Divergence tendance 1D vs 1S** (cache 7J, ~2 h/pt, σ-normalisée) → HAUSSIER/NEUTRE/BAISSIER + force faible/moyenne/forte (barres). ② **Squeeze Bollinger 30J** (bandwidth 20j vs moyenne des fenêtres, squeeze si <60 %). ③ Score technique RSI14+momentum+range (déplacé de la page IA). ④ **Anomalies** : z-score EMA (α=0,03) mempool + fees, **alerte vol 2σ** (dernier rendement 2h > 2σ du cache 7J) ; latch → `evAnomaly` → alarme sonore (filtrée nuit/veille). Réciprocité honnête : tout est étiqueté "indicateur", aucune promesse de prédiction.
- **⚠️ Leçon ML** : un MLP 6→8→3 (features RSI/momentum/vol/range) entraîné offline pour prédire la direction J+1 du prix a été testé (`train_trend_model.py`, venv `.venv`, Kraken daily) : **test 28-38 % < baseline majoritaire ~41 %, deux essais** (1 an et 2 ans). La direction J+1 n'est pas prédictible avec ces features → NON embarqué. Ne pas réintroduire de "prédiction de prix IA" sans validation sur données de test.
- **Cache klines** (devise × timeframe, ~9 Ko) : switch onglet/devise instantané (`chartCacheLoad`), fetch frais en arrière-plan ; `fetchKlinesView(fc, ft)` alimente n'importe quel slot sans perturber la vue affichée.
- **CUBE** (page 2) : cube isométrique ~140 px (arêtes pointillés orange, projection 3D maison) rempli de particules-transactions (220 max, grille 6×6×6, niveau = `min(1, mempoolCount/4000)`). Nouveau bloc → chaîne à maillons alternés qui descend (0,8 s) → emporte le cube vers la gauche avec tangage + particules qui s'échappent (1,2 s) → nouveau cube depuis la droite (0,5 s). HUD : mempool, fee rapide, bloc, pool. Si la page n'est pas visible → flash "NEW BLOCK" classique (conservé).
- **POOLS WAR** (page 3) : `mining/pools/1w` avec `DeserializationOption::Filter` (payload volumineux). Top 6 barres horizontales **animées** (interpolation vers la cible, effet course), palette fixe, #1 en orange + marqueur, nom + blocs + %.
- **LIGHTNING** (page 4) : `lightning/statistics/latest` → capacité réseau en BTC (gros chiffre + éclair stylé : polygone plein 7 points + ombre portée + contour + reflets), tuiles channels / nodes / capacité moyenne (sats) / fee rate moyen (ppm).
- **BTC DOOM** (page 7) : mini raycaster façon Wolfenstein 3D — map 16×16, 120 rayons DDA (colonnes 4 px), correction fisheye, shading pré-calculé `shade565` (4 niveaux × 2 côtés), collision par axe. **Gameplay** : 6 démons billboard (ellipses ombrées + yeux, occlusion par z-buffer des murs, tri peintre) qui **chassent le joueur** (0,45 cell/s) ; bouton **FIRE** (hitscan, muzzle flash, son), kill = +100 (agonie animée, respawn ~6 s), contact = **TOUCHE !** -50 + flash rouge ; score en chiffres lissés, minimap (joueur orange, démons rouges), viseur, arme dessinée. Contrôle au doigt par **drag** (horizontal = tourner, vertical = avancer/reculer), sortie par **✕**. NB : l'appui long = veille uniquement si le doigt reste **immobile** (>40 px de mouvement = drag de jeu, pas un appui).
- **Jauge F&G** : secteurs pleins en quads triangulaires (fini les stries), aiguille épaisse 3 px + moyeu, valeur/label centrés sur l'axe de la jauge (cx=110 — plus de chevauchement du panneau RESEAU).
- **Chiffres lissés anti-aliasés** (`smooth_font.h`, alpha 4 bpp généré par `gen_smooth_font.py`, blend565 par pixel) : prix (page 0), block height (page 1), valeur F&G (page 5), capacité LN (page 4). **Prix animé** : `dispPrice` interpole vers la cible à ~30 FPS pendant la transition (snap sur 1re valeur).
- **Portail AP** : champ **IP du nœud** ajouté au formulaire de config WiFi (placeholder %NODEIP%, sauvegardé en NVS comme sur la page web principale).
- **Conservé de V3** : prix EUR/USD/CHF + graphe 4 timeframes + curseur tactile, DONG nouveau bloc, alertes prix latch/hystérésis 2 %, whale watch, fees, difficulté, halving, F&G, portail WiFi AP + page web (alertes/IP nœud/reset), mDNS `blockclock.local`, mode nuit 23h–7h, appui long veille, watchdog WiFi 3 min, NTP CET, header/footer, logo bitmap.
- **Navigation = barre d'onglets** (footer, 8 icônes dessinées en primitives : courbe, cube, bac, podium, éclair, jauge, neurones, viseur) : tap = accès direct à n'importe quelle page, icône active orange + underline. Swipe conservé en bonus. **Debounce 250 ms** (`lastActionMs`/`touchIgnored`) : les appuis fantômes après une action sont ignorés (fix double-changement de page au tap).

## ✅ Features implémentées (V3 — historique)
- **3 pages swipe** : PRIX / ON-CHAIN / NŒUD & SENTIMENT
- Prix **EUR/USD/CHF** (tap sur devise pour switcher) + variation 24h (CoinGecko `simple/price`)
- **Graphe** onglets tactiles 1H/24H/7J/30J (CoinGecko `market_chart` + filtre ArduinoJson, sous-échantillonné ~90 pts) : dégradé, pointillés open, dot, **curseur tactile** (prix+heure du point)
- **DONG cloche** à chaque nouveau bloc (mempool.space `tip/height` poll 20s) + animation flash
- **Alertes prix** seuils haut/bas via page web (`/alerts`, NVS) → alarme 3 notes, latch avec hystérésis 2%
- ON-CHAIN : bloc + chrono + barre 10min, pool + nb TX du dernier bloc, fees 4 niveaux, difficulté (blocs restants + estimation), **Whale Watch** (TX >50 BTC mempool, dong grave, latch txid), countdown halving (bloc 1 050 000)
- NŒUD : jauge **Fear & Greed** (alternative.me), statut Umbrel (ping TCP :2105)
- Config WiFi **portail web** (AP `BlockClock-Setup` mdp `12345678`), NVS, mDNS `blockclock.local`, page web état+config
- Mode nuit 23h-7h (backlight 15%, silencieux), appui long = veille, watchdog WiFi (reboot 3 min)

## 🧠 Choix techniques retenus
- **CoinGecko only** (Binance = 451/bloqué France ; CoinMarketCap = clé payante + pas d'historique gratuit)
- Pas de LVGL (UI maison Arduino_GFX — plus léger, contrôle total)
- Aucun credential en dur (portail web, comme le projet surveillance)

## 🚀 PISTES D'AMÉLIORATION (roadmap)
- [ ] **Sync réelle du nœud Umbrel** : l'API Knots `:2105/api/rpc/sync` renvoie 302 → nécessite token JWT Umbrel. À creuser : token via login Umbrel ou endpoint public alternatif (electrs ?)
- [ ] Prix animé (transition douce entre deux valeurs)
- [ ] Thème couleur au choix (tap long sur logo)
- [ ] Historique local persistant (NVS/LittleFS) pour graphes sans réseau + stats maison
- [ ] Deep sleep nocturne + réveil matin (économie batterie)
- [ ] Samples WAV sur SD (vraie cloche) au lieu de la synthèse
- [ ] OTA updates
- [ ] Boîtier 3D dédié (comme les autres projets)

## 🐛 À vérifier sur le hardware réel
- Mapping tactile paysage en conditions réelles (ajuster si inversé)
- Volume du NS4168 (forum ESPHome signale un volume faible sur certaines cartes — ajouter gain logiciel si besoin)
- Charge batterie : circuit fonctionnel ? (tester autonomie)
- **V4** : fluidité réelle des pages Cube/Doom (~30 FPS visé, flush full-frame QSPI ~307 Ko par frame)
- **V4** : payload `mining/pools/1w` (volumineux) — vérifier que le filtre ArduinoJson suffit et que le heap ne sature pas
- **V4** : stack netTask 12 Ko sous charge TLS (CoinGecko + mempool.space) — surveiller d'éventuels stack canary dans le log série
- **V4** : séquence Cube sur nouveau bloc (chaîne → drag → spawn) et comportement appui long immobile vs drag sur BTC Doom

## 🔧 Commandes utiles
```bash
# Compiler
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,USBMode=hwcdc,CDCOnBoot=cdc" .
# Flasher (puis RESET PHYSIQUE !)
arduino-cli upload --fqbn "esp32:esp32:esp32s3" -p COM42 .
# Régénérer le logo (source PNG → btc_logo.h) : script Pillow dans l'historique de conversation
```
