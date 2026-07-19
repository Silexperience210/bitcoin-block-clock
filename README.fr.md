# Boîtier BitcoinClock — Guition JC3248W535

Deux versions au choix :

- **v1 compacte** (`build_case.py`) : coque arrière « galet Alexa » de 20 mm,
  l'écran nu forme la face avant, fixation par les 4 vis d'origine.
- **v2 profonde ~15 cm** (`build_case_deep.py`) : version 2 pièces pour
  **haut-parleur + batterie 2000 mAh**, style « mini TV rétro / Echo Show »,
  avec grille haut-parleur au dos.

---

## v2 — Version profonde (2 pièces)

| Fichier | Rôle |
|---|---|
| `boitier_deep_avant.stl` | **Cadre avant** à imprimer (logement carte + puits de vis) |
| `boitier_deep_arriere.stl` | **Coque arrière** à imprimer (~158 mm de haut d'impression) |
| `boitier_deep_*_display.stl` | Mêmes pièces en orientation « posée » |
| `images/preview_deep.png` | Aperçu de l'ensemble |

- Encombrement posé : **116 × 82 × ~147 mm** (L × H × P), inclinaison 12°,
  fond plat sur toute la profondeur — très stable.
- Le **cadre avant** reçoit la carte exactement comme la v1 (logement +
  4 puits, vis d'origine) ; ses puits sont accessibles par l'arrière quand la
  coque est retirée.
- **Fentes de passage des fils** : en bas (HP, connecteurs JST) et en haut
  (connecteur batterie) — brancher HP + batterie sur la carte, faire passer
  les fiches dans la coque, puis refermer.
- **Grille haut-parleur** au dos (8 fentes) — coller le haut-parleur derrière
  (mousse double-face / colle chaude).
- Jonction cadre/coque : la coque coulisse sur le boss du cadre, puis
  **4 vis M3 × 8–10 auto-taraudeuses** radiales sur les côtés (pilotes Ø2,5 mm
  pré-percés).

### Impression v2

- **Aucun support nécessaire** : les deux pièces s'impriment comme des bols,
  ouverture vers le haut (orientations déjà correctes dans les STL).
- Coque arrière : ~158 mm de haut — **impression longue** (~20–30 h selon la
  machine), ~120 cm³ de matière solide. Cadre avant : ~3–4 h.
- PLA ou PETG, 3–4 périmètres, remplissage 15–25 %.
- Astuce : pour réduire la profondeur, modifier `TUBE_T_END` dans
  `build_case_deep.py` (p. ex. 80–100 mm suffisent largement pour HP +
  batterie) et régénérer.

### Notes v2

- L'interrupteur batterie et les boutons boot/reset (au dos de la carte)
  deviennent inaccessibles une fois fermé : laisser l'interrupteur sur ON, et
  ouvrir la coque (4 vis M3) pour y accéder.
- Chargement : USB-C sur le côté gauche (découpe dans le cadre avant).

---

## v1 — Version compacte

Coque arrière style « Alexa » (oblongue arrondie, légèrement bombée en
profondeur) pour la carte **Guition JC3248W535** (ESP32-S3 + écran 3,5"
480×320). L'écran nu forme la face avant ; la coque se fixe par les **4 vis
d'origine** de la carte. Inclinaison de **12°** vers l'arrière pour la lecture
sur un bureau.

## Fichiers

| Fichier | Rôle |
|---|---|
| `boitier_bitcoinclock.stl` | **À imprimer** — orienté face avant à plat sur le plateau |
| `boitier_bitcoinclock_display_orientation.stl` | Même pièce en orientation « posée » (visualisation) |
| `build_case.py` | Source paramétrique v1 (Python + trimesh/manifold3d) |
| `build_case_deep.py` | Source paramétrique v2 (2 pièces, ~15 cm) |
| `images/preview.png`, `images/preview_pose.png`, `images/preview_deep.png` | Aperçus 3D |
| `ref/` | Photos fabricant et boîtier de référence (mesures) |

## Dimensions clés

- Encombrement : **115,4 × 79,6 × 20 mm** ; carte 94,5 × 62 mm
- Volume matière (solide) : ~63 cm³ → **~35–45 g** imprimé (remplissage 25 %)
- Maillage vérifié : étanche (watertight), variété (manifold) — aucune
  réparation nécessaire à l'import dans le slicer
- Entraxe des 4 vis : **84,5 × 52,0 mm** (trous à 5 mm des bords, mesuré sur
  photos fabricant — vérifier au pied à coulisse avant impression)
- Puits de vis : fraisure **Ø6 mm** pour la tête, trou de passage **Ø3,2 mm**,
  fond de puits **1 mm** (vis d'origine réutilisées)
- Découpe **USB-C** sur le côté gauche (vue de face), centrée à 26,4 mm du
  bord supérieur de la carte

## Impression

- **Orientation** : telle que fournie — face avant (côté écran) à plat sur le
  plateau, dôme vers le haut.
- **Supports : OUI** — uniquement pour le plafond de la cavité interne
  (accessible et amovible par la grande ouverture avant) ; supports en
  arborescence (tree/organic) recommandés. Les traces restent cachées à
  l'intérieur.
- Matériau : PLA ou PETG ; 3–4 périmètres, remplissage 20–25 %.
- Hauteur de couche 0,2 mm (0,16 pour un dôme plus lisse).
- Pas de radeau nécessaire ; bordure optionnelle.

## Montage

1. **Attention** : les 4 vis maintiennent aussi le cadre de l'écran. Dévisser
   doucement, une par une, en retenant l'écran.
2. Poser la carte dans le logement avant (écran vers l'extérieur), USB-C à
   gauche.
3. Revisser les 4 vis d'origine à travers les puits arrière (la tête vient
   se loger dans la fraisure). Ne pas serrer fort : le fond de puits fait 1 mm.
   Si une vis semble trop courte, la remplacer par une **M2,5/M3** 2–3 mm plus
   longue que l'originale.
4. Brancher l'USB-C par la découpe latérale.

## Régénérer / modifier

```bash
python -m venv .venv
.venv/Scripts/pip install trimesh manifold3d numpy scipy shapely networkx rtree matplotlib pillow mapbox_earcut
.venv/Scripts/python build_case.py        # v1 compacte
.venv/Scripts/python build_case_deep.py   # v2 profonde (~15 cm)
```

Paramètres en tête des scripts : dimensions carte, entraxe des vis
(`HOLE_DX/HOLE_DY`), inclinaison (`TILT_DEG`), profondeur (`DOME_Z` v1 /
`TUBE_T_END` v2), découpe USB, grille HP (`GRILLE_*`), etc.

Sources des mesures : photos fabricant (via
[github.com/GthiN89/JC3248W535EN](https://github.com/GthiN89/JC3248W535EN)) —
carte 94,5 × 62 mm, zone active 73,4 × 49 mm.
