# SPDX-License-Identifier: Apache-2.0
# Copyright 2025-2026 silexperience — https://github.com/Silexperience210/bitcoin-block-clock
# Contrôle d'intégration : volumes d'interférence composants / boîtier (doivent être ~0)
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_case_v3 as C
from manifold3d import Manifold
face = C.build_face(); body = C.build_body()
case = face + body
S = C.SHEAR
def sheared(m): return m.transform(S)
comps = {}
# module : bloc avant (verre + cadre + PCB) + électronique au dos (hors goussets)
comps['module (verre->PCB)'] = C.extrude_z(C.rrect(C.MOD_W, C.MOD_H, C.MOD_R), 0.0, -C.MOD_T)
# électronique au dos : tout le PCB SAUF une zone Ø9 autour de chaque vis (comme le V1)
back = C.extrude_z(C.rrect(C.MOD_W - 1, C.MOD_H - 1, 2), -C.MOD_T, -C.MOD_T - 7.0)
for sx_ in (-1, 1):
    for sy_ in (-1, 1):
        back = back - C.cyl_axis(5.4, (sx_*C.HOLE_DX/2, sy_*C.HOLE_DY/2, 0), (sx_*C.HOLE_DX/2, sy_*C.HOLE_DY/2, -20), 48)
comps['module (électronique au dos, 7 mm)'] = back
# fiche USB-C : partie métal + surmoulage 12,3 x 8,3 jusqu'à l'extérieur
comps['fiche USB-C (métal)'] = C.box(-C.MOD_W/2 - 6.6, -C.MOD_W/2, C.USB_Y - 4.15, C.USB_Y + 4.15, C.USB_ZC - 1.25, C.USB_ZC + 1.25)
comps['fiche USB-C (surmoulage)'] = C.extrude_z(C.rrect(12.3, 8.3, 2.0), 0, 20).transform(__import__('numpy').array([[0,0,-1,-C.MOD_W/2-6.6],[1,0,0,C.USB_Y],[0,1,0,C.USB_ZC]],float)) if True else C.box(-C.W/2 - 20, -C.MOD_W/2 - 6.6, C.USB_Y - 6.15, C.USB_Y + 6.15, C.USB_ZC - 4.15, C.USB_ZC + 4.15)
# batterie 10 000 mAh (110 x 62 x 12,5) debout contre la paroi arrière
zr = -C.TOTAL_D + C.WALL
comps['batterie 10 000 mAh'] = sheared(C.box(-C.BAT_L/2, C.BAT_L/2, C.BAT_Y0, C.BAT_Y0 + C.BAT_H, zr + 0.3, zr + 0.3 + C.BAT_T))
# haut-parleur Ø40 x 6 contre la paroi droite
spk = Manifold.cylinder(6.0, C.SPK_D/2, C.SPK_D/2, 96)
spk = spk.transform(np.array([[0,0,-1, C.W/2 - C.WALL - 0.05],[1,0,0,C.SPK_Y],[0,1,0,C.SPK_Z]], float))
comps['haut-parleur Ø40'] = sheared(spk)
# carte annexe type Arduino Nano (45 x 18 x 8) posée au fond
comps['carte annexe 45x18x8'] = sheared(C.box(-45, 0, -C.H/2 + C.WALL + 0.2, -C.H/2 + C.WALL + 8.2, -52, -34))
tot = 0
for k, m in comps.items():
    v = (m ^ case).volume()
    tot += m.volume()
    print(f"  {k:36s} volume {m.volume()/1000:6.2f} cm3   interférence {v:8.3f} mm3")
# les composants ne se chevauchent pas entre eux
names = list(comps)
for i in range(len(names)):
    for j in range(i + 1, len(names)):
        v = (comps[names[i]] ^ comps[names[j]]).volume()
        if v > 0.01 and not ('module' in names[i] and 'module' in names[j]) and not ('USB' in names[i] and 'USB' in names[j]):
            print(f"  ⚠ chevauchement {names[i]} / {names[j]} : {v:.1f} mm3")
