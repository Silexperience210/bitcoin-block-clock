# -*- coding: utf-8 -*-
"""Rendu studio des boitiers — remplace render_preview.py (matplotlib).

L'ancien apercu tracait les facettes en 3D avec matplotlib : pas d'ombres
reelles, aretes visibles, aucune matiere. On rend ici en EEVEE avec un vrai
eclairage trois points.

Note d'echelle : le modele est en millimetres mais Blender raisonne en metres
pour l'eclairage. On travaille donc sur une copie mise a l'echelle 1/1000,
ce qui permet des puissances de lampes normales.
"""
import bpy
import numpy as np
from mathutils import Vector

MM = 0.001  # facteur d'echelle mm -> m pour l'eclairage


def _look_at(ob, target):
    d = Vector(target) - ob.location
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


def material(ob, color, roughness=0.42, metallic=0.0):
    m = bpy.data.materials.new(f"{ob.name}_mat")
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Roughness"].default_value = roughness
    if "Metallic" in b.inputs:
        b.inputs["Metallic"].default_value = metallic
    ob.data.materials.clear()
    ob.data.materials.append(m)
    return m


def _bounds(obs):
    lo = np.array([1e9] * 3)
    hi = -lo
    for ob in obs:
        for v in ob.data.vertices:
            w = np.array(ob.matrix_world @ v.co)
            lo, hi = np.minimum(lo, w), np.maximum(hi, w)
    return lo, hi


def scale_to_meters(obs, tilt_deg=12.0):
    """Echelle metre, orientation "pose sur un bureau", recentrage.

    Les coordonnees coque ont y en haut et z vers l'arriere ; Blender est en
    Z-up. On combine donc le basculement Z-up (+90 deg autour de X) avec
    l'inclinaison de la piece (-tilt), soit une seule rotation de 90 - tilt :
    la semelle repose alors a plat et l'ecran penche comme en vrai.
    """
    from mathutils import Matrix

    rot = Matrix.Rotation(np.radians(90.0 - tilt_deg), 4, 'X')
    for ob in obs:
        ob.matrix_world = rot @ ob.matrix_world
        ob.scale = (MM, MM, MM)
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    lo, hi = _bounds(obs)
    mid = (lo + hi) / 2
    for ob in obs:                      # centre en x/y, pose sur z = 0
        ob.location = (ob.location[0] - mid[0],
                       ob.location[1] - mid[1],
                       ob.location[2] - lo[2])
    bpy.context.view_layer.update()
    return float(np.linalg.norm(hi - lo))


def studio(diag, key=(1.1, -1.5, 1.3), fill=(-1.5, -0.9, 0.5),
           rim=(0.2, 1.6, 0.9)):
    """Eclairage trois points + fond neutre, dimensionne sur la diagonale."""
    scene = bpy.context.scene

    for name, d, energy, size in (("Key", key, 55.0, 1.4),
                                  ("Fill", fill, 16.0, 2.2),
                                  ("Rim", rim, 28.0, 1.0)):
        ld = bpy.data.lights.new(name, 'AREA')
        ld.energy = energy * diag * diag
        ld.size = size * diag
        lo = bpy.data.objects.new(name, ld)
        bpy.context.collection.objects.link(lo)
        lo.location = tuple(c * diag for c in d)
        _look_at(lo, (0, 0, 0.25 * diag))

    w = scene.world or bpy.data.worlds.new("World")
    scene.world = w
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (.05, .05, .06, 1)

    # sol legerement sous la piece
    bpy.ops.mesh.primitive_plane_add(size=20 * diag, location=(0, 0, 0))
    floor = bpy.context.active_object
    floor.name = "Sol"
    material(floor, (0.07, 0.07, 0.08), roughness=0.65)
    return floor


def camera(diag, direction=(1.0, -1.9, 0.85), lens=70):
    cd = bpy.data.cameras.new("Cam")
    cd.lens = lens
    cam = bpy.data.objects.new("Cam", cd)
    bpy.context.collection.objects.link(cam)
    cam.location = tuple(c * diag for c in direction)
    cam.location.z = abs(cam.location.z) + 0.15 * diag
    _look_at(cam, (0, 0, 0.25 * diag))
    bpy.context.scene.camera = cam
    return cam


def render(path, res=(1400, 1000), samples=48):
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE'
    try:
        scene.eevee.taa_render_samples = samples
    except AttributeError:
        pass
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    return path
