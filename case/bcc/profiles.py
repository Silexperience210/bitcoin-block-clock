# -*- coding: utf-8 -*-
"""Generateurs de contours 2D — numpy pur, aucune dependance externe.

Porte tel quel depuis build_case.py / build_case_deep.py : ces fonctions
etaient deja independantes de trimesh, elles tournent sous le Python embarque
de Blender sans modification.

Tous les contours sont fermes, centres, et orientes CCW vus de +z.
"""
import numpy as np

from .params import N


def stadium_pts(W, H, n=N):
    """Contour 'stade' (obround) W x H centre, points CCW vus de +z."""
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
    """Rectangle a coins arrondis W x H centre, rayon r, CCW vu de +z."""
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
    """Re-echantillonne une polyligne fermee (M,d) en n points (perimetre).

    Fonctionne en 2D comme en 3D : l'interpolation est lineaire entre sommets
    consecutifs, donc une corde posee sur un plan le reste apres passage.
    """
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


def circle_pts(r, n=48, cx=0.0, cy=0.0):
    """Cercle CCW vu de +z — base des cylindres (plots, percages, logements)."""
    a = np.linspace(0.0, 2.0 * np.pi, n, endpoint=False)
    return np.column_stack([cx + r * np.cos(a), cy + r * np.sin(a)])
