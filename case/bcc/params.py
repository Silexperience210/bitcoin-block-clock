# -*- coding: utf-8 -*-
"""Cotes du boitier BitcoinClock — source unique pour les deux versions.

Systeme de coordonnees "coque" (commun v1 / v2) :
  - z = 0 sur la face avant, la coque s'etend vers z negatif (vers l'arriere)
  - x = gauche/droite vue de face, y = haut/bas

Unites : millimetres.
"""
import numpy as np

# ------------------------------------------------------------------- carte
BOARD_W, BOARD_H = 94.5, 62.0       # contour de la carte (mesure fabricant)
POCKET_DEPTH = 4.2                  # PCB 1.6 + cadre LCD ~2.4 + marge
POCKET_CLEAR = 0.3                  # jeu par cote
HOLE_DX, HOLE_DY = 84.5, 52.0       # entraxe des 4 vis (trous a 5 mm des bords)

# --------------------------------------------------- bossages du capot dos
# La carte n'a PAS un dos plan : son capot arriere porte 4 bossages en relief
# aux angles, sur l'axe des vis. Sans logement pour les recevoir, la carte
# repose dessus et l'ecran ressort du boitier de BOSS_H.
BOSS_D = 6.0                        # diametre du bossage (a la base)
BOSS_H = 12.0                       # depassement au-dessus du plan du capot
BOSS_BORE_D = BOSS_D + 1.5          # logement : jeu radial de 0.75 par cote
BOSS_BORE_T = BOSS_H                # le fond du logement EST le plan d'appui.
#                                     Un seul datum : les bossages portent au
#                                     fond, et la poche est degagee de
#                                     POCKET_RELIEF pour ne pas surcontraindre.
#                                     Une erreur sur BOSS_H decale l'ecran
#                                     d'autant, mais a l'echelle du dixieme —
#                                     plus des 12 mm actuels.
POCKET_RELIEF = 0.2                 # degagement du plan de poche vs bossages

# ------------------------------------------------------------------- visserie
SCREW_CLEAR_D = 3.2                 # trou de passage vis M2.5/M3
HEAD_D = 6.0                        # fraisure pour tete de vis (~O4 mm)
WELL_FLOOR = 1.0                    # matiere restante sous la tete de vis
POST_D = 9.0                        # diametre des plots

# ------------------------------------------------------------------- coque
RIM = 10.0                          # largeur du cadre autour de la carte
WALL = 2.2                          # epaisseur parois (reference)
TILT_DEG = 12.0                     # inclinaison vers l'arriere

# ---------------------------------------------------------------- USB-C
# ATTENTION : le connecteur est perpendiculaire au PCB, ouverture tournee vers
# l'ARRIERE — le cable ne vient pas lateralement mais de derriere, a l'equerre.
# Il faut donc un passage traversant (cloison + capot), pas une echancrure de
# flanc. Vu de dos le connecteur est pres du bord droit, soit x negatif dans le
# repere coque (ou x est oriente vu de FACE).
USB_Y = 4.6                         # legerement au-dessus du milieu
USB_PASS_X = -42.25                 # ~5 mm du bord de la carte
USB_PASS_Y = 3.0                    # centre du passage (voir ci-dessous)
# Passage volontairement SURDIMENSIONNE : la position exacte du connecteur n'est
# connue qu'a la photo, et une lumiere trop juste condamnerait l'impression. Le
# dos n'est pas visible, donc rien n'est perdu a etre large. La hauteur est
# bornee par le creneau libre de la cloison (y de -14.6 a 20.5, soit 35.1 mm)
# entre la fente a fils et le plot de vis superieur : 32 mm laissent ~1.5 mm de
# nervure de chaque cote.
USB_PASS_W = 20.0
USB_PASS_H = 32.0

# ancienne decoupe laterale, conservee pour le v1 compact d'origine
USB_W, USB_H = 11.0, 5.2
USB_ZTOP = -0.3

# ------------------------------------------------------------- v1 "compact"
# BAND_Z / DOME_Z ont ete approfondis (-10/-20 a l'origine) pour loger les
# bossages : au droit des vis la coque descendait a z = -13.06 seulement, alors
# que le logement doit atteindre -16.2. Voir profondeur_au_droit_des_vis().
V1_BAND_Z = -18.0                   # fin de la bande verticale, debut du dome
V1_DOME_Z = -30.0                   # profondeur totale au centre
# Ces deux cotes sont contraintes par les bossages, pas par le style. Les vis
# sont a 42.25 mm du centre, quasiment au bord du stade : le dome s'y echappe
# tres vite. Ce qui compte n'est pas la profondeur au CENTRE de la vis mais au
# BORD de la pastille d'appui (rayon POST_D/2 plus loin) :
#   BAND_Z=-15 / DOME_Z=-30 -> bord a -15.87, le logement (-16.20) debouche
#   BAND_Z=-18 / DOME_Z=-30 -> bord a -18.70, il reste 2.50 mm de matiere
# C'est donc l'allongement de la BANDE DROITE qui debloque, pas l'approfondis-
# sement du dome : le dos reste plein diametre plus longtemps avant de s'arrondir.
V1_CAV_FLOOR_Z = -10.5              # fond plan de la cavite composants
V1_CAV_INSET = 2.5                  # retrait de la cavite vs logement carte
V1_BASE_RISE = 4.5                  # hauteur de la semelle plane

# geometrie d'origine, conservee pour le test de non-regression du portage
V1_LEGACY_BAND_Z = -10.0
V1_LEGACY_DOME_Z = -20.0

# indexe par le drapeau legacy : DOME_ARGS[False] = corrige, [True] = origine
DOME_ARGS = {
    False: (V1_BAND_Z, V1_DOME_Z),
    True: (V1_LEGACY_BAND_Z, V1_LEGACY_DOME_Z),
}

# ------------------------------------------- v1 XL : galet approfondi (80 max)
# Meme forme que le v1 compact, mais assez profond pour loger batterie et
# haut-parleur derriere une cloison interne. Contrainte utilisateur : 80 mm max.
# Construction PRISME OBLIQUE (et non galet) : a cette profondeur, un dome
# coupe par un plan voit son appui reculer jusqu'a ne plus porter le centre de
# masse — le boitier bascule vers l'avant. Le prisme garde un fond plat sur
# toute la longueur.
V1XL_T_END = 55.0       # profondeur le long de t (limite utilisateur : 80)
V1XL_RIM_T = 10.0       # longueur du conge avant->arriere
V1XL_BACK_S = 0.97      # dos quasi plan : il recoit un capot rapporte

# Corps ouvert a l'arriere + capot plat visse. Les 4 vis d'origine de la carte
# s'inserent PAR L'ARRIERE : dans un boitier monobloc ferme elles seraient
# inatteignables (seule ouverture : la fente a fils). Le dos ouvert les rend
# accessibles au tournevis, puis le capot referme.
CAP_T = 3.0             # epaisseur du capot
CAP_LIP_T = 4.0         # longueur de la levre de centrage
CAP_LIP_CLEAR = 0.35    # jeu de la levre dans le corps
CAP_SCREW_D = 2.6       # pilote M3 auto-taraudeuse dans le corps
CAP_SCREW_CLEAR = 3.3   # passage dans le capot
CAP_SCREW_HEAD = 6.2    # fraisure de tete dans le capot
CAP_BOSS_D = 8.5        # plot taraude dans le corps
CAP_BOSS_INSET = 10.0   # retrait des plots depuis le bord du contour

# grille de sortie du son, percee dans le capot (le HP est dans le compartiment
# arriere : sans elle il rayonnerait dans un volume clos et serait etouffe)
GRILLE_SLOTS = 8        # nombre de fentes
GRILLE_LEN = 46.0       # longueur d'une fente (selon x)
GRILLE_SLOT_W = 2.4     # largeur d'une fente (selon y)
GRILLE_STEP = 5.0       # pas entre fentes

DIVIDER_Z = -22.0       # face avant de la cloison (derriere les bossages a -16.2)
DIVIDER_T = 2.0         # epaisseur de la cloison
WIRE_SLOT_W = 50.0      # fente de passage des fils vers le compartiment arriere
WIRE_SLOT_H = 12.0
WIRE_SLOT_Y = -18.0     # en bas du boitier, hors de l'aplomb de la batterie

# encombrements a loger (LiPo type DTP634169 + petit HP 8 ohms)
BATT_L, BATT_W_, BATT_T = 69.0, 41.0, 7.0
SPK_D, SPK_T = 40.0, 10.0

# ---------------------------------------------------------------- v2 "deep"
OUT_W, OUT_H, OUT_R = 116.0, 82.0, 18.0   # profil exterieur (rect arrondi)
FRAME_T = 15.0          # longueur du cadre le long de t
SHOULDER_T = 8.0        # epaulement (debut du boss)
BOSS_INSET = 2.5        # retrait du boss d'emboitement
TUBE_T_END = 150.0      # profondeur totale le long de t (~15 cm)
TUBE_RIM_T = 10.0       # longueur du bord arrondi arriere
BACK_CAP_S = 0.85       # echelle du profil a la face arriere
V2_CAV_FLOOR_Z = -10.3  # fond de la cavite composants du cadre
V2_CAV_INSET = 2.5
JOIN_SCREW_D = 2.5      # pilotes M3 auto-taraudeuses (jonction cadre/tube)
WIRE_W, WIRE_H = 50.0, 12.0   # fente de passage des fils (bas)
GRILLE_N, GRILLE_PITCH = 8, 5.0

# ------------------------------------------------------------------- maillage
N = 160  # points par anneau

# --------------------------------------------------------------- masses (bilan)
MASS_SHELL_G = 30.0     # coque imprimee ~30 g (parois + remplissage 25 %)
MASS_BOARD_G = 90.0     # carte complete avec ecran

# ------------------------------------------------------------------- derives
HOLES = [(sx * HOLE_DX / 2.0, sy * HOLE_DY / 2.0)
         for sx in (1, -1) for sy in (1, -1)]

T_HAT = np.array([0.0,
                  np.sin(np.radians(TILT_DEG)),
                  -np.cos(np.radians(TILT_DEG))])


def v1_outer_size():
    """Encombrement exterieur du v1 (stade) — utilise par plusieurs modules."""
    return BOARD_W + 2 * (RIM + 0.45), BOARD_H + 2 * RIM


def profondeur_coque(px, py, band_z, dome_z):
    """Profondeur z de la surface exterieure du dome a l'aplomb de (px, py).

    Le dome retrecit le contour selon s = cos(u*pi/2)**0.75. La surface passe
    par (px, py) quand le stade a l'echelle s le contient tout juste ; on en
    deduit u puis z. Sert a verifier qu'un percage ne debouche pas, et a poser
    le fond de cavite au plus profond sans percer.
    """
    Wo, Ho = v1_outer_size()

    lo, hi = 0.01, 3.0
    for _ in range(200):                     # dichotomie sur l'echelle
        s = (lo + hi) / 2.0
        R, L = Ho * s / 2.0, max(Wo * s - Ho * s, 0.0)
        d = max(abs(px) - L / 2.0, 0.0)
        if d * d + py * py <= R * R:
            hi = s
        else:
            lo = s

    if hi >= 1.0:                            # point hors du dome : bande droite
        return band_z
    u = np.arccos(hi ** (1.0 / 0.75)) * 2.0 / np.pi
    return band_z + u * (dome_z - band_z)


def profondeur_au_droit_des_vis(band_z, dome_z):
    """Profondeur de coque a l'aplomb d'une vis (cas particulier usuel)."""
    return profondeur_coque(HOLE_DX / 2.0, HOLE_DY / 2.0, band_z, dome_z)


def base_pastille(band_z, dome_z, marge=0.7):
    """Cote a laquelle demarrer une pastille d'appui sans percer le dome.

    On l'evalue au point le plus excentre de la pastille, la ou la coque
    remonte le plus. `marge` garde la pastille strictement dans l'epaisseur de
    paroi : une pastille qui affleure la surface exterieure creerait des faces
    coincidentes et un maillage non-manifold.
    """
    hx, hy = HOLE_DX / 2.0, HOLE_DY / 2.0
    r = POST_D / 2.0
    n = np.hypot(hx, hy)
    bord = profondeur_coque(hx * (1 + r / n), hy * (1 + r / n), band_z, dome_z)
    return bord + marge


def fond_cavite(band_z, dome_z, wall=None):
    """Fond de cavite le plus profond laissant `wall` de matiere partout.

    La cavite est un rectangle arrondi inscrit dans la poche ; on la dimensionne
    sur son coin le plus defavorable, celui ou le dome remonte le plus.
    """
    wall = WALL if wall is None else wall
    cx = (BOARD_W + 2 * POCKET_CLEAR - 2 * V1_CAV_INSET) / 2.0
    cy = (BOARD_H + 2 * POCKET_CLEAR - 2 * V1_CAV_INSET) / 2.0
    return profondeur_coque(cx, cy, band_z, dome_z) + wall
