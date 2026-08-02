# -*- coding: utf-8 -*-
"""Boitier BitcoinClock XL — corps + capot arriere, batterie et haut-parleur.

    blender --background --python case/build_v1xl_blender.py

Forme : PRISME OBLIQUE. Le contour (rectangle arrondi) glisse le long de
t = (0, sin12, -cos12), d'ou un dessous plat sur toute la profondeur et une face
avant penchee a 12 deg. Le galet du v1 ne convient pas a cette profondeur : son
appui recule au point que le boitier bascule vers l'avant.

Deux pieces, parce que les 4 vis d'origine de la carte s'inserent par l'arriere
et seraient inatteignables dans un monobloc ferme :
  - CORPS  : ouvert a l'arriere, poche carte a l'avant, cloison + fente a fils
  - CAPOT  : plaque plate a levre de centrage, 4 vis M3

Assemblage :
  1. carte dans la poche avant
  2. 4 vis d'origine, vissees depuis le dos ouvert
  3. haut-parleur et batterie dans le compartiment arriere, fils par la fente
  4. capot visse

Impression : corps sur son FOND PLAT (cloison verticale, aucun support) ;
capot a plat.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Matrix  # noqa: E402

from bcc import build, validate  # noqa: E402
from bcc.params import (BATT_L, BATT_T, BATT_W_, BOARD_H, BOARD_W,  # noqa: E402
                        BOSS_BORE_T, BOSS_H,
                        CAP_BOSS_D, CAP_BOSS_INSET, CAP_LIP_CLEAR, CAP_LIP_T,
                        CAP_SCREW_CLEAR, CAP_SCREW_D, CAP_SCREW_HEAD, CAP_T,
                        DIVIDER_T, GRILLE_LEN, GRILLE_SLOTS, GRILLE_SLOT_W,
                        GRILLE_STEP, HEAD_D, HOLES, OUT_H, OUT_R, OUT_W,
                        POCKET_CLEAR, POCKET_DEPTH, POCKET_RELIEF, POST_D,
                        SCREW_CLEAR_D, SPK_D, SPK_T, TILT_DEG, USB_PASS_H,
                        USB_PASS_W, USB_PASS_X, USB_PASS_Y, USB_H, USB_W,
                        USB_Y, V1XL_BACK_S,
                        V1XL_RIM_T, V1XL_T_END,
                        WALL, WELL_FLOOR, WIRE_SLOT_H, WIRE_SLOT_W)
from bcc.profiles import circle_pts, resample_closed, rrect_pts  # noqa: E402
from bcc.rings import prism_rings  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))

TILT = np.radians(TILT_DEG)
T_HAT = np.array([0.0, np.sin(TILT), -np.cos(TILT)])

DIV_T = 20.0                                # cloison le long de t
BORE_FLOOR = -POCKET_DEPTH - BOSS_BORE_T    # appui des bossages : z = -16.2

# plots de fixation du capot, aux 4 angles du contour
CAP_BOSSES = [(sx * (OUT_W / 2 - CAP_BOSS_INSET),
               sy * (OUT_H / 2 - CAP_BOSS_INSET))
              for sx in (1, -1) for sy in (1, -1)]

# ------------------------------------------------------- orientation ecran
# L'ecran du JC3248W535 sort a l'envers dans le sens de montage d'origine. On le
# remet a l'endroit en tournant la carte de 180 deg dans sa poche, ce que la
# mecanique autorise sans rien changer d'autre : les 4 vis sont a
# (+-HOLE_DX/2, +-HOLE_DY/2), donc invariantes par cette rotation, et la poche
# est centree. Seul le passage USB-C suit la carte.
#
# La rotation envoie le connecteur de (USB_PASS_X, USB_Y) a (-USB_PASS_X,
# -USB_Y), mais le passage, lui, ne bouge QU'EN X. Deux raisons :
#   - il est tres surdimensionne (20 x 31 pour un connecteur de 11 x 5.2) et
#     couvre deja largement y = -USB_Y ; verifier_passage_usb() le prouve au
#     lieu de le supposer.
#   - le descendre a y = -4 le ferait mordre la fraisure de la vis basse du
#     capot (sommet a -12.5) : c'est la contrainte qui avait deja fixe
#     USB_PASS_H a 31. Le premier essai en (x,y) -> (-x,-y) s'est fait
#     rejeter par verifier_interferences() sur exactement cette paire.
# Pilote par --ecran180 ; USB_PX / USB_PY sont fixes par _orienter().
USB_PX, USB_PY = USB_PASS_X, USB_PASS_Y
ECRAN180 = False


def _orienter(retourne):
    """Place le passage USB-C selon le sens de montage de la carte."""
    global USB_PX, USB_PY, ECRAN180
    ECRAN180 = bool(retourne)
    USB_PX = -USB_PASS_X if ECRAN180 else USB_PASS_X
    USB_PY = USB_PASS_Y
    print(f"--- orientation carte : "
          f"{'180 deg (ecran retourne)' if ECRAN180 else 'origine'}"
          f" -> passage USB-C a ({USB_PX:+.2f}, {USB_PY:+.2f})")


def verifier_passage_usb():
    """Controle que le passage couvre bien le connecteur, carte tournee ou non.

    Sans ce controle, mirroir-en-x-seulement ne serait qu'une intuition : rien
    ne garantirait que le connecteur, qui descend a -USB_Y quand la carte
    tourne, reste dans une lumiere centree sur +USB_PASS_Y.
    """
    cx = -USB_PASS_X if ECRAN180 else USB_PASS_X
    cy = -USB_Y if ECRAN180 else USB_Y
    marges = {
        "gauche": (cx - USB_W / 2) - (USB_PX - USB_PASS_W / 2),
        "droite": (USB_PX + USB_PASS_W / 2) - (cx + USB_W / 2),
        "bas": (cy - USB_H / 2) - (USB_PY - USB_PASS_H / 2),
        "haut": (USB_PY + USB_PASS_H / 2) - (cy + USB_H / 2),
    }
    pire = min(marges.values())
    etat = "OK" if pire >= 1.0 else ("JUSTE" if pire > 0 else "HORS LUMIERE")
    detail = "  ".join(f"{k} {v:+.1f}" for k, v in marges.items())
    print(f"--- connecteur ({cx:+.2f}, {cy:+.2f}) dans le passage : "
          f"marge {pire:+.1f} mm  {etat}")
    print(f"      {detail}")
    return pire


def verifier_levre_plots():
    """Contrôle que la lèvre du capot passe entre les plots de vis du corps.

    Ce contrôle existe parce que l'oubli a coûté un tirage : les plots étaient
    placés à 10 mm du bord, la lèvre les heurtait de 2.11 mm, et le capot ne
    rentrait tout simplement pas. Rien dans le maillage ne le signalait — les
    deux pièces étaient parfaitement étanches chacune de son côté.
    """
    lev_int = _contour(WALL + CAP_LIP_CLEAR + 2.0)
    r = CAP_BOSS_D / 2.0
    marges = [float(np.min(np.hypot(lev_int[:, 0] - bx, lev_int[:, 1] - by))) - r
              for bx, by in CAP_BOSSES]
    pire = min(marges)
    etat = "OK" if pire >= 1.0 else ("JUSTE" if pire > 0 else "COLLISION")
    print(f"--- levre du capot vs plots de vis : marge {pire:+.2f} mm  {etat}")
    return pire


def verifier_interferences():
    """Contrôle que les percages d'une même pièce ne se recoupent pas.

    Les deux défauts du premier tirage venaient de là, et aucun contrôle de
    maillage ne pouvait les voir : chaque pièce était parfaitement étanche,
    mais leurs percages se chevauchaient. On raisonne sur les rectangles
    englobants dans le plan de chaque pièce, en tenant compte du cisaillement
    (le contour se décale en y quand on avance le long de t).
    """
    dec_capot = V1XL_T_END * np.sin(TILT)
    dec_cloison = DIV_T * np.sin(TILT)

    def rect(cx, cy, w, h):
        return (cx - w / 2, cx + w / 2, cy - h / 2, cy + h / 2)

    def chevauche(a, b):
        return not (a[1] <= b[0] or b[1] <= a[0] or a[3] <= b[2] or b[3] <= a[2])

    usb = rect(USB_PX, USB_PY, USB_PASS_W, USB_PASS_H)
    grille = rect(0, dec_capot, GRILLE_LEN,
                  (GRILLE_SLOTS - 1) * GRILLE_STEP + GRILLE_SLOT_W)
    fente = rect(0, dec_cloison - OUT_H / 2 + WIRE_SLOT_H + WALL,
                 WIRE_SLOT_W, WIRE_SLOT_H)

    paires = []
    for bx, by in CAP_BOSSES:                       # dans le capot
        fr = rect(bx, by + dec_capot, CAP_SCREW_HEAD, CAP_SCREW_HEAD)
        paires.append((f"vis capot ({bx:+.0f},{by + dec_capot:+.1f}) / USB", usb, fr))
        paires.append((f"vis capot ({bx:+.0f},{by + dec_capot:+.1f}) / grille",
                       grille, fr))
    paires.append(("grille / USB", grille, usb))
    paires.append(("fente à fils / USB", fente, usb))
    for hx, hy in HOLES:                            # dans la cloison
        pl = rect(hx, hy, POST_D, POST_D)
        paires.append((f"plot carte ({hx:+.0f},{hy:+.0f}) / USB", usb, pl))
        paires.append((f"plot carte ({hx:+.0f},{hy:+.0f}) / fente", fente, pl))

    fautes = [nom for nom, a, b in paires if chevauche(a, b)]
    if fautes:
        print("--- interférences : " + str(len(fautes)) + " COLLISION(S)")
        for f in fautes:
            print(f"      {f}")
    else:
        print(f"--- interférences : aucune ({len(paires)} paires vérifiées)")
    return fautes


def verifier_ecran():
    """Position de la face avant de l'écran une fois la carte posée."""
    dos = -POCKET_DEPTH - BOSS_BORE_T + BOSS_H     # plan du capot de la carte
    ecran = dos + POCKET_DEPTH
    etat = "affleurant" if abs(ecran) < 0.3 else (
        f"{-ecran:+.1f} mm (négatif = enfoncé)")
    print(f"--- écran : plots à {-POCKET_DEPTH - BOSS_BORE_T:.1f}, "
          f"face avant à {ecran:+.1f} -> {etat}")
    return ecran


def _at(t):
    return t * T_HAT


def _contour(inset, scale=1.0):
    return rrect_pts((OUT_W - 2 * inset) * scale, (OUT_H - 2 * inset) * scale,
                     max(OUT_R - inset, 0.5) * scale)


def _plate(contour, t_front, thickness, name):
    """Plaque perpendiculaire a z, posee a l'abscisse t_front du prisme."""
    org = _at(t_front)
    pts = contour + np.array([0.0, org[1]])
    return build._extrude_profile(pts, org[2] - thickness, org[2], name)


def _prism_seg(contour, ta, tb, name, n=4):
    """Troncon de prisme OBLIQUE entre les abscisses ta et tb.

    Indispensable pour la levre du capot : une extrusion droite ne suivrait pas
    le cisaillement du corps (0.83 mm sur 4 mm de levre) et coincerait, alors
    que le jeu prevu n'est que de CAP_LIP_CLEAR.
    """
    rings = []
    for t in np.linspace(ta, tb, n):
        o = _at(t)
        rings.append(np.column_stack([
            contour[:, 0],
            contour[:, 1] + o[1],
            np.full(len(contour), o[2])]))
    rings.sort(key=lambda r: -r[0, 2])          # z decroissant, comme loft l'attend
    return build.loft(rings, name)


def _cannelures(corps, n=46, r=1.7, prof=0.7, t_a=5.0, garde_fond=8.0):
    """Cannelures longitudinales creusees dans le flanc du corps.

    Chaque cannelure est un tube circulaire suivant l'axe t du prisme : comme
    les anneaux du corps, elle est cisaillee en y, donc elle epouse exactement
    la surface au lieu de la traverser en biais.

    Le FOND PLAT est laisse lisse (`garde_fond`) : c'est a la fois l'appui du
    boitier et sa face d'impression. Les cannelures demarrent a t_a pour ne pas
    entailler le rebord avant.
    """
    C = _contour(0.0)
    ncont = len(C)
    outils = []
    for k in range(n):
        i = int(round(k * ncont / n)) % ncont
        p = C[i]
        if p[1] < -OUT_H / 2 + garde_fond:      # on epargne la semelle
            continue
        tang = C[(i + 1) % ncont] - C[(i - 1) % ncont]
        nrm = np.array([-tang[1], tang[0]])     # +90 deg : vers l'interieur (CCW)
        nrm /= np.linalg.norm(nrm)
        # Centre du tube place a l'EXTERIEUR de la surface : la penetration vaut
        # alors exactement `prof`. Centre a l'interieur, elle vaudrait 2r - prof
        # (2.7 mm ici) et la cannelure traverserait la paroi de 2.2 mm.
        c = p - nrm * (r - prof)
        outils.append(_prism_seg(circle_pts(r, 20, c[0], c[1]),
                                 t_a, V1XL_T_END + 1.0, f"cann{k}", n=3))
    print(f"    {len(outils)} cannelures (r={r}, profondeur={prof} mm)")
    return build.difference(corps, outils)


def _blocs(corps, bloc_s=8.0, bloc_t=10.0, jeu_s=2.8, jeu_t=3.2, prof=0.65,
           t_a=6.0, garde_fond=9.0, chanfrein=4.0):
    """Appareil de blocs en quinconce, en creux, faisant le tour du boitier.

    Motif thematique : chaque pastille est un bloc, les rangees decalees d'un
    demi-pas figurent le chainage. Il enveloppe les flancs et le dessus, le fond
    plat restant lisse.

    Comme pour les cannelures, le cutter est centre a l'EXTERIEUR de la surface
    pour que la penetration vaille exactement `prof` et n'entame pas la paroi.
    Les 112 pastilles sont fusionnees en un seul maillage (elles sont
    disjointes) : une seule soustraction au lieu de 112.
    """
    C = _contour(0.0)
    per = float(np.sum(np.linalg.norm(
        np.diff(np.vstack([C, C[:1]]), axis=0), axis=1)))
    pas_s = bloc_s + jeu_s
    ncol = max(int(round(per / pas_s)), 8)
    Cs = resample_closed(C, ncol)

    pas_t = bloc_t + jeu_t
    t_max = V1XL_T_END - 2.0

    outils = []
    for i in range(ncol):
        p = Cs[i]
        if p[1] < -OUT_H / 2 + garde_fond:          # semelle epargnee
            continue
        tang = Cs[(i + 1) % ncol] - Cs[(i - 1) % ncol]
        tang /= np.linalg.norm(tang)
        nrm = np.array([-tang[1], tang[0]])         # vers l'interieur (CCW)

        base = rrect_pts(bloc_s, chanfrein, min(bloc_s, chanfrein) * 0.35)
        ca, sa = tang[0], tang[1]
        base = base @ np.array([[ca, sa], [-sa, ca]])   # aligne sur la tangente
        centre = p + nrm * (prof - chanfrein / 2.0)
        pts = base + centre

        j = 0
        while True:                                  # rangees decalees
            t1 = t_a + j * pas_t + (i % 2) * pas_t / 2.0
            t2 = t1 + bloc_t
            if t2 > t_max:
                break
            outils.append(_prism_seg(pts, t1, t2, f"bl{i}_{j}", n=3))
            j += 1

    print(f"    {len(outils)} blocs ({ncol} colonnes, profondeur {prof} mm)")
    return build.difference(corps, build.join(outils))


GENESIS = ("000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f")


def _genesis_bits():
    """Les 256 bits du hash du bloc genesis, de poids fort a poids faible."""
    return [(int(c, 16) >> b) & 1 for c in GENESIS for b in (3, 2, 1, 0)]


def _arc_utile(garde_fond, n_ech=1200):
    """Portion du contour hors semelle, re-echantillonnee a pas constant.

    Le fond plat est exclu : c'est l'appui et la face d'impression. La zone
    restante est un arc contigu (flancs + dessus) que l'on parcourt de bout en
    bout pour y ranger les colonnes du motif.
    """
    C = resample_closed(_contour(0.0), n_ech)
    ok = C[:, 1] >= -OUT_H / 2 + garde_fond
    # remettre l'arc a plat : on demarre juste apres la semelle
    depart = None
    for i in range(n_ech):
        if ok[i] and not ok[(i - 1) % n_ech]:
            depart = i
            break
    if depart is None:
        return C
    ordre = [(depart + k) % n_ech for k in range(n_ech)]
    return np.array([C[i] for i in ordre if ok[i]])


def _hash_relief(corps, ncol=32, nrow=8, bloc_s=6.4, bloc_t=4.4,
                 relief=1.2, ancrage=1.0, t_a=7.0, garde_fond=11.0):
    """Hash du bloc genesis grave en relief sur la coque.

    256 bits ranges en `ncol` x `nrow` : un bit a 1 devient une pastille
    saillante, un bit a 0 laisse la surface nue. Les 32 zeros de tete du hash
    forment donc une bande lisse continue — la signature visuelle de Bitcoin.

    En RELIEF (union) et non en creux : la paroi de 2.2 mm n'est pas entamee,
    les pastilles ne font qu'ajouter de la matiere par-dessus. Elles sont
    ancrees de `ancrage` mm sous la surface pour fusionner franchement.

    Les pastilles sont disjointes : on les concatene en un seul maillage et on
    ne fait qu'une union, au lieu de 128.
    """
    arc = _arc_utile(garde_fond)
    if len(arc) < ncol * 2:
        raise ValueError("arc utile trop court pour ce nombre de colonnes")

    bits = _genesis_bits()
    pas_t = bloc_t + 2.0
    t_max = V1XL_T_END - 3.0
    if t_a + nrow * pas_t > t_max:
        pas_t = (t_max - t_a) / nrow

    pastilles = []
    for col in range(ncol):
        k = int(round((col + 0.5) * (len(arc) - 1) / ncol))
        p = arc[k]
        tang = arc[min(k + 1, len(arc) - 1)] - arc[max(k - 1, 0)]
        tang /= np.linalg.norm(tang)
        nrm = np.array([-tang[1], tang[0]])          # vers l'interieur

        ext = ancrage + relief                       # epaisseur selon la normale
        base = rrect_pts(bloc_s, ext, min(bloc_s, ext) * 0.3)
        base = base @ np.array([[tang[0], tang[1]], [-tang[1], tang[0]]])
        centre = p + nrm * ((ancrage - relief) / 2.0)
        pts = base + centre

        for row in range(nrow):
            if not bits[row * ncol + col]:           # bit a 0 : surface nue
                continue
            t1 = t_a + row * pas_t
            pastilles.append(_prism_seg(pts, t1, t1 + bloc_t,
                                        f"h{col}_{row}", n=3))

    print(f"    hash genesis : {len(pastilles)} pastilles sur {ncol * nrow} bits"
          f" ({ncol} colonnes x {nrow} rangees, relief {relief} mm)")
    return build.union(corps, build.join(pastilles))


# ------------------------------------------------------------------- corps
def build_corps(motif=False):
    build.purge_scene()

    print(f"corps : prisme t_end={V1XL_T_END} mm, dos ouvert")
    corps = build.loft(prism_rings(OUT_W, OUT_H, OUT_R, V1XL_T_END,
                                   V1XL_RIM_T, V1XL_BACK_S), "CorpsXL")

    # interieur debouchant a l'arriere (t1 > t_end) -> le dos s'ouvre
    inner = build.loft(prism_rings(OUT_W, OUT_H, OUT_R, V1XL_T_END,
                                   V1XL_RIM_T, 1.0, inset=WALL,
                                   t0=WALL, t1=V1XL_T_END + 6.0), "inner")
    corps = build.difference(corps, inner)
    validate.report(corps, "corps evide, dos ouvert")

    # --- poche carte
    pocket = build.box_tool(0, 0, -POCKET_DEPTH - POCKET_RELIEF, 3.0,
                            BOARD_W + 2 * POCKET_CLEAR,
                            BOARD_H + 2 * POCKET_CLEAR, 4.5, "pocket")
    corps = build.difference(corps, pocket)

    # --- cloison + fente de passage des fils
    org = _at(DIV_T)
    cloison = _plate(_contour(WALL), DIV_T, DIVIDER_T, "cloison")
    fente = build.box(0, org[1] - OUT_H / 2 + WIRE_SLOT_H + WALL,
                      org[2] - DIVIDER_T / 2,
                      WIRE_SLOT_W, WIRE_SLOT_H, DIVIDER_T + 4, "fente")
    # passage du cable USB-C : le connecteur est a l'equerre, ouverture vers
    # l'arriere, donc le cable traverse la cloison puis le capot en ligne droite
    usb_pass = build.box(USB_PX, USB_PY, org[2] - DIVIDER_T / 2,
                         USB_PASS_W, USB_PASS_H, DIVIDER_T + 4, "usbpass")
    cloison = build.difference(cloison, [fente, usb_pass])
    corps = build.union(corps, cloison)

    # --- plots portant les bossages de la carte
    plots = [build.cylz(POST_D / 2, org[2] - DIVIDER_T, BORE_FLOOR, x, y,
                        name=f"plot{i}")
             for i, (x, y) in enumerate(HOLES)]
    corps = build.union(corps, plots)

    # --- puits de vis de la carte, ouverts vers le compartiment arriere
    head_top = BORE_FLOOR - WELL_FLOOR
    for i, (x, y) in enumerate(HOLES):
        tool = build.stepped_cylz([(HEAD_D / 2, org[2] - DIVIDER_T - 0.5),
                                   (HEAD_D / 2, head_top),
                                   (SCREW_CLEAR_D / 2, head_top),
                                   (SCREW_CLEAR_D / 2, BORE_FLOOR + 0.2)],
                                  x, y, name=f"screw{i}")
        corps = build.difference(corps, tool)

    # --- plots taraudes recevant les vis du capot
    back = _at(V1XL_T_END)
    boss_len = 16.0
    t_boss = V1XL_T_END - boss_len / np.cos(TILT)
    t_web_fin = V1XL_T_END - CAP_LIP_T - 1.0     # s'arrete avant la levre
    bosses, pilots = [], []
    for i, (bx, by) in enumerate(CAP_BOSSES):
        y = by + back[1]
        bosses.append(build.cylz(CAP_BOSS_D / 2, back[2] + boss_len, back[2],
                                 bx, y, name=f"cboss{i}"))

        # Nervure rattachant le plot a la paroi. SANS ELLE LE PLOT FLOTTE : a
        # 14 mm du bord il ne touche plus rien, et rien ne le signale — un
        # cylindre isole dans une coque reste un maillage parfaitement etanche.
        # Elle s'arrete avant la levre du capot pour ne pas la gener.
        cc = np.array([np.sign(bx) * (OUT_W / 2 - OUT_R),
                       np.sign(by) * (OUT_H / 2 - OUT_R)])
        d = np.array([bx, by]) - cc
        d /= np.linalg.norm(d)
        p_paroi = cc + d * (OUT_R - WALL / 2)     # milieu de l'epaisseur
        milieu = (np.array([bx, by]) + p_paroi) / 2.0
        longueur = float(np.linalg.norm(p_paroi - np.array([bx, by]))) + CAP_BOSS_D
        base = rrect_pts(longueur, 5.0, 2.0)
        base = base @ np.array([[d[0], d[1]], [-d[1], d[0]]])
        bosses.append(_prism_seg(base + milieu, t_boss, t_web_fin,
                                 f"cweb{i}", n=3))

        pilots.append(build.cylz(CAP_SCREW_D / 2, back[2] + boss_len - 1.5,
                                 back[2] - 1.0, bx, y, name=f"cpil{i}"))
    corps = build.union(corps, bosses)
    corps = build.difference(corps, pilots)

    if motif == "cannelures":
        corps = _cannelures(corps)
    elif motif == "blocs":
        corps = _blocs(corps)
    elif motif == "genesis":
        corps = _hash_relief(corps)

    validate.report(corps, "corps final")
    return corps


# ------------------------------------------------------------------- capot
def build_capot():
    print("capot arriere...")
    back = _at(V1XL_T_END)

    capot = _plate(_contour(0.0), V1XL_T_END, CAP_T, "CapotXL")

    # Levre de centrage, rentrante dans le corps. Construite en troncon de
    # prisme (et non extrudee droite) pour suivre le cisaillement, et evidee :
    # une levre pleine ajouterait 4 mm de matiere massive sur 116 x 82.
    lip_out = _prism_seg(_contour(WALL + CAP_LIP_CLEAR),
                         V1XL_T_END - CAP_LIP_T, V1XL_T_END, "lip")
    lip_in = _prism_seg(_contour(WALL + CAP_LIP_CLEAR + 2.0),
                        V1XL_T_END - CAP_LIP_T - 1.0, V1XL_T_END + 1.0, "lipin")
    lip = build.difference(lip_out, lip_in)
    capot = build.union(capot, lip)

    # --- grille de sortie du son. Le HP loge dans le compartiment arriere :
    # sans elle il rayonnerait dans un volume totalement clos.
    org = _at(V1XL_T_END)
    y0 = org[1] - (GRILLE_SLOTS - 1) * GRILLE_STEP / 2.0
    fentes = [build.box(0, y0 + k * GRILLE_STEP, org[2] - CAP_T / 2,
                        GRILLE_LEN, GRILLE_SLOT_W, CAP_T + 2,
                        name=f"grille{k}")
              for k in range(GRILLE_SLOTS)]
    capot = build.difference(capot, fentes)

    # --- passage du cable USB-C, aligne sur celui de la cloison
    usb_pass = build.box(USB_PX, USB_PY, back[2] - CAP_T / 2,
                         USB_PASS_W, USB_PASS_H, CAP_T + CAP_LIP_T + 4,
                         "usbpass")
    capot = build.difference(capot, usb_pass)

    # percages fraises pour les 4 vis
    for i, (bx, by) in enumerate(CAP_BOSSES):
        y = by + back[1]
        tool = build.stepped_cylz(
            [(CAP_SCREW_HEAD / 2, back[2] + 1.0),
             (CAP_SCREW_HEAD / 2, back[2] - 1.4),
             (CAP_SCREW_CLEAR / 2, back[2] - 1.4),
             (CAP_SCREW_CLEAR / 2, back[2] - CAP_T - CAP_LIP_T - 1.0)],
            bx, y, name=f"ctool{i}")
        capot = build.difference(capot, tool)

    validate.report(capot, "capot final")
    return capot


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    # avant tout controle : la position du passage USB-C en depend
    _orienter("--ecran180" in argv)
    verifier_ecran()
    verifier_levre_plots()
    verifier_passage_usb()
    verifier_interferences()
    motif = ("cannelures" if "--motif" in argv
             else "blocs" if "--blocs" in argv
             else "genesis" if "--genesis" in argv else None)
    rot = "_ecran180" if ECRAN180 else ""
    suf = (f"_{motif}" if motif else "") + rot
    corps = build_corps(motif)

    dispo = V1XL_T_END - DIV_T - CAP_LIP_T
    print("--- compartiment arriere")
    print(f"    profondeur utile : {dispo:.1f} mm")
    print(f"    batterie {BATT_L:.0f}x{BATT_W_:.0f}x{BATT_T:.0f} + "
          f"HP O{SPK_D:.0f}x{SPK_T:.0f} -> empile {BATT_T + SPK_T:.0f} mm : "
          f"{'OK' if dispo > BATT_T + SPK_T + 3 else 'JUSTE'}")

    st = validate.stability(corps)
    print("--- stabilite")
    print(f"    semelle z [{st['semelle_z'][0]:.1f}, {st['semelle_z'][1]:.1f}] mm")
    print(f"    CdM z={st['com_z']:.1f} -> "
          f"{'STABLE' if st['stable'] else 'INSTABLE'}")

    build.export_stl(corps, os.path.join(
        HERE, f"boitier_xl_corps{suf}_display_orientation.stl"))
    build.apply_transform(corps, Matrix.Rotation(np.radians(90 - TILT_DEG), 4, 'X'))
    build.drop_to_bed(corps)
    print(f"    corps sur plateau : {np.round(validate.extents(corps), 1)} mm")
    build.export_stl(corps, os.path.join(HERE, f"boitier_xl_corps{suf}.stl"))

    capot = build_capot()
    build.export_stl(capot, os.path.join(
        HERE, f"boitier_xl_capot{rot}_display_orientation.stl"))
    # le capot s'imprime A PLAT (face exterieure sur le plateau), pas dans
    # l'orientation du corps : ses faces sont deja perpendiculaires a z
    build.apply_transform(capot, Matrix.Rotation(np.pi, 4, 'X'))
    build.drop_to_bed(capot)
    print(f"    capot sur plateau : {np.round(validate.extents(capot), 1)} mm")
    build.export_stl(capot, os.path.join(HERE, f"boitier_xl_capot{rot}.stl"))

    print(f"ecrits : boitier_xl_corps{suf}.stl, boitier_xl_capot{rot}.stl "
          f"(+ orientations ecran)")


if __name__ == "__main__":
    main()
