# -*- coding: utf-8 -*-
"""
Bitcoin Block Clock — boîtier V3 « Flush »  (Guition JC3248W535, ESP32-S3 3,5")
=================================================================================

Deux pièces + une jauge d'essai, générées par CSG exacte (manifold3d) :

  1. FACE   : façade « squircle » : SEUL l'écran est visible, à fleur de la
              face avant. Le module entre par l'avant dans une poche au jeu
              serré ; il est serré par ses 4 vis d'origine, par l'arrière,
              sur 4 goussets d'angle. Passage de câble USB-C sur le flanc
              gauche. Languette arrière avec 2 crochets et 2 bossages de vis.
  2. CORPS  : coque inclinée à 12° (profil en parallélogramme, dessus et
              dessous horizontaux), BANDE CANNELÉE tout autour, grille
              hexagonale pour le haut-parleur (flanc droit), berceau pour une
              batterie LiPo 10 000 mAh debout contre la paroi arrière, zone à
              plat pour une carte annexe (Arduino / chargeur), face arrière
              gravée d'un motif de blocs + plaque « BITCOIN BLOCK CLOCK ».
  3. JAUGE  : les 9 premiers mm de la FACE (poche, goussets, passage USB) pour
              valider l'ajustement du module en ~40 min avant l'impression.

Impression sans supports :
  - FACE  : face avant sur le plateau (surface parfaite autour de l'écran).
  - CORPS : face arrière sur le plateau (gravure nette), cannelures verticales.
  Toutes les parois penchent de 12° au plus ; les ponts font ≤ 13 mm.

Repère « coque » : x à droite, y en haut, z sort de l'écran (z = 0 face avant).
On construit en « espace droit » (parois selon -z) puis on applique un
cisaillement y' = y - z·tan(12°) : les parois suivent l'axe incliné, les plans
parallèles à l'écran le restent. Les éléments liés au module (poche, goussets,
USB) sont ajoutés APRÈS le cisaillement, donc exactement perpendiculaires à
l'écran.

⚠️  Cotes du module à VÉRIFIER sur ta carte avec la JAUGE (MOD_*, USB_*).
"""
import math, os, sys
import numpy as np
from manifold3d import Manifold, CrossSection, FillRule, JoinType, OpType

OUT = os.path.dirname(os.path.abspath(__file__))

# =============================================================== paramètres
# --- module JC3248W535 (face avant = verre + cadre noir, affleurant) ---
MOD_W, MOD_H = 94.5, 62.0      # contour du module (fabricant)
MOD_R = 2.0                    # rayon des coins du module
MOD_T = 4.2                    # face avant -> dos du PCB (appui sur les goussets)
MOD_CLEAR = 0.25               # jeu par côté dans la poche (ajuster à la jauge)
HOLE_DX, HOLE_DY = 84.5, 52.0  # entraxe des 4 vis d'origine
SCREW_CLEAR_D = 3.2            # passage vis
HEAD_D = 6.2                   # lamage pour la tête
WELL_FLOOR = 1.2               # matière sous la tête
PAD_T = 3.0                    # épaisseur des goussets d'angle
PAD_R = 5.3                    # rayon du plot autour du trou (dépasse de 0,3 mm le bord du module)
# --- USB-C (bord GAUCHE vu de face, connecteur au dos du PCB) ---
USB_Y = 4.6                    # centre vertical du port
USB_ZC = -(MOD_T + 1.6)        # centre en profondeur (dos du PCB + ~1,6 mm)
USB_SLOT_W, USB_SLOT_H = 13.4, 8.8   # passage pour surmoulage de fiche standard (12,3 x 8,3 typ.)
# --- enveloppe ---
TILT = 12.0                    # inclinaison de l'écran (°)
W, H, R = 124.0, 88.0, 18.0    # profil extérieur (squircle)
WALL = 2.4                     # parois
FACE_D = 16.0                  # profondeur de la façade (espace droit)
TOTAL_D = 86.0                 # profondeur totale (espace droit)
FRONT_CH = 1.2                 # chanfrein avant (côté plateau)
REAR_CH = 3.0                  # chanfrein arrière
# --- assemblage façade / corps ---
FIT = 0.2                      # jeu languette
TONGUE = 9.0                   # longueur de la languette
TONGUE_W = 2.0                 # épaisseur de la languette
SNAP_X = 30.0                  # position des crochets (haut) et vis (bas)
# --- textures ---
FLUTE_PITCH = 4.0              # pas des cannelures (ajusté pour tomber juste)
FLUTE_A = 0.9                  # relief des cannelures
BLOCK = 5.0                    # blocs gravés au dos
BLOCK_GAP = 1.5
DEBOSS = 0.6                   # profondeur de gravure
# --- haut-parleur (flanc droit) ---
SPK_D = 40.0                   # Ø du haut-parleur
SPK_Y, SPK_Z = 4.0, -48.0      # centre (espace droit)
HEX_F = 3.0                    # trou hexagonal (entre plats)
HEX_PITCH = 4.2
# --- batterie LiPo 10 000 mAh (debout contre la paroi arrière) ---
BAT_L, BAT_H, BAT_T = 110.0, 62.0, 12.5   # longueur (x), hauteur (y), épaisseur
BAT_CLEAR = 1.0
BAT_Y0 = -36.0                 # bas de la batterie (au-dessus de l'arrondi)

SEG = 72
T12 = math.tan(math.radians(TILT))
SHEAR = np.array([[1, 0, 0, 0], [0, 1, -T12, 0], [0, 0, 1, 0]], float)

# =============================================================== helpers 2D
def rrect(w, h, r, seg=SEG):
    r = max(0.01, min(r, w / 2 - 0.01, h / 2 - 0.01))
    return CrossSection.square([w - 2 * r, h - 2 * r], center=True).offset(r, JoinType.Round, 2.0, seg)

def rrect_pts(w, h, r, n):
    """points + normales sortantes, paramétrés par abscisse curviligne (CCW)."""
    hx, hy = w / 2, h / 2; cx, cy = hx - r, hy - r
    L1, L2, A = 2 * cx, 2 * cy, math.pi / 2 * r
    P = 2 * L1 + 2 * L2 + 4 * A
    pts, nrm, tag = [], [], []
    for i in range(n):
        s = i / n * P
        if s < L1:                     # bas, gauche -> droite
            pts.append((-cx + s, -hy)); nrm.append((0, -1)); tag.append('bottom')
        elif s < L1 + A:               # coin bas-droit
            a = -math.pi / 2 + (s - L1) / r
            pts.append((cx + r * math.cos(a), -cy + r * math.sin(a))); nrm.append((math.cos(a), math.sin(a))); tag.append('arc')
        elif s < L1 + A + L2:          # droite, bas -> haut
            pts.append((hx, -cy + (s - L1 - A))); nrm.append((1, 0)); tag.append('right')
        elif s < L1 + 2 * A + L2:      # coin haut-droit
            a = (s - L1 - A - L2) / r
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a))); nrm.append((math.cos(a), math.sin(a))); tag.append('arc')
        elif s < 2 * L1 + 2 * A + L2:  # haut, droite -> gauche
            pts.append((cx - (s - L1 - 2 * A - L2), hy)); nrm.append((0, 1)); tag.append('top')
        elif s < 2 * L1 + 3 * A + L2:  # coin haut-gauche
            a = math.pi / 2 + (s - 2 * L1 - 2 * A - L2) / r
            pts.append((-cx + r * math.cos(a), cy + r * math.sin(a))); nrm.append((math.cos(a), math.sin(a))); tag.append('arc')
        elif s < 2 * L1 + 3 * A + 2 * L2:  # gauche, haut -> bas
            pts.append((-hx, cy - (s - 2 * L1 - 3 * A - L2))); nrm.append((-1, 0)); tag.append('left')
        else:                          # coin bas-gauche
            a = math.pi + (s - 2 * L1 - 3 * A - 2 * L2) / r
            pts.append((-cx + r * math.cos(a), -cy + r * math.sin(a))); nrm.append((math.cos(a), math.sin(a))); tag.append('arc')
    return np.array(pts), np.array(nrm), P

def fluted_profile(w, h, r, pitch, amp):
    """squircle cannelé ; pas de cannelures sur la semelle (dessous plat)."""
    P0 = 2 * (w - 2 * r) + 2 * (h - 2 * r) + 2 * math.pi * r
    nfl = int(round(P0 / pitch))
    per = 10
    pts, nrm, P = rrect_pts(w, h, r, nfl * per)
    s = np.arange(len(pts)) / len(pts) * P
    g = 0.5 - 0.5 * np.cos(2 * math.pi * s / (P / nfl))     # 0 = creux, 1 = crête
    # poids : 0 sur la semelle (dessous plat), rampe douce sur 10 mm dans les arrondis du bas
    flat_half = w / 2 - r
    wgt = np.ones(len(pts))
    for i, (x, y) in enumerate(pts):
        if y < -h / 2 + 1e-6 and abs(x) <= flat_half + 1e-6:
            wgt[i] = 0.0
        elif y < -h / 2 + r and abs(x) > flat_half:
            a = math.atan2(y - (-h / 2 + r), abs(x) - flat_half)       # -90° (bas) .. 0° (flanc)
            k = min(1.0, (a + math.pi / 2) * r / 10.0)
            wgt[i] = k * k * (3 - 2 * k)
    off = pts + nrm * (amp * g * wgt)[:, None]
    return CrossSection([off], FillRule.Positive)

def extrude_z(cs, z0, z1, scale_at_z1=(1.0, 1.0), scale_at_z0=None):
    """extrusion de cs entre z0 et z1 (z0 > z1 ou l'inverse), échelle optionnelle."""
    lo, hi = min(z0, z1), max(z0, z1)
    if scale_at_z0 is not None:                     # échelle appliquée à l'extrémité z0
        m = Manifold.extrude(cs, hi - lo, 0, 0.0, scale_at_z0)
        return m.translate([0, 0, lo]) if z0 >= z1 else m.mirror([0, 0, 1]).translate([0, 0, hi])
    m = Manifold.extrude(cs, hi - lo, 0, 0.0, scale_at_z1)
    return m.translate([0, 0, lo]) if z1 >= z0 else m.mirror([0, 0, 1]).translate([0, 0, hi])

def cyl_axis(r, p0, p1, seg=48):
    """cylindre entre deux points."""
    p0, p1 = np.array(p0, float), np.array(p1, float)
    d = p1 - p0; L = np.linalg.norm(d); u = d / L
    m = Manifold.cylinder(L, r, r, seg)
    z = np.array([0, 0, 1.0])
    v = np.cross(z, u); c = float(np.dot(z, u))
    if np.linalg.norm(v) < 1e-9:
        Rm = np.eye(3) if c > 0 else np.diag([1, -1, -1])
    else:
        vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
        Rm = np.eye(3) + vx + vx @ vx * (1 / (1 + c))
    M = np.hstack([Rm, p0[:, None]])
    return m.transform(M)

def box(x0, x1, y0, y1, z0, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])

def union(ms):
    ms = [m for m in ms if m is not None and not m.is_empty()]
    return Manifold.batch_boolean(ms, OpType.Add) if len(ms) > 1 else ms[0]

def text_cs(text, size, font_weight='bold'):
    """texte -> CrossSection (DejaVu Sans), centré."""
    from matplotlib.textpath import TextPath
    from matplotlib.font_manager import FontProperties
    tp = TextPath((0, 0), text, size=size, prop=FontProperties(family='DejaVu Sans', weight=font_weight))
    polys = [np.array(p, float) for p in tp.to_polygons() if len(p) > 2]
    cs = CrossSection(polys, FillRule.EvenOdd)
    b = cs.bounds()
    return cs.translate([-(b[0] + b[2]) / 2, -(b[1] + b[3]) / 2])

BASE = rrect(W, H, R)
def base_off(d): return BASE.offset(d, JoinType.Round, 2.0, SEG) if d else BASE

# =============================================================== FAÇADE
def build_face():
    parts = []
    # enveloppe (espace droit) : chanfrein avant + corps
    sx, sy = (W - 2 * FRONT_CH) / W, (H - 2 * FRONT_CH) / H
    parts.append(extrude_z(BASE, 0.0, -FRONT_CH, scale_at_z0=(sx, sy)))
    parts.append(extrude_z(BASE, -FRONT_CH, -FACE_D))
    shell = union(parts)
    # cavité : derrière la dalle d'écran, épaississement en 45° vers l'arrière
    cav_in = base_off(-WALL)
    cavity = extrude_z(cav_in, -MOD_T, -(FACE_D - 4.6))
    k = TONGUE_W + FIT
    cx_s, cy_s = (W - 2 * (WALL + k)) / (W - 2 * WALL), (H - 2 * (WALL + k)) / (H - 2 * WALL)
    cavity = cavity + extrude_z(cav_in, -(FACE_D - 4.6), -(FACE_D - 4.6 + k), scale_at_z1=(cx_s, cy_s))
    shell = shell - cavity
    # languette arrière
    t_out = base_off(-(WALL + FIT)); t_in = base_off(-(WALL + FIT + TONGUE_W))
    tongue = extrude_z(t_out - t_in, -(FACE_D - 4.6 + k) + 0.01, -(FACE_D + TONGUE))
    open_back = extrude_z(t_in, -(FACE_D - 4.6 + k) + 0.02, -(FACE_D + TONGUE) - 1)
    shell = (shell + tongue) - open_back
    # crochets (haut) : rampe côté insertion
    hooks = []
    yt = H / 2 - WALL - FIT
    for sxn in (-1, 1):
        x0 = sxn * SNAP_X
        zh = -(FACE_D + TONGUE - 2.2)
        pts = [(x0 - 3, yt - 0.2, zh + 1.2), (x0 + 3, yt - 0.2, zh + 1.2), (x0 - 3, yt + 0.8, zh + 1.2), (x0 + 3, yt + 0.8, zh + 1.2),
               (x0 - 3, yt - 0.2, zh - 1.8), (x0 + 3, yt - 0.2, zh - 1.8)]
        hooks.append(Manifold.hull_points(np.array(pts)))
    # bossages de vis (bas) + trous pilotes
    bosses, pilots = [], []
    yb = -H / 2 + WALL + FIT + TONGUE_W
    zs = -(FACE_D + TONGUE / 2)
    for sxn in (-1, 1):
        x0 = sxn * SNAP_X
        bosses.append(cyl_axis(3.6, (x0, yb - 0.5, zs), (x0, yb + 3.5, zs)))
        pilots.append(cyl_axis(1.25, (x0, -H / 2 - 1, zs), (x0, yb + 2.5, zs), 24))
    shell = (shell + union(hooks + bosses)) - union(pilots)
    # ---- cisaillement 12° : les parois suivent l'axe incliné ----
    face = shell.transform(SHEAR)
    # ---- éléments liés au module (perpendiculaires à l'écran) ----
    pocket = extrude_z(rrect(MOD_W + 2 * MOD_CLEAR, MOD_H + 2 * MOD_CLEAR, MOD_R + MOD_CLEAR), 1.0, -MOD_T)
    face = face - pocket
    # goussets d'angle : du coin du module jusqu'à la paroi, sous le PCB
    hx, hy = HOLE_DX / 2, HOLE_DY / 2
    cav_sheared = extrude_z(base_off(-WALL + 0.8), -MOD_T + 0.01, -(MOD_T + PAD_T)).transform(SHEAR)
    pads, holes = [], []
    for sxn in (-1, 1):
        for syn in (-1, 1):
            cxh, cyh = sxn * hx, syn * hy
            # plot rond Ø10,6 autour du trou (empreinte minimale sous le module) ...
            disc = cyl_axis(PAD_R, (cxh, cyh, -(MOD_T + PAD_T)), (cxh, cyh, -MOD_T + 0.02), 48)
            # ... relié à la paroi par un bras en L situé HORS du contour du module
            ex, ey = sxn * MOD_W / 2, syn * MOD_H / 2
            arm_x = box(*sorted([ex - sxn * 0.4, sxn * (W / 2 + 5)]), *sorted([cyh - syn * 3.5, syn * (H / 2 + 5)]), -(MOD_T + PAD_T), -MOD_T + 0.02)
            arm_y = box(*sorted([cxh - sxn * 3.5, sxn * (W / 2 + 5)]), *sorted([ey - syn * 0.4, syn * (H / 2 + 5)]), -(MOD_T + PAD_T), -MOD_T + 0.02)
            pads.append((disc + arm_x + arm_y) ^ cav_sheared)
            holes.append(cyl_axis(SCREW_CLEAR_D / 2, (cxh, cyh, 1), (cxh, cyh, -(MOD_T + PAD_T) - 1), 32))
            holes.append(cyl_axis(HEAD_D / 2, (cxh, cyh, -(MOD_T + WELL_FLOOR)), (cxh, cyh, -(MOD_T + PAD_T) - 1), 40))
    face = (face + union(pads)) - union(holes)
    # passage USB-C (flanc gauche) : du dehors jusqu'au bord du module
    slot = rrect(USB_SLOT_W, USB_SLOT_H, 2.2)          # (y, z) local
    tool = Manifold.extrude(slot, W / 2 - MOD_W / 2 + 4.0)   # le long de +z local
    # local : x_loc -> y, y_loc -> z, z_loc -> -x (vers la gauche)
    M = np.array([[0, 0, -1, -(MOD_W / 2 - 1.0)], [1, 0, 0, USB_Y], [0, 1, 0, USB_ZC]], float)
    face = face - tool.transform(M)
    return face

# =============================================================== CORPS
def hexagon(f):
    r = f / math.sqrt(3)                       # circumradius, pointe selon +y (vertical à l'impression)
    return CrossSection([np.array([[r * math.cos(math.radians(90 + 60 * k)), r * math.sin(math.radians(90 + 60 * k))] for k in range(6)])])

def build_body():
    z_front, z_rear = -FACE_D, -TOTAL_D
    parts = []
    # rainure de joint (ligne d'ombre) puis coque pleine
    parts.append(extrude_z(base_off(-0.6), z_front, z_front - 1.0))
    parts.append(extrude_z(BASE, z_front - 1.0, z_rear + REAR_CH))
    sx, sy = (W - 2 * REAR_CH) / W, (H - 2 * REAR_CH) / H
    parts.append(extrude_z(BASE, z_rear + REAR_CH, z_rear, scale_at_z1=(sx, sy)))
    # bande cannelée
    parts.append(extrude_z(fluted_profile(W, H, R, FLUTE_PITCH, FLUTE_A), z_front - 5.0, z_rear + REAR_CH + 5.0))
    shell = union(parts)
    # cavité (ouverte à l'avant)
    shell = shell - extrude_z(base_off(-WALL), z_front + 1.0, z_rear + WALL)
    # poches des crochets (haut) + vis (bas) + patins + passe-câbles
    cuts = []
    yt = H / 2 - WALL
    zh = -(FACE_D + TONGUE - 2.2)
    for sxn in (-1, 1):
        x0 = sxn * SNAP_X
        cuts.append(box(x0 - 3.6, x0 + 3.6, yt - 0.5, yt + 1.1, zh - 2.2, zh + 1.8))
        zs = -(FACE_D + TONGUE / 2)
        cuts.append(cyl_axis(1.75, (x0, -H / 2 - 1, zs), (x0, -H / 2 + WALL + 1, zs), 32))
        cuts.append(cyl_axis(3.3, (x0, -H / 2 - 1, zs), (x0, -H / 2 + 1.3, zs), 40))
        for zf in (-(FACE_D + 14.0), -(TOTAL_D - 12.0)):
            cuts.append(cyl_axis(5.0, (sxn * 34.0, -H / 2 - 1, zf), (sxn * 34.0, -H / 2 + 1.0, zf), 48))
    for xz in (-36.0, -10.0):                      # fentes pour collier (carte annexe)
        cuts.append(box(xz - 1.8, xz + 1.8, -H / 2 - 1, -H / 2 + WALL + 1, -(FACE_D + 34.0), -(FACE_D + 29.0)))
    # fenêtre haut-parleur (flanc droit) : légère cuvette + trous hexagonaux
    win_cs = rrect(SPK_D + 6.0, SPK_D + 6.0, 9.0)          # (y, z) local
    win = Manifold.extrude(win_cs, 3.0)
    Mw = np.array([[0, 0, 1, W / 2 - 0.5], [1, 0, 0, SPK_Y], [0, 1, 0, SPK_Z]], float)
    cuts.append(win.transform(Mw))
    hexes = []
    rmax = SPK_D / 2 - 3.0
    hx_cs = hexagon(HEX_F)
    rowh = HEX_PITCH * math.sqrt(3) / 2
    j = -10
    while j <= 10:
        off = (HEX_PITCH / 2) if j % 2 else 0.0
        for i in range(-10, 11):
            u, v = i * HEX_PITCH + off, j * rowh       # u -> y, v -> z
            if math.hypot(u, v) <= rmax:
                hexes.append(Manifold.extrude(hx_cs.translate([u, v]), WALL + 3.0).transform(
                    np.array([[0, 0, 1, W / 2 - WALL - 1.5], [1, 0, 0, SPK_Y], [0, 1, 0, SPK_Z]], float)))
        j += 1
    cuts.extend(hexes)
    shell = shell - union(cuts)
    # anneau de centrage du haut-parleur
    ring = extrude_z(CrossSection.circle(SPK_D / 2 + 2.2, 96) - CrossSection.circle(SPK_D / 2 + 0.3, 96), 0, 3.0)
    ring = ring.transform(np.array([[0, 0, -1, W / 2 - WALL + 0.01], [1, 0, 0, SPK_Y], [0, 1, 0, SPK_Z]], float))
    # berceau batterie : appui + lèvre avant
    zb1 = z_rear + WALL + BAT_T + BAT_CLEAR
    cradle = box(-42, 42, -H / 2 + 0.5, BAT_Y0, z_rear + WALL - 0.01, zb1)
    lip = box(-42, 42, -H / 2 + 0.5, BAT_Y0 + 9.0, zb1, zb1 + 2.2)
    shell = shell + union([ring, cradle, lip])
    # ---- cisaillement ----
    body = shell.transform(SHEAR)
    # ---- gravure arrière (dans le plan de la face arrière, suit le cisaillement) ----
    y_shift = -z_rear * T12
    field = base_off(-(REAR_CH + 7.0))
    plaque = rrect(72, 18, 5)
    blocks = []
    pitch = BLOCK + BLOCK_GAP
    for jj in range(-12, 13):
        off = (pitch / 2) if jj % 2 else 0
        for ii in range(-14, 15):
            cx, cy = ii * pitch + off, jj * pitch
            blocks.append(rrect(BLOCK, BLOCK, 1.0, 16).translate([cx, cy]))
    blk_cs = CrossSection.batch_boolean(blocks, OpType.Add)
    blk_cs = (blk_cs ^ field) - plaque.offset(2.0, JoinType.Round, 2.0, 32)
    txt = text_cs("BITCOIN BLOCK CLOCK", 4.6)
    deb = Manifold.extrude(blk_cs + txt.mirror([1, 0]), DEBOSS + 0.5)      # miroir : lu de dos
    deb = deb.translate([0, y_shift, z_rear - 0.5])
    body = body - deb
    return body

# =============================================================== sorties
def to_stl(m, path, orient):
    mesh = m.to_mesh()
    V = np.asarray(mesh.vert_properties)[:, :3].astype(np.float64)
    F = np.asarray(mesh.tri_verts).astype(np.int64)
    if orient == 'face':            # face avant sur le plateau
        V = V * np.array([1, -1, -1])
    V -= [ (V[:, 0].min() + V[:, 0].max()) / 2, (V[:, 1].min() + V[:, 1].max()) / 2, V[:, 2].min()]
    import trimesh
    tm = trimesh.Trimesh(V, F, process=False)
    tm.export(path)
    return tm

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    print("façade..."); face = build_face()
    print("  statut", face.status(), "volume %.1f cm3" % (face.volume() / 1000))
    print("corps..."); body = build_body()
    print("  statut", body.status(), "volume %.1f cm3" % (body.volume() / 1000))
    gauge = face ^ box(-W, W, -H, H, -9.0, 1.0)
    for m, name, o in ((face, 'bbc_v3_face.stl', 'face'), (body, 'bbc_v3_corps.stl', 'body'), (gauge, 'bbc_v3_jauge_ecran.stl', 'face')):
        tm = to_stl(m, os.path.join(OUT, name), o)
        print(f"  {name}: {len(tm.faces)} triangles, étanche={tm.is_watertight}, "
              f"{tm.extents[0]:.1f} x {tm.extents[1]:.1f} x {tm.extents[2]:.1f} mm, ~{m.volume() / 1000 * 1.24:.0f} g PLA (plein)")
