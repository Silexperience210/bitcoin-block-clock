# SPDX-License-Identifier: Apache-2.0
# Copyright 2025-2026 silexperience — https://github.com/Silexperience210/bitcoin-block-clock
import sys, os, math, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_case_v3 as C, render_preview as RP
from manifold3d import Manifold
face = C.build_face(); body = C.build_body()
Vf, Ff = RP.mesh_of(face); Vb, Fb = RP.mesh_of(body)
Vs, Fs, tex = RP.screen_quad(os.path.join(os.path.dirname(__file__), '..', 'images', 'screens-v5', 'page0_prix.png'))
G, O = (52, 54, 60), (236, 132, 28)
CEN = np.array([0, 8.0, -43.0])          # centre approximatif de l'objet
def c(V, d=(0, 0, 0)): return V - CEN + np.array(d)
RP.render([(c(Vf), Ff, G, 'm'), (c(Vb), Fb, O, 'm'), (c(Vs), Fs, None, 'screen')], yaw=-30, pitch=12, out='/tmp/f_hero.png', screen_tex=tex, dist=400)
RP.render([(c(Vf), Ff, G, 'm'), (c(Vb), Fb, O, 'm')], yaw=150, pitch=18, out='/tmp/f_rear.png', dist=400)
# coupe : on retire la moitié gauche du boîtier, composants en place
cut = C.box(0.0, 100, -100, 100, -200, 50)
fc, bc = face ^ cut, body ^ cut
S = C.SHEAR
zr = -C.TOTAL_D + C.WALL
bat = C.box(-C.BAT_L/2, C.BAT_L/2, C.BAT_Y0, C.BAT_Y0 + C.BAT_H, zr + 0.3, zr + 0.3 + C.BAT_T).transform(S) ^ cut
spk = Manifold.cylinder(6.0, C.SPK_D/2, C.SPK_D/2 * 0.8, 96).transform(np.array([[0,0,-1, C.W/2 - C.WALL - 0.05],[1,0,0,C.SPK_Y],[0,1,0,C.SPK_Z]], float)).transform(S)
mod_front = C.extrude_z(C.rrect(C.MOD_W, C.MOD_H, C.MOD_R), 0.0, -2.6) ^ cut
pcb = C.extrude_z(C.rrect(C.MOD_W - 0.5, C.MOD_H - 0.5, C.MOD_R), -2.6, -C.MOD_T) ^ cut
esp = C.box(5, 30, -12, 6, -C.MOD_T - 3.2, -C.MOD_T)
ard = C.box(2, 40, -C.H/2 + C.WALL + 0.2, -C.H/2 + C.WALL + 2.0, -52, -34).transform(S)
objs = [(c(RP.mesh_of(fc)[0]), RP.mesh_of(fc)[1], G, 'm'), (c(RP.mesh_of(bc)[0]), RP.mesh_of(bc)[1], O, 'm'),
        (c(RP.mesh_of(bat)[0]), RP.mesh_of(bat)[1], (70, 110, 170), 'm'), (c(RP.mesh_of(spk)[0]), RP.mesh_of(spk)[1], (30, 30, 34), 'm'),
        (c(RP.mesh_of(mod_front)[0]), RP.mesh_of(mod_front)[1], (14, 14, 16), 'm'), (c(RP.mesh_of(pcb)[0]), RP.mesh_of(pcb)[1], (30, 110, 60), 'm'),
        (c(RP.mesh_of(esp)[0]), RP.mesh_of(esp)[1], (180, 180, 186), 'm'), (c(RP.mesh_of(ard)[0]), RP.mesh_of(ard)[1], (20, 120, 140), 'm')]
RP.render(objs, yaw=62, pitch=22, out='/tmp/f_cut.png', dist=380)
# plateau d'impression : les deux pièces dans leur orientation d'impression
import trimesh
D = os.path.dirname(os.path.abspath(__file__)); tf = trimesh.load(os.path.join(D, 'bbc_v3_face.stl')); tb = trimesh.load(os.path.join(D, 'bbc_v3_corps.stl'))
Vp1 = np.asarray(tf.vertices) + [-70, 0, 0]; Vp2 = np.asarray(tb.vertices) + [70, 0, 0]
def to_case(V):   # plateau (z haut) -> repère de rendu : y haut
    return np.column_stack([V[:, 0], V[:, 2], -V[:, 1]])
bed = C.box(-150, 150, -115, 115, -1.0, 0.0)
Vbed, Fbed = RP.mesh_of(bed)
RP.C.TILT = 0.0
RP.render([(to_case(Vbed), Fbed, (40, 44, 50), 'm'), (to_case(Vp1), np.asarray(tf.faces), G, 'm'), (to_case(Vp2), np.asarray(tb.faces), O, 'm')],
          yaw=-20, pitch=38, out='/tmp/f_bed.png', dist=560)
print('ok')
