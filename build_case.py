# -*- coding: utf-8 -*-
"""
Boitier BitcoinClock pour Guition JC3248W535 (ESP32-S3 + écran 3.5" 480x320).

Coque arrière style "Alexa" : oblongue arrondie (stade + dôme), légère
inclinaison de 12°, l'écran nu forme la face avant, fixation par les 4 vis
d'origine de la carte (puits fraisés à l'arrière).

Unités : millimètres.  Système de coordonnées "coque" :
  - z = 0 sur la face avant, la coque s'étend vers z négatif (vers l'arrière)
  - x = gauche/droite vue de face, y = haut/bas
Le STL final est pivoté pour l'impression (face avant à plat sur le plateau).

Construction : lofts paramétriques (aucune facette dégénérée), semelle plane
obtenue par coupe EXACTE des anneaux (cordes sur le plan), booléens manifold
uniquement pour des intersections franches (perpendiculaires).
"""
import numpy as np
import trimesh
from shapely.geometry import Polygon
from trimesh.creation import triangulate_polygon
from trimesh.boolean import difference, union

# ---------------------------------------------------------------- paramètres
BOARD_W, BOARD_H = 94.5, 62.0       # contour de la carte (mesuré fabricant)
POCKET_DEPTH = 4.2                  # PCB 1.6 + cadre LCD ~2.4 + marge
POCKET_CLEAR = 0.3                  # jeu par côté
HOLE_DX, HOLE_DY = 84.5, 52.0       # entraxe des 4 vis (trous à 5 mm des bords)
SCREW_CLEAR_D = 3.2                 # trou de passage vis M2.5/M3
HEAD_D = 6.0                        # fraisure pour tête de vis (~Ø4 mm)
WELL_FLOOR = 1.0                    # matière restante sous la tête de vis
POST_D = 9.0                        # diamètre des plots
RIM = 10.0                          # largeur du cadre autour de la carte
WALL = 2.2                          # épaisseur parois (référence)
BAND_Z = -10.0                      # fin de la bande verticale, début du dôme
DOME_Z = -20.0                      # profondeur totale au centre
CAV_FLOOR_Z = -10.5                 # fond plan de la cavité composants
CAV_INSET = 2.5                     # retrait de la cavité vs logement carte
TILT_DEG = 12.0                     # inclinaison vers l'arrière
BASE_RISE = 4.5                     # hauteur de la semelle plane
# Découpe USB-C : bord gauche vue de face, centre mesuré à y = +4.6 mm
USB_Y = 4.6
USB_W, USB_H = 11.0, 5.2            # largeur (selon y) / hauteur (selon z)
USB_ZTOP = -0.3                     # le connecteur affleure le haut du PCB

N = 160  # points par anneau

# ------------------------------------------------------- générateurs de forme
def stadium_pts(W, H, n=N):
    """Contour 'stade' (obround) W x H centré, points CCW vus de +z."""
    R, L = H / 2.0, max(W - H, 0.0)
    per_seg, per_arc = L, np.pi * R
    P = 2 * per_seg + 2 * per_arc
    pts = []
    for i in range(n):
        s = i / n * P
        if s < per_seg:                       # segment haut (gauche->droite)
            pts.append((-L / 2 + s, R))
        elif s < per_seg + per_arc:           # arc droit (haut->bas)
            a = np.pi / 2 - (s - per_seg) / R
            pts.append((L / 2 + R * np.cos(a), R * np.sin(a)))
        elif s < 2 * per_seg + per_arc:       # segment bas (droite->gauche)
            s2 = s - per_seg - per_arc
            pts.append((L / 2 - s2, -R))
        else:                                 # arc gauche (bas->haut)
            a = -np.pi / 2 - (s - 2 * per_seg - per_arc) / R
            pts.append((-L / 2 + R * np.cos(a), R * np.sin(a)))
    return np.array(pts)


def rrect_pts(W, H, r, n=N):
    """Rectangle à coins arrondis W x H centré, rayon r, CCW vu de +z."""
    hx, hy = W / 2.0, H / 2.0
    r = min(r, hx, hy)
    seg_x, seg_y = 2 * (hx - r), 2 * (hy - r)
    arc = np.pi / 2 * r
    P = 2 * seg_x + 2 * seg_y + 4 * arc
    cx, cy = hx - r, hy - r
    pts = []
    for i in range(n):
        s = i / n * P
        if s < seg_x:                         # haut : droite -> gauche
            pts.append((cx - s, hy))
        elif s < seg_x + arc:                 # coin haut-gauche
            a = np.pi / 2 + (s - seg_x) / r
            pts.append((-cx + r * np.cos(a), cy + r * np.sin(a)))
        elif s < seg_x + arc + seg_y:         # gauche : haut -> bas
            s2 = s - seg_x - arc
            pts.append((-hx, cy - s2))
        elif s < seg_x + 2 * arc + seg_y:     # coin bas-gauche
            a = np.pi + (s - seg_x - arc - seg_y) / r
            pts.append((-cx + r * np.cos(a), -cy + r * np.sin(a)))
        elif s < 2 * seg_x + 2 * arc + seg_y:  # bas : gauche -> droite
            s2 = s - seg_x - 2 * arc - seg_y
            pts.append((-cx + s2, -hy))
        elif s < 2 * seg_x + 3 * arc + seg_y:  # coin bas-droit
            a = 3 * np.pi / 2 + (s - 2 * seg_x - 2 * arc - seg_y) / r
            pts.append((cx + r * np.cos(a), -cy + r * np.sin(a)))
        elif s < 2 * seg_x + 3 * arc + 2 * seg_y:  # droite : bas -> haut
            s2 = s - 2 * seg_x - 3 * arc - seg_y
            pts.append((hx, -cy + s2))
        else:                                 # coin haut-droit
            a = (s - 2 * seg_x - 3 * arc - 2 * seg_y) / r
            pts.append((cx + r * np.cos(a), cy + r * np.sin(a)))
    return np.array(pts)


def resample_closed(pts, n=N):
    """Ré-échantillonne une polyligne fermée (M,d) en n points (périmètre)."""
    d = np.diff(np.vstack([pts, pts[:1]]), axis=0)
    seg = np.linalg.norm(d, axis=1)
    cum = np.concatenate([[0.0], np.cumsum(seg)])
    total = cum[-1]
    out = []
    j = 0
    for i in range(n):
        s = i / n * total
        while j < len(seg) - 1 and cum[j + 1] < s:
            j += 1
        t = 0.0 if seg[j] == 0 else (s - cum[j]) / seg[j]
        out.append(pts[j] * (1 - t) + pts[(j + 1) % len(pts)] * t)
    return np.array(out)


def cap_earcut(pts2d, z, reverse=False):
    """Cap planaire triangulée (earcut) à la hauteur z (scalaire)."""
    poly = Polygon(pts2d)
    if not poly.is_valid or poly.area < 1e-6:
        return [], []
    v, f = triangulate_polygon(poly)
    V = [(float(x), float(y), float(z)) for x, y in v]
    F = [tuple(t) for t in (f[:, ::-1] if reverse else f)]
    return V, F


def loft(rings3d, cap_end_fan=True):
    """Mesh étanche à partir d'anneaux 3D [(M,3), ...] (M identique).

    Anneaux CCW vus de +z, ordonnés du plus grand z au plus petit z.
    Cap avant (premier anneau, planaire) : earcut. Cap arrière : éventail.
    """
    n = len(rings3d[0])
    V, F = [], []
    for P in rings3d:
        V.extend(map(tuple, P))
    for k in range(len(rings3d) - 1):
        for i in range(n):
            a, b = k * n + i, k * n + (i + 1) % n
            c, d = (k + 1) * n + (i + 1) % n, (k + 1) * n + i
            F.append((a, b, c))
            F.append((a, c, d))
    # cap avant (premier anneau) : éventail depuis le centroïde, normale +z
    # (éventail : fonctionne même si l'anneau est non planaire après coupe)
    c0 = len(V)
    V.append(tuple(np.asarray(rings3d[0]).mean(axis=0)))
    for i in range(n):
        F.append((c0, (i + 1) % n, i))
    # cap arrière (dernier anneau) : éventail, normale -z
    c1 = len(V)
    off = (len(rings3d) - 1) * n
    V.append(tuple(np.asarray(rings3d[-1]).mean(axis=0)))
    for i in range(n):
        F.append((c1, off + i, off + (i + 1) % n))
    m = trimesh.Trimesh(vertices=np.array(V), faces=np.array(F), process=True)
    if m.volume < 0:
        m.invert()
    return m


def box_tool(cx, cy, z0, z1, W, H, r):
    """Outil de soustraction : prisme rect arrondi entre z0 et z1."""
    pts = rrect_pts(W, H, r)
    V1, F1 = cap_earcut(pts, z0, reverse=True)
    V2, F2 = cap_earcut(pts, z1, reverse=False)
    V = V1 + V2
    n = len(pts)
    F = list(F1) + [(a + len(V1), b + len(V1), c + len(V1)) for a, b, c in F2]
    for i in range(n):
        a, b = i, (i + 1) % n
        c, d = (i + 1) % n + len(V1), i + len(V1)
        F.append((a, b, c))
        F.append((a, c, d))
    m = trimesh.Trimesh(vertices=np.array(V), faces=np.array(F), process=True)
    if m.volume < 0:
        m.invert()
    m.apply_translation([cx, cy, 0])
    return m


def cyl(r, z0, z1, cx=0.0, cy=0.0, sec=48):
    m = trimesh.creation.cylinder(radius=r, height=abs(z1 - z0), sections=sec)
    m.apply_translation([cx, cy, (z0 + z1) / 2])
    return m


# ------------------------------------------------------------------- coque
def build_outer_pillow():
    """Solide extérieur : stade, bande verticale, dôme, semelle plane 12°."""
    Wo, Ho = BOARD_W + 2 * (RIM + 0.45), BOARD_H + 2 * RIM   # ~116 x 82
    tilt = np.radians(TILT_DEG)
    u = np.array([0.0, np.cos(tilt), np.sin(tilt)])  # "haut monde" en coords coque

    # anneaux bruts (z, contour)
    raw = []
    for t in np.linspace(0, 1, 4):                # chanfrein avant
        s = 0.95 + 0.05 * t
        raw.append((-2.0 * t, stadium_pts(Wo * s, Ho * s)))
    raw.append((BAND_Z, stadium_pts(Wo, Ho)))     # bande verticale
    for z in np.linspace(BAND_Z, DOME_Z, 41)[1:]:  # dôme cos^0.75
        uu = (z - BAND_Z) / (DOME_Z - BAND_Z)
        s = np.cos(uu * np.pi / 2) ** 0.75
        raw.append((z, stadium_pts(Wo * max(s, 0.02), Ho * max(s, 0.02))))

    def f_of(pts, z):
        return pts[:, 0] * u[0] + pts[:, 1] * u[1] + z * u[2]

    c = min(float(f_of(pts, z).min()) for z, pts in raw) + BASE_RISE

    # coupe exacte : chaque anneau est conservé, coupé (corde sur le plan)
    # ou supprimé ; la base = loft des cordes (toutes sur le plan u.p=c)
    rings3d = []
    for z, pts in raw:
        f = f_of(pts, z) - c
        if (f >= 0).all():
            rings3d.append(np.column_stack(
                [pts.astype(float), np.full(len(pts), float(z))]))
            continue
        if (f < 0).all():
            continue
        # --- anneau traversant : arc conservé + corde sur le plan
        n = len(pts)
        kept = f >= 0
        crossings = []
        for i in range(n):
            j = (i + 1) % n
            if kept[i] != kept[j]:                # croisement i -> j
                t = f[i] / (f[i] - f[j])
                P3 = np.array([*(pts[i] * (1 - t) + pts[j] * t), float(z)])
                P3 -= (P3 @ u - c) * u            # projection exacte sur le plan
                crossings.append((i, P3))
        if len(crossings) != 2:                   # cas pathologique : on garde
            rings3d.append(np.column_stack(       # l'anneau tel quel
                [pts.astype(float), np.full(len(pts), float(z))]))
            continue
        (iA, PA), (iB, PB) = crossings
        # orienter : les sommets après iA doivent être conservés
        if not kept[(iA + 1) % n]:
            (iA, PA), (iB, PB) = (iB, PB), (iA, PA)
        # arc conservé : de PA à PB en passant par les sommets gardés
        arc = [PA]
        idx = (iA + 1) % n
        while True:
            if kept[idx]:
                arc.append(np.array([pts[idx][0], pts[idx][1], float(z)]))
            if idx == iB:
                break
            idx = (idx + 1) % n
        arc.append(PB)
        # polyligne 3D fermée : arc (z anneau) + corde PB->PA (sur le plan) ;
        # le ré-échantillonnage interpole en 3D, la corde reste sur le plan
        rings3d.append(resample_closed(np.array(arc), N))
    return loft(rings3d)


def debug_check(m, tag):
    """Reproduit le cycle export STL float32 -> fusion exacte -> comptage."""
    q = np.asarray(m.vertices, dtype=np.float32).astype(np.float64)
    mm = trimesh.Trimesh(vertices=q, faces=np.asarray(m.faces),
                         process=True, validate=False)
    edges = np.sort(mm.edges, axis=1)
    uniq, counts = np.unique(edges, axis=0, return_counts=True)
    nbad = int((counts != 2).sum())
    print(f'  [debug] {tag}: arêtes !=2 -> {nbad}')
    return nbad


hxs, hys = HOLE_DX / 2.0, HOLE_DY / 2.0
HOLES = [(sx * hxs, sy * hys) for sx in (1, -1) for sy in (1, -1)]

print("construction de la coque...")
pillow = build_outer_pillow()
assert pillow.is_watertight, "pillow non étanche"
debug_check(pillow, "pillow (avec semelle)")

pocket = box_tool(0, 0, -POCKET_DEPTH, 1.0,
                  BOARD_W + 2 * POCKET_CLEAR, BOARD_H + 2 * POCKET_CLEAR, 4.5)
cavity = box_tool(0, 0, CAV_FLOOR_Z, -POCKET_DEPTH,
                  BOARD_W + 2 * POCKET_CLEAR - 2 * CAV_INSET,
                  BOARD_H + 2 * POCKET_CLEAR - 2 * CAV_INSET, 3.0)

usb = trimesh.creation.box(extents=(30.0, USB_W, USB_H))
usb.apply_translation([
    -(BOARD_W / 2) - 10.0,          # traverse la paroi gauche
    USB_Y,
    USB_ZTOP - USB_H / 2,
])

posts = union([cyl(POST_D / 2, -16.0, -POCKET_DEPTH + 0.1, cx=x, cy=y)
               for x, y in HOLES], engine="manifold")

shell = difference([pillow, pocket, cavity, usb], engine="manifold")
debug_check(shell, "après pocket/cavity/usb")
shell = union([shell, posts], engine="manifold")
debug_check(shell, "après posts")

# puits de vis : fraisure Ø6 depuis l'extérieur + trou de passage Ø3.2
for x, y in HOLES:
    well = cyl(HEAD_D / 2, DOME_Z - 1, -POCKET_DEPTH - WELL_FLOOR, cx=x, cy=y)
    hole = cyl(SCREW_CLEAR_D / 2, DOME_Z - 1, -POCKET_DEPTH + 0.2, cx=x, cy=y)
    shell = difference([shell, well, hole], engine="manifold")
debug_check(shell, "après puits")
assert shell.is_watertight, "coque non étanche après perçages"


def sanitize(m):
    """Round-trip manifold3d : garantit un solide parfaitement étanche.
    IMPORTANT : ne pas laisser trimesh fusionner les sommets (process=False),
    sinon des micro-facettes s'effondrent et rouvrent le maillage."""
    import manifold3d
    out = manifold3d.Manifold(mesh=manifold3d.Mesh(
        vert_properties=np.asarray(m.vertices, dtype=np.float32),
        tri_verts=np.asarray(m.faces, dtype=np.uint32))).to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(out.vert_properties[:, :3]),
                           faces=np.asarray(out.tri_verts),
                           process=False, validate=False)


shell = sanitize(shell)
print("étanche après assainissement :", shell.is_watertight)
debug_check(shell, "final")

# ------------------------------------------------- orientation impression
# face avant (z=0) vers le plateau : rotation 180° autour de X
flip = trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0])
shell_print = shell.copy()
shell_print.apply_transform(flip)
shell_print.apply_translation([0, 0, -shell_print.bounds[0, 2]])

shell.export("boitier_bitcoinclock_display_orientation.stl")
shell_print.export("boitier_bitcoinclock.stl")

# ------------------------------------------------------------------ bilan
print("étanche :", shell_print.is_watertight)
print("encombrement impression (x, y, z) mm :",
      np.round(shell_print.extents, 2))
print("volume coque :", round(shell.volume / 1000, 1), "cm3  ->",
      round(shell.volume / 1000 * 1.24, 1), "g PETG")

# stabilité : projection du centre de masse (coque + carte ~90 g) dans la
# semelle, en coords monde (on applique le tilt de -12° autour de x)
Rworld = trimesh.transformations.rotation_matrix(-np.radians(TILT_DEG), [1, 0, 0])
sw = shell.copy(); sw.apply_transform(Rworld)
board = trimesh.creation.box(extents=(BOARD_W, BOARD_H, 4.0))
board.apply_translation([0, 0, -POCKET_DEPTH + 2.0])
board.apply_transform(Rworld)
# coque imprimée ~30 g (parois + remplissage 25 %), carte complète ~90 g
m_shell, m_board = 30.0, 90.0
com = (sw.center_mass * m_shell + board.center_mass * m_board) / (m_shell + m_board)
ymin = sw.vertices[:, 1].min()
foot = sw.vertices[sw.vertices[:, 1] < ymin + 0.5]
print("semelle : profondeur z_monde [%.1f, %.1f] mm, largeur x [%.1f, %.1f]"
      % (foot[:, 2].min(), foot[:, 2].max(), foot[:, 0].min(), foot[:, 0].max()))
print("CdM monde (y=%.1f mm au-dessus table, z=%.1f mm) -> %s"
      % (com[1] - ymin, com[2],
         "STABLE" if foot[:, 2].min() < com[2] < foot[:, 2].max() else "INSTABLE"))
print("fichiers écrits : boitier_bitcoinclock.stl (impression),",
      "boitier_bitcoinclock_display_orientation.stl")
