# -*- coding: utf-8 -*-
"""Generation des anneaux 3D et coupe de plan exacte — numpy pur.

Porte tel quel depuis build_case.py (build_outer_pillow). C'est la partie la
plus delicate du modele : la semelle plane est obtenue non pas par un booleen
(qui laisserait des micro-facettes) mais par une COUPE EXACTE des anneaux —
chaque anneau traversant le plan est remplace par son arc conserve plus une
corde dont les deux extremites sont projetees analytiquement sur le plan.
Tous les points de la semelle sont donc rigoureusement coplanaires.
"""
import numpy as np

from .params import N, V1_BASE_RISE, TILT_DEG, v1_outer_size
from .profiles import rrect_pts, stadium_pts, resample_closed


def _raw_rings(band_z, dome_z, inset=0.0):
    """Anneaux bruts (z, contour 2D) du galet v1 : chanfrein, bande, dome.

    `inset` rentre la surface de cette distance (parois) : le contour est reduit
    de 2*inset et le dome remonte d'autant, ce qui approche un offset uniforme
    sur une surface aussi douce.
    """
    Wo, Ho = v1_outer_size()
    Wo, Ho = Wo - 2 * inset, Ho - 2 * inset
    band_z, dome_z = band_z + inset, dome_z + inset

    raw = []
    for t in np.linspace(0, 1, 4):                 # chanfrein avant
        s = 0.95 + 0.05 * t
        raw.append((-2.0 * t, stadium_pts(Wo * s, Ho * s)))
    raw.append((band_z, stadium_pts(Wo, Ho)))      # bande verticale
    for z in np.linspace(band_z, dome_z, 41)[1:]:  # dome cos^0.75
        uu = (z - band_z) / (dome_z - band_z)
        s = np.cos(uu * np.pi / 2) ** 0.75
        raw.append((z, stadium_pts(Wo * max(s, 0.02), Ho * max(s, 0.02))))
    return raw


def pillow_rings(band_z, dome_z, base_rise=V1_BASE_RISE, tilt_deg=TILT_DEG,
                 inset=0.0, z_max=None):
    """Anneaux 3D du galet v1, coupes par le plan de la semelle inclinee.

    Retourne une liste d'anneaux (N,3) ordonnes du plus grand z au plus petit,
    prets a etre loftes. Le plan de coupe est u.p = c, ou u est le "haut monde"
    exprime en coordonnees coque.

    Avec `inset` > 0 on obtient la surface INTERIEURE : la soustraire au solide
    exterieur donne une coque creuse d'epaisseur `inset`, au lieu d'un bloc
    massif. Le plan de semelle est lui aussi remonte de `inset`.
    """
    tilt = np.radians(tilt_deg)
    u = np.array([0.0, np.cos(tilt), np.sin(tilt)])   # "haut monde" en coords coque

    raw = _raw_rings(band_z, dome_z, inset)
    if z_max is not None:
        # evidement limite a l'arriere : on ne garde que les anneaux sous z_max,
        # le loft refermant l'avant par un eventail plan a cette cote
        raw = [(z, pts) for z, pts in raw if z <= z_max]
        if not raw:
            raise ValueError("z_max au-dessus de tous les anneaux")

    def f_of(pts, z):
        return pts[:, 0] * u[0] + pts[:, 1] * u[1] + z * u[2]

    # le plan de coupe se cale sur la geometrie EXTERIEURE, puis remonte de
    # `inset` : sinon la semelle interieure ne serait pas parallele a l'exterieure
    outer = _raw_rings(band_z, dome_z, 0.0)
    c = min(float(f_of(pts, z).min()) for z, pts in outer) + base_rise + inset

    rings3d = []
    for z, pts in raw:
        f = f_of(pts, z) - c
        if (f >= 0).all():
            rings3d.append(np.column_stack(
                [pts.astype(float), np.full(len(pts), float(z))]))
            continue
        if (f < 0).all():
            continue
        # --- anneau traversant : arc conserve + corde sur le plan
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
        # orienter : les sommets apres iA doivent etre conserves
        if not kept[(iA + 1) % n]:
            (iA, PA), (iB, PB) = (iB, PB), (iA, PA)
        # arc conserve : de PA a PB en passant par les sommets gardes
        arc = [PA]
        idx = (iA + 1) % n
        while True:
            if kept[idx]:
                arc.append(np.array([pts[idx][0], pts[idx][1], float(z)]))
            if idx == iB:
                break
            idx = (idx + 1) % n
        arc.append(PB)
        # polyligne 3D fermee : arc (z anneau) + corde PB->PA (sur le plan) ;
        # le re-echantillonnage interpole en 3D, la corde reste sur le plan
        rings3d.append(resample_closed(np.array(arc), N))
    return rings3d


def ring_at(z, band_z, dome_z, inset=0.0):
    """Contour 2D de la surface (exterieure ou interieure) a la cote z.

    Sert a poser une cloison qui epouse exactement la section interne : un
    plateau trop grand ressortirait de la coque, trop petit laisserait une
    fente parasite.
    """
    raw = _raw_rings(band_z, dome_z, inset)
    zs = np.array([zz for zz, _ in raw])
    i = int(np.argmin(np.abs(zs - z)))
    return raw[i][1]


def prism_rings(W, H, R, t_end, rim_t, back_s, inset=0.0, tilt_deg=TILT_DEG,
                n_body=6, n_rim=12, t0=None, t1=None):
    """Anneaux 3D d'un prisme OBLIQUE a fond plat.

    Le contour (rectangle arrondi) reste parallele au plan XY, mais son origine
    glisse le long de t = (0, sin(tilt), -cos(tilt)). En coordonnees monde le
    dessus et le dessous sont donc horizontaux tandis que la face avant penche
    de `tilt` : le boitier repose a plat sur toute sa profondeur.

    C'est ce qui distingue cette construction du galet (dome coupe par un plan),
    dont l'appui recule quand on l'approfondit au point de le faire basculer.

    `back_s` ferme l'arriere en reduisant le contour sur les `rim_t` derniers
    millimetres.
    """
    tilt = np.radians(tilt_deg)
    t_hat = np.array([0.0, np.sin(tilt), -np.cos(tilt)])

    W, H = W - 2 * inset, H - 2 * inset
    R = max(R - inset, 0.5)
    # par defaut la paroi ferme aussi les extremites ; passer t1 > t_end fait
    # deboucher le volume interieur, ce qui OUVRE le dos (capot rapporte)
    t0 = inset if t0 is None else t0
    t1 = (t_end - inset) if t1 is None else t1

    rings = []
    for t in np.linspace(t0, t1 - rim_t, n_body):
        rings.append((t, rrect_pts(W, H, R)))

    for k in range(1, n_rim + 1):          # conge arriere en quart de cosinus
        u = k / n_rim
        t = (t1 - rim_t) + u * rim_t
        s = back_s + (1.0 - back_s) * np.cos(u * np.pi / 2) ** 0.8
        rings.append((t, rrect_pts(W * s, H * s, R * s)))

    out = []
    for t, pts in rings:
        ring = np.column_stack([pts[:, 0], pts[:, 1], np.zeros(len(pts))])
        out.append(ring + t * t_hat)
    return out
