# -*- coding: utf-8 -*-
"""Controles geometriques — reimplementation des garanties de l'ancien pipeline.

L'ancien code s'appuyait sur trimesh (is_watertight, center_mass, volume) et sur
un debug_check() qui rejouait la quantification float32 de l'export STL. Tout
est refait ici avec bmesh + numpy, sans dependance externe.
"""
import bmesh
import numpy as np

from .params import (HOLES, MASS_BOARD_G, MASS_SHELL_G, POCKET_DEPTH,
                     BOARD_W, BOARD_H, TILT_DEG)


def _bm(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    return bm


def verts_faces(ob):
    """Sommets (V,3) et faces triangulees (F,3) en coordonnees locales."""
    bm = _bm(ob)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    V = np.array([v.co[:] for v in bm.verts], dtype=np.float64)
    bm.faces.ensure_lookup_table()
    F = np.array([[v.index for v in f.verts] for f in bm.faces], dtype=np.int64)
    bm.free()
    return V, F


def is_watertight(ob):
    """Toutes les aretes bordees par exactement 2 faces."""
    bm = _bm(ob)
    bad = [e for e in bm.edges if len(e.link_faces) != 2]
    n = len(bad)
    bm.free()
    return n == 0, n


def edges_after_float32(ob):
    """Compte les aretes != 2 APRES quantification float32.

    Rejoue exactement ce que subit le maillage a l'export STL : des sommets
    distincts en float64 peuvent fusionner en float32 et rouvrir le maillage.
    C'est ce controle qui garantit le "zero reparation dans le slicer".
    """
    V, F = verts_faces(ob)
    q = V.astype(np.float32).astype(np.float64)

    # fusion exacte des sommets coincidents apres quantification
    uniq, inv = np.unique(q, axis=0, return_inverse=True)
    Fq = inv[F]

    # une face degeneree (deux sommets fusionnes) ne borde plus rien
    keep = (Fq[:, 0] != Fq[:, 1]) & (Fq[:, 1] != Fq[:, 2]) & (Fq[:, 0] != Fq[:, 2])
    Fq = Fq[keep]

    e = np.vstack([Fq[:, [0, 1]], Fq[:, [1, 2]], Fq[:, [2, 0]]])
    e = np.sort(e, axis=1)
    _, counts = np.unique(e, axis=0, return_counts=True)
    return int((counts != 2).sum()), int((~keep).sum())


def volume_mm3(ob):
    """Volume signe (mm3) par decomposition tetraedrique."""
    V, F = verts_faces(ob)
    a, b, c = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    return float(np.abs(np.einsum('ij,ij->i', a, np.cross(b, c)).sum()) / 6.0)


def center_mass(ob):
    """Centre de masse d'un solide homogene, par tetraedres signes."""
    V, F = verts_faces(ob)
    a, b, c = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    vol6 = np.einsum('ij,ij->i', a, np.cross(b, c))
    centro = (a + b + c) / 4.0
    total = vol6.sum()
    if abs(total) < 1e-9:
        return V.mean(axis=0)
    return (centro * vol6[:, None]).sum(axis=0) / total


def bounds(ob):
    V, _ = verts_faces(ob)
    return V.min(axis=0), V.max(axis=0)


def extents(ob):
    lo, hi = bounds(ob)
    return hi - lo


# ------------------------------------------------------- controles metier
def boss_bore_clearance(ob, bore_bottom_z):
    """Matiere restante sous le fond des logements de bossages.

    C'est le controle du defaut qui a motive la refonte : si le logement
    debouche a l'arriere, la valeur est negative.
    """
    V, _ = verts_faces(ob)
    out = []
    for hx, hy in HOLES:
        d = np.hypot(V[:, 0] - hx, V[:, 1] - hy)
        near = V[d < 8.0]
        if len(near) == 0:
            out.append((hx, hy, float('nan')))
            continue
        # surface exterieure la plus arriere autour de la vis
        out.append((hx, hy, float(bore_bottom_z - near[:, 2].min())))
    return out


def stability(ob):
    """Projection du centre de masse (coque + carte) dans le polygone d'appui.

    Coordonnees monde : on applique le tilt de -TILT_DEG autour de x, la table
    est le plan y = min.
    """
    from mathutils import Matrix

    R = np.array(Matrix.Rotation(np.radians(-TILT_DEG), 3, 'X'))

    V, F = verts_faces(ob)
    Vw = V @ R.T
    com_shell = center_mass(ob) @ R.T

    # carte assimilee a un pave dans la poche
    com_board = np.array([0.0, 0.0, -POCKET_DEPTH + 2.0]) @ R.T

    com = ((com_shell * MASS_SHELL_G + com_board * MASS_BOARD_G)
           / (MASS_SHELL_G + MASS_BOARD_G))

    ymin = Vw[:, 1].min()
    foot = Vw[Vw[:, 1] < ymin + 0.5]
    z0, z1 = foot[:, 2].min(), foot[:, 2].max()
    return {
        "hauteur_com": float(com[1] - ymin),
        "com_z": float(com[2]),
        "semelle_z": (float(z0), float(z1)),
        "semelle_x": (float(foot[:, 0].min()), float(foot[:, 0].max())),
        "stable": bool(z0 < com[2] < z1),
    }


def report(ob, label=""):
    """Bilan complet imprime sur stdout."""
    wt, nbad = is_watertight(ob)
    nfloat, ndegen = edges_after_float32(ob)
    vol = volume_mm3(ob)
    ext = extents(ob)

    print(f"--- controle {label or ob.name}")
    print(f"    etanche          : {wt}" + ("" if wt else f"  ({nbad} aretes != 2)"))
    print(f"    apres float32    : {nfloat} aretes != 2, {ndegen} faces degenerees")
    print(f"    encombrement     : {np.round(ext, 2)} mm")
    print(f"    volume           : {vol / 1000:.1f} cm3  ->  "
          f"{vol / 1000 * 1.24:.1f} g PETG")
    return {"watertight": wt, "bad_edges": nbad, "float32_bad": nfloat,
            "volume_mm3": vol, "extents": ext}
