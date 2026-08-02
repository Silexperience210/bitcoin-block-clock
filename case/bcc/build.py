# -*- coding: utf-8 -*-
"""Construction bmesh + booleens MANIFOLD + export STL.

Remplace le trio trimesh / shapely / manifold3d :
  - loft()      -> quads bmesh natifs (plus de triangulation manuelle)
  - box_tool()  -> prisme bmesh (n-gons, shapely devient inutile)
  - difference/union(engine="manifold") -> BooleanModifier(solver='MANIFOLD'),
    qui est le MEME noyau manifold3d.
"""
import bmesh
import bpy
import numpy as np

from .profiles import circle_pts, rrect_pts


# --------------------------------------------------------------- utilitaires
def purge_scene():
    """Vide la scene — indispensable en --background ou le cube par defaut."""
    bpy.ops.wm.read_factory_settings(use_empty=True)


def _mesh_from_bm(bm, name):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    return ob


def loft(rings3d, name="loft"):
    """Solide etanche a partir d'anneaux 3D [(M,3), ...] de meme cardinalite.

    Anneaux CCW vus de +z, ordonnes du plus grand z au plus petit. Les flancs
    sont des quads ; les deux extremites sont fermees par un eventail depuis le
    centroide, ce qui reste valide meme si l'anneau n'est plus planaire apres
    la coupe du plan de semelle.
    """
    bm = bmesh.new()
    layers = [[bm.verts.new(tuple(map(float, p))) for p in ring]
              for ring in rings3d]
    n = len(layers[0])

    for a, b in zip(layers, layers[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))

    # eventail avant (normale +z) et arriere (normale -z)
    c0 = bm.verts.new(tuple(np.asarray(rings3d[0], dtype=float).mean(axis=0)))
    for i in range(n):
        bm.faces.new((c0, layers[0][(i + 1) % n], layers[0][i]))

    c1 = bm.verts.new(tuple(np.asarray(rings3d[-1], dtype=float).mean(axis=0)))
    for i in range(n):
        bm.faces.new((c1, layers[-1][i], layers[-1][(i + 1) % n]))

    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return _mesh_from_bm(bm, name)


def _extrude_profile(pts2d, z0, z1, name):
    """Prisme droit entre z0 et z1 a partir d'un contour 2D ferme."""
    bm = bmesh.new()
    lo = [bm.verts.new((float(x), float(y), float(z0))) for x, y in pts2d]
    hi = [bm.verts.new((float(x), float(y), float(z1))) for x, y in pts2d]
    n = len(pts2d)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bm.faces.new(lo[::-1])
    bm.faces.new(hi)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return _mesh_from_bm(bm, name)


def box_tool(cx, cy, z0, z1, W, H, r, name="box_tool"):
    """Outil de soustraction : prisme rect arrondi entre z0 et z1."""
    pts = rrect_pts(W, H, r)
    pts = pts + np.array([cx, cy])
    return _extrude_profile(pts, min(z0, z1), max(z0, z1), name)


def cylz(r, z0, z1, cx=0.0, cy=0.0, sec=48, name="cyl"):
    """Cylindre d'axe z entre z0 et z1."""
    return _extrude_profile(circle_pts(r, sec, cx, cy),
                            min(z0, z1), max(z0, z1), name)


def stepped_cylz(steps, cx=0.0, cy=0.0, sec=48, name="stepped"):
    """Cylindre etage d'axe z, defini par [(rayon, z), ...] ordonne en z.

    Sert a couper fraisure + passage de vis EN UN SEUL outil. Deux cylindres
    concentriques soustraits separement partagent des surfaces coincidentes,
    ce qui produit des facettes d'aire nulle et rouvre le maillage a la
    quantification float32 de l'export STL.
    """
    steps = sorted(steps, key=lambda s: -s[1])       # z decroissant
    rings = [circle_pts(r, sec, cx, cy) for r, _ in steps]
    rings3d = [np.column_stack([ring, np.full(len(ring), float(z))])
               for ring, (_, z) in zip(rings, steps)]
    return loft(rings3d, name)


def box(cx, cy, cz, sx, sy, sz, name="box"):
    """Pave droit centre sur (cx, cy, cz)."""
    pts = np.array([(-sx / 2, -sy / 2), (sx / 2, -sy / 2),
                    (sx / 2, sy / 2), (-sx / 2, sy / 2)]) + np.array([cx, cy])
    return _extrude_profile(pts, cz - sz / 2, cz + sz / 2, name)


# ----------------------------------------------------------------- booleens
def _boolean(target, tool, operation):
    """Applique un booleen MANIFOLD et consomme l'outil."""
    mod = target.modifiers.new(name="bool", type='BOOLEAN')
    mod.operation = operation
    mod.solver = 'MANIFOLD'
    mod.object = tool

    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=mod.name)

    bpy.data.objects.remove(tool, do_unlink=True)
    return target


def difference(target, tools):
    for t in tools if isinstance(tools, (list, tuple)) else [tools]:
        target = _boolean(target, t, 'DIFFERENCE')
    return target


def union(target, tools):
    for t in tools if isinstance(tools, (list, tuple)) else [tools]:
        target = _boolean(target, t, 'UNION')
    return target


def intersect(target, tools):
    for t in tools if isinstance(tools, (list, tuple)) else [tools]:
        target = _boolean(target, t, 'INTERSECT')
    return target


def join(objs):
    """Fusionne des objets DISJOINTS en un seul maillage, sans booleen.

    Un motif de N pastilles coute N booleens si on les soustrait une a une.
    Comme elles ne se recoupent pas, on peut simplement concatener leurs
    maillages : le solide multi-composant reste manifold, et une seule
    difference suffit ensuite.
    """
    objs = [o for o in objs if o is not None]
    if not objs:
        return None
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    return objs[0]


def duplicate(ob, name=None):
    dup = ob.copy()
    dup.data = ob.data.copy()
    if name:
        dup.name = name
    bpy.context.collection.objects.link(dup)
    return dup


# NOTE sur l'assainissement du maillage
# ------------------------------------
# L'ancien pipeline terminait par un round-trip manifold3d. Deux equivalents
# ont ete essayes ici et sont a EVITER :
#   - bmesh.ops.dissolve_degenerate : effondre les micro-facettes et ROUVRE le
#     maillage (l'ancien code documentait deja ce piege) ;
#   - union de la coque avec elle-meme : surfaces coincidentes partout, le
#     solveur produit des dizaines d'aretes non-manifold.
# La bonne reponse est en amont : ne pas creer de tangences pathologiques
# (cf. stepped_cylz pour les puits de vis, et la profondeur de coque qui evite
# aux percages d'affleurer le dome). Un maillage issu de booleens francs sort
# propre sans post-traitement.


# ------------------------------------------------------------- transformations
def apply_transform(ob, matrix):
    ob.matrix_world = matrix @ ob.matrix_world
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return ob


def drop_to_bed(ob):
    """Pose l'objet sur z=0 (plateau d'impression)."""
    zmin = min((ob.matrix_world @ v.co).z for v in ob.data.vertices)
    ob.location.z -= zmin
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.transform_apply(location=True)
    return ob


# ------------------------------------------------------------------- export
def triangulate(ob):
    """Triangule le maillage en place.

    A faire AVANT l'export : laisse a l'exporteur STL le soin de retrianguler
    les n-gons issus des booleens produit des fissures (aretes bordees par une
    seule face) alors que le maillage est sain dans Blender. On impose donc la
    triangulation de bmesh, dont la validation verifie qu'elle sort propre.
    """
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return ob


def export_stl(ob, path):
    """Export STL binaire de l'objet seul."""
    triangulate(ob)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.wm.stl_export(filepath=str(path), export_selected_objects=True,
                          ascii_format=False, apply_modifiers=True)
    return path
