# -*- coding: utf-8 -*-
"""Boitier BitcoinClock v1 "compact" — construction sous Blender.

    blender --background --python case/build_v1_blender.py
    blender --background --python case/build_v1_blender.py -- --legacy

Le mode --legacy reproduit la geometrie d'origine (plots pleins, BAND_Z=-10,
DOME_Z=-20) pour comparer au STL produit par l'ancien pipeline trimesh : c'est
le test de non-regression du portage. Le mode par defaut applique le correctif
des bossages de la carte.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy  # noqa: E402
from mathutils import Matrix  # noqa: E402

from bcc import build, validate  # noqa: E402
from bcc.params import (BOARD_H, BOARD_W, BOSS_BORE_T, DOME_ARGS, base_pastille,  # noqa: E402
                        HEAD_D, HOLES, POCKET_CLEAR, POCKET_DEPTH, POCKET_RELIEF,
                        POST_D, SCREW_CLEAR_D, USB_H, USB_W, USB_Y, USB_ZTOP,
                        V1_BASE_RISE, V1_CAV_FLOOR_Z, V1_CAV_INSET, WALL,
                        WELL_FLOOR)
from bcc.rings import pillow_rings  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def build_shell(legacy=False):
    band_z, dome_z = DOME_ARGS[bool(legacy)]

    build.purge_scene()

    print(f"construction de la coque (BAND_Z={band_z}, DOME_Z={dome_z})...")
    shell = build.loft(pillow_rings(band_z, dome_z, V1_BASE_RISE), "Boitier")

    # --- logement de la carte + cavite composants + USB-C
    pocket_floor = -POCKET_DEPTH - (0.0 if legacy else POCKET_RELIEF)
    pocket = build.box_tool(0, 0, pocket_floor, 1.0,
                            BOARD_W + 2 * POCKET_CLEAR,
                            BOARD_H + 2 * POCKET_CLEAR, 4.5, "pocket")
    cavity = build.box_tool(0, 0, V1_CAV_FLOOR_Z, -POCKET_DEPTH,
                            BOARD_W + 2 * POCKET_CLEAR - 2 * V1_CAV_INSET,
                            BOARD_H + 2 * POCKET_CLEAR - 2 * V1_CAV_INSET,
                            3.0, "cavity")
    usb = build.box(-(BOARD_W / 2) - 10.0, USB_Y, USB_ZTOP - USB_H / 2,
                    30.0, USB_W, USB_H, "usb")

    shell = build.difference(shell, [pocket, cavity, usb])
    validate.report(shell, "apres pocket/cavity/usb")

    if legacy:
        # --- geometrie d'origine : plots pleins jusqu'au dos (suppose plan)
        posts = [build.cylz(POST_D / 2, -16.0, -POCKET_DEPTH + 0.1, x, y,
                            name=f"post{i}")
                 for i, (x, y) in enumerate(HOLES)]
        shell = build.union(shell, posts)
        validate.report(shell, "apres plots")
        bore_floor = None
    else:
        # --- correctif. La coque approfondie serait un bloc massif sous la
        # cavite : on l'evide en suivant le dome (paroi WALL), ce qui laisse
        # aussi la place aux bossages de la carte.
        inner = build.loft(pillow_rings(band_z, dome_z, V1_BASE_RISE,
                                        inset=WALL, z_max=V1_CAV_FLOOR_Z),
                           "inner")
        shell = build.difference(shell, inner)
        validate.report(shell, "apres evidement")

        # 4 pastilles portant les bossages : c'est sur elles que la carte
        # s'appuie, et les vis les plaquent l'une contre l'autre.
        # base_pastille() les demarre STRICTEMENT dans l'epaisseur de paroi :
        # une pastille qui ressort du dome (cas des plots d'origine) ou qui
        # affleure sa surface rend le maillage non-manifold.
        bore_floor = -POCKET_DEPTH - BOSS_BORE_T
        pad_base = base_pastille(band_z, dome_z)
        print(f"pastilles : de {pad_base:.2f} a {bore_floor:.2f}")
        pads = [build.cylz(POST_D / 2, pad_base, bore_floor, x, y,
                           name=f"pad{i}")
                for i, (x, y) in enumerate(HOLES)]
        shell = build.union(shell, pads)
        validate.report(shell, "apres pastilles d'appui")

    # --- puits de vis : fraisure O6 depuis l'exterieur + passage O3.2
    head_top = (-POCKET_DEPTH - WELL_FLOOR if legacy
                else bore_floor - WELL_FLOOR)
    thru_top = (-POCKET_DEPTH + 0.2 if legacy else bore_floor + 0.2)
    # outil etage unique (fraisure + passage) : deux cylindres concentriques
    # soustraits separement generent des facettes d'aire nulle
    for i, (x, y) in enumerate(HOLES):
        tool = build.stepped_cylz([(HEAD_D / 2, dome_z - 1),
                                   (HEAD_D / 2, head_top),
                                   (SCREW_CLEAR_D / 2, head_top),
                                   (SCREW_CLEAR_D / 2, thru_top)],
                                  x, y, name=f"screw{i}")
        shell = build.difference(shell, tool)

    validate.report(shell, "final")
    return shell, bore_floor


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    legacy = "--legacy" in argv
    suffix = "_legacy" if legacy else ""

    shell, bore_floor = build_shell(legacy)

    # --- controle du defaut d'origine : le logement debouche-t-il ?
    if bore_floor is not None:
        print("--- degagement sous les logements de bossages")
        for hx, hy, clr in validate.boss_bore_clearance(shell, bore_floor):
            verdict = "OK" if clr > 2.0 else ("LIMITE" if clr > 0 else "PERCE")
            print(f"    vis ({hx:+6.2f}, {hy:+6.2f}) : {clr:6.2f} mm  {verdict}")

    # --- stabilite
    st = validate.stability(shell)
    print("--- stabilite")
    print(f"    semelle z [{st['semelle_z'][0]:.1f}, {st['semelle_z'][1]:.1f}] mm, "
          f"x [{st['semelle_x'][0]:.1f}, {st['semelle_x'][1]:.1f}] mm")
    print(f"    CdM a {st['hauteur_com']:.1f} mm au-dessus de la table, "
          f"z={st['com_z']:.1f} -> {'STABLE' if st['stable'] else 'INSTABLE'}")

    # --- exports : orientation ecran + orientation impression
    out_disp = os.path.join(HERE, f"boitier_bitcoinclock{suffix}_display_orientation.stl")
    build.export_stl(shell, out_disp)

    # face avant (z=0) vers le plateau : rotation 180 deg autour de X
    build.apply_transform(shell, Matrix.Rotation(np.pi, 4, 'X'))
    build.drop_to_bed(shell)
    out_print = os.path.join(HERE, f"boitier_bitcoinclock{suffix}.stl")
    build.export_stl(shell, out_print)

    print(f"ecrits : {os.path.basename(out_print)}, {os.path.basename(out_disp)}")


if __name__ == "__main__":
    main()
