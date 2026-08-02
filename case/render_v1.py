# -*- coding: utf-8 -*-
"""Apercu rendu du boitier v1.

    blender --background --python case/render_v1.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402

from bcc import build, render  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(os.path.dirname(HERE), "images")

VUES = {
    "iso": ((1.05, -1.6, 0.75), 68),
    "dos": ((0.9, 1.5, 0.7), 68),
}


def main():
    stl = os.path.join(HERE, "boitier_bitcoinclock_display_orientation.stl")

    for nom, (direction, lens) in VUES.items():
        build.purge_scene()
        bpy.ops.wm.stl_import(filepath=stl)
        ob = bpy.context.selected_objects[0]
        ob.name = "Boitier"

        for poly in ob.data.polygons:      # dome lisse, aretes vives gardees
            poly.use_smooth = True
        mod = ob.modifiers.new("Aretes", 'EDGE_SPLIT')
        mod.split_angle = 0.55

        render.material(ob, (0.36, 0.39, 0.44), roughness=0.38)

        diag = render.scale_to_meters([ob])
        render.studio(diag)
        render.camera(diag, direction, lens)

        out = os.path.join(IMG, f"preview_v1_{nom}.png")
        render.render(out)
        print("rendu ->", out)


if __name__ == "__main__":
    main()
