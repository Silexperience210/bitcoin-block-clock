# -*- coding: utf-8 -*-
"""
Boitier BitcoinClock "deep" pour Guition JC3248W535 — version 2 pièces,
profondeur ~15 cm pour haut-parleur + batterie 2000 mAh.

Architecture :
  - AVANT : cadre avec logement carte (pocket), 4 plots + puits pour les vis
    d'origine, découpe USB-C, boss arrière, fente de passage des fils.
  - ARRIÈRE : coque profonde (prisme oblique incliné 12°) qui coulisse sur le
    boss du cadre, fond plat stable sur toute la profondeur, face arrière
    verticale avec grille haut-parleur, 4 vis M3 radiales de jonction.

Coords "coque" : x droite, y haut, z avant -> arrière (z=0 face avant).
Le prisme est extrudé le long de t = (0, sin12°, -cos12°) : en coordonnées
monde, le dessus et le dessous sont horizontaux, l'écran penche 12° vers
l'arrière. Pour l'impression, on aligne t avec +Z (bols, sans supports).
"""
import numpy as np
import trimesh
from shapely.geometry import Polygon
from trimesh.creation import triangulate_polygon
from trimesh.boolean import difference, union

# ---------------------------------------------------------------- paramètres
BOARD_W, BOARD_H = 94.5, 62.0
POCKET_DEPTH = 4.2
POCKET_CLEAR = 0.3
HOLE_DX, HOLE_DY = 84.5, 52.0
SCREW_CLEAR_D = 3.2
HEAD_D = 6.0
WELL_FLOOR = 1.0
POST_D = 9.0
TILT_DEG = 12.0

OUT_W, OUT_H, OUT_R = 116.0, 82.0, 18.0   # profil extérieur (rect arrondi)
WALL = 2.2
FRAME_T = 15.0          # longueur du cadre le long de t
SHOULDER_T = 8.0        # épaulement (début du boss)
BOSS_INSET = 2.5        # retrait du boss
TUBE_T_END = 150.0      # profondeur totale le long de t (~15 cm)
TUBE_RIM_T = 10.0       # longueur du bord arrondi arrière
BACK_CAP_S = 0.85       # échelle du profil à la face arrière
CAV_FLOOR_Z = -10.3     # fond de la cavité composants du cadre
CAV_INSET = 2.5
JOIN_SCREW_D = 2.5      # pilotes M3 auto-taraudeuses (jonction cadre/tube)
USB_Y = 4.6
USB_W, USB_H = 11.0, 5.2
USB_ZTOP = -0.3
WIRE_W, WIRE_H = 50.0, 12.0   # fente de passage des fils (bas)
GRILLE_N, GRILLE_PITCH = 8, 5.0

N = 160
t_hat = np.array([0.0, np.sin(np.radians(TILT_DEG)), -np.cos(np.radians(TILT_DEG))])

# ------------------------------------------------------------------ helpers
def rrect_pts(W, H, r, n=N):
    hx, hy = W / 2.0, H / 2.0
    r = min(r, hx, hy)
    seg_x, seg_y = 2 * (hx - r), 2 * (hy - r)
    arc = np.pi / 2 * r
    P = 2 * seg_x + 2 * seg_y + 4 * arc
    cx, cy = hx - r, hy - r
    pts = []
    for i in range(n):
        s = i / n * P
        if s < seg_x:
            pts.append((cx - s, hy))
        elif s < seg_x + arc:
            a = np.pi / 2 + (s - seg_x) / r
            pts.append((-cx + r * np.cos(a), cy + r * np.sin(a)))
        elif s < seg_x + arc + seg_y:
            pts.append((-hx, cy - (s - seg_x - arc)))
        elif s < seg_x + 2 * arc + seg_y:
            a = np.pi + (s - seg_x - arc - seg_y) / r
            pts.append((-cx + r * np.cos(a), -cy + r * np.sin(a)))
        elif s < 2 * seg_x + 2 * arc + seg_y:
            pts.append((-cx + (s - seg_x - 2 * arc - seg_y), -hy))
        elif s < 2 * seg_x + 3 * arc + seg_y:
            a = 3 * np.pi / 2 + (s - 2 * seg_x - 2 * arc - seg_y) / r
            pts.append((cx + r * np.cos(a), -cy + r * np.sin(a)))
        elif s < 2 * seg_x + 3 * arc + 2 * seg_y:
            pts.append((hx, -cy + (s - 2 * seg_x - 3 * arc - seg_y)))
        else:
            a = (s - 2 * seg_x - 3 * arc - 2 * seg_y) / r
            pts.append((cx + r * np.cos(a), cy + r * np.sin(a)))
    return np.array(pts)


def cap_earcut(pts2d, z, reverse=False):
    poly = Polygon(pts2d)
    if not poly.is_valid or poly.area < 1e-6:
        return [], []
    v, f = triangulate_polygon(poly)
    V = [(float(x), float(y), float(z)) for x, y in v]
    F = [tuple(t) for t in (f[:, ::-1] if reverse else f)]
    return V, F


def prism(sections, flip_caps=(True, False)):
    """Prisme oblique le long de t_hat.
    sections = [(s, pts2d), ...] : profil (x,y) placé à p + s*t_hat.
    Anneaux plans (z constant par anneau) -> caps earcut aux deux bouts."""
    rings = []
    for s, pts in sections:
        P = np.column_stack([pts[:, 0] + s * t_hat[0],
                             pts[:, 1] + s * t_hat[1],
                             np.full(len(pts), -s * 1.0) * 0.0 + s * t_hat[2]])
        rings.append(P)
    n = len(rings[0])
    V, F = [], []
    for P in rings:
        V.extend(map(tuple, P))
    for k in range(len(rings) - 1):
        for i in range(n):
            a, b = k * n + i, k * n + (i + 1) % n
            c, d = (k + 1) * n + (i + 1) % n, (k + 1) * n + i
            F.append((a, b, c))
            F.append((a, c, d))
    # caps (z constant par anneau -> earcut en xy)
    v0 = len(V)
    Vc, Fc = cap_earcut(np.asarray(rings[0])[:, :2], rings[0][0, 2],
                        reverse=flip_caps[0])
    V.extend(Vc)
    F.extend([(a + v0, b + v0, c + v0) for a, b, c in Fc])
    v1 = len(V)
    Vc, Fc = cap_earcut(np.asarray(rings[-1])[:, :2], rings[-1][0, 2],
                        reverse=flip_caps[1])
    V.extend(Vc)
    F.extend([(a + v1, b + v1, c + v1) for a, b, c in Fc])
    m = trimesh.Trimesh(vertices=np.array(V), faces=np.array(F), process=True)
    if m.volume < 0:
        m.invert()
    return m


def box_tool(cx, cy, z0, z1, W, H, r):
    """Outil prisme droit (selon z) rect arrondi entre z0 et z1."""
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


def cyl(r, h, center, axis=(0, 0, 1), sec=40):
    m = trimesh.creation.cylinder(radius=r, height=h, sections=sec)
    if axis != (0, 0, 1):
        m.apply_transform(trimesh.geometry.align_vectors([0, 0, 1], axis))
    m.apply_translation(center)
    return m


def cylz(r, z0, z1, x, y, sec=40):
    """Cylindre d'axe z couvrant [z0, z1] (ordre quelconque), centré (x, y)."""
    return cyl(r, abs(z1 - z0), (x, y, (z0 + z1) / 2), sec=sec)


def sanitize(m):
    import manifold3d
    out = manifold3d.Manifold(mesh=manifold3d.Mesh(
        vert_properties=np.asarray(m.vertices, dtype=np.float32),
        tri_verts=np.asarray(m.faces, dtype=np.uint32))).to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(out.vert_properties[:, :3]),
                           faces=np.asarray(out.tri_verts),
                           process=False, validate=False)


def debug_check(m, tag):
    q = np.asarray(m.vertices, dtype=np.float32).astype(np.float64)
    mm = trimesh.Trimesh(vertices=q, faces=np.asarray(m.faces),
                         process=True, validate=False)
    edges = np.sort(mm.edges, axis=1)
    uniq, counts = np.unique(edges, axis=0, return_counts=True)
    print(f'  [debug] {tag}: arêtes !=2 -> {int((counts != 2).sum())}')


hxs, hys = HOLE_DX / 2.0, HOLE_DY / 2.0
HOLES = [(sx * hxs, sy * hys) for sx in (1, -1) for sy in (1, -1)]
BACK_Z = -FRAME_T * abs(t_hat[2])   # z de la face arrière du cadre (-14.67)

# ============================================================== CADRE AVANT
print("cadre avant...")
prof_full = rrect_pts(OUT_W, OUT_H, OUT_R)
prof_boss = rrect_pts(OUT_W - 2 * BOSS_INSET, OUT_H - 2 * BOSS_INSET,
                      OUT_R - BOSS_INSET)
frame = prism([(0.0, prof_full),
               (SHOULDER_T, prof_full),
               (SHOULDER_T + 0.3, prof_boss),
               (FRAME_T, prof_boss)])
debug_check(frame, "frame brut")

pocket = box_tool(0, 0, -POCKET_DEPTH, 1.0,
                  BOARD_W + 2 * POCKET_CLEAR, BOARD_H + 2 * POCKET_CLEAR, 4.5)
cavity = box_tool(0, 0, CAV_FLOOR_Z, -POCKET_DEPTH,
                  BOARD_W + 2 * POCKET_CLEAR - 2 * CAV_INSET,
                  BOARD_H + 2 * POCKET_CLEAR - 2 * CAV_INSET, 3.0)
usb = trimesh.creation.box(extents=(30.0, USB_W, USB_H))
usb.apply_translation([-(BOARD_W / 2) - 10.0, USB_Y, USB_ZTOP - USB_H / 2])
# fentes de passage des fils (bas : HP/IO, haut : batterie -> coque arrière)
wire = box_tool(0, -20.0, -9.0, BACK_Z - 2.0, WIRE_W, WIRE_H, 3.0)
wire_bat = box_tool(20.0, 20.0, -9.0, BACK_Z - 2.0, 24.0, 10.0, 3.0)

# plots : du fond du logement carte jusqu'à la face arrière du cadre
posts = union([cylz(POST_D / 2, BACK_Z - 0.2, -POCKET_DEPTH + 0.1, x, y)
               for x, y in HOLES], engine="manifold")

frame = difference([frame, pocket, cavity, usb, wire, wire_bat],
                   engine="manifold")
debug_check(frame, "frame - pocket/cavity/usb/wire")
frame = union([frame, posts], engine="manifold")
debug_check(frame, "frame + posts")

# puits de vis : accessibles depuis la face arrière du cadre (côté coque)
for x, y in HOLES:
    well = cylz(HEAD_D / 2, BACK_Z - 0.3, -POCKET_DEPTH - WELL_FLOOR, x, y)
    hole = cylz(SCREW_CLEAR_D / 2, BACK_Z - 0.4, -POCKET_DEPTH + 0.2, x, y)
    frame = difference([frame, well, hole], engine="manifold")
debug_check(frame, "frame après puits")

# pilotes M3 radiaux de jonction (2 par côté), à mi-boss
JOIN_T = (SHOULDER_T + FRAME_T) / 2 + 1.5
join_tools = []
for sx in (1, -1):
    for sy in (1, -1):
        cx = sx * (OUT_W / 2 + 5)
        join_tools.append(
            cyl(JOIN_SCREW_D / 2, 20.0,
                (sx * (OUT_W / 2 + 5 - 10), sy * 15.0 + JOIN_T * t_hat[1],
                 JOIN_T * t_hat[2]), axis=(1, 0, 0)))
join = join_tools[0]
for jt in join_tools[1:]:
    join = union([join, jt], engine="manifold")
frame = difference([frame, join], engine="manifold")
debug_check(frame, "frame après pilotes")

frame = sanitize(frame)
debug_check(frame, "frame final")

# ============================================================ COQUE ARRIÈRE
print("coque arrière...")
prof_inner = rrect_pts(OUT_W - 2 * BOSS_INSET + 0.3, OUT_H - 2 * BOSS_INSET + 0.3,
                       OUT_R - BOSS_INSET)
# extérieur : plein jusqu'à TUBE_T_END - TUBE_RIM_T, puis arrondi du bord
outer_secs = [(SHOULDER_T, prof_full)]
rim_ts = np.linspace(TUBE_T_END - TUBE_RIM_T, TUBE_T_END, 5)
outer_secs.append((rim_ts[0], prof_full))
for tt in rim_ts[1:]:
    uu = (tt - rim_ts[0]) / TUBE_RIM_T
    s = 1.0 - (1.0 - BACK_CAP_S) * np.sin(uu * np.pi / 2)
    outer_secs.append((tt, rrect_pts(OUT_W * s, OUT_H * s, OUT_R * s)))
tube_outer = prism(outer_secs)

tube_inner = prism([(SHOULDER_T - 0.2, prof_inner),
                    (TUBE_T_END - WALL, prof_inner)])
tube = difference([tube_outer, tube_inner], engine="manifold")
debug_check(tube, "tube creusé")

# grille haut-parleur sur la face arrière (fentes selon t, montées à y=+15
# pour être à mi-hauteur de la face en position posée)
GRILLE_Y = 15.0
slots = []
for k in range(GRILLE_N):
    x = (k - (GRILLE_N - 1) / 2) * GRILLE_PITCH
    slots.append(prism([(TUBE_T_END - WALL - 2.0, rrect_pts(3.0, 20.0, 1.4)),
                        (TUBE_T_END + 1.0, rrect_pts(3.0, 20.0, 1.4))],
                       flip_caps=(True, False)))
    slots[-1].apply_translation([x, GRILLE_Y, 0])
    # le profil est en coords profil (x,y) : l'extrusion part de z=0 -> rien à translater en z
grille = slots[0]
for sl in slots[1:]:
    grille = union([grille, sl], engine="manifold")
tube = difference([tube, grille], engine="manifold")
debug_check(tube, "tube après grille")

# trous de jonction M3 (alignés sur les pilotes du cadre)
tube = difference([tube, join], engine="manifold")
debug_check(tube, "tube après trous jonction")

tube = sanitize(tube)
debug_check(tube, "tube final")

# ================================================== orientation impression
# t_hat -> +Z : rotation 168° autour de X
R_print = trimesh.transformations.rotation_matrix(np.radians(168.0), [1, 0, 0])

# cadre : face arrière (t=FRAME_T) sur le plateau -> on flippe après rotation
frame_p = frame.copy()
frame_p.apply_transform(R_print)
frame_p.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0]))
frame_p.apply_translation([0, 0, -frame_p.bounds[0, 2]])

# tube : face arrière (t=TUBE_T_END) sur le plateau
tube_p = tube.copy()
tube_p.apply_transform(R_print)
tube_p.apply_transform(trimesh.transformations.rotation_matrix(np.pi, [1, 0, 0]))
tube_p.apply_translation([0, 0, -tube_p.bounds[0, 2]])

frame.export("boitier_deep_avant_display.stl")
tube.export("boitier_deep_arriere_display.stl")
frame_p.export("boitier_deep_avant.stl")
tube_p.export("boitier_deep_arriere.stl")

# ------------------------------------------------------------------ bilan
for name, m in (("avant", frame_p), ("arrière", tube_p)):
    print(name, ": étanche", m.is_watertight,
          "| encombrement", np.round(m.extents, 1),
          "| matière", round(abs(m.volume) / 1000, 1), "cm3")

# stabilité monde (assemblé) : rotation -12° autour de X puis pose sur y=min
asm = trimesh.util.concatenate([frame.copy(), tube.copy()])
Rworld = trimesh.transformations.rotation_matrix(-np.radians(TILT_DEG), [1, 0, 0])
asm.apply_transform(Rworld)
board = trimesh.creation.box(extents=(BOARD_W, BOARD_H, 4.0))
board.apply_translation([0, 0, -POCKET_DEPTH + 2.0])
board.apply_transform(Rworld)
m_shell = (abs(frame.volume) + abs(tube.volume)) * 1.24e-3 * 0.35  # ~remplissage
m_all = m_shell + 90.0 + 60.0   # coque + carte + batterie/haut-parleur
com = (asm.center_mass * m_shell + board.center_mass * 90.0) / (m_shell + 90.0)
com = (com * (m_shell + 90.0) + np.array([0, 0, -60]) * 60.0) / m_all
ymin = asm.vertices[:, 1].min()
foot = asm.vertices[asm.vertices[:, 1] < ymin + 0.5]
print("semelle monde : z [%.1f, %.1f] x [%.1f, %.1f]" %
      (foot[:, 2].min(), foot[:, 2].max(), foot[:, 0].min(), foot[:, 0].max()))
print("CdM z=%.1f -> %s" % (com[2],
      "STABLE" if foot[:, 2].min() < com[2] < foot[:, 2].max() else "INSTABLE"))
