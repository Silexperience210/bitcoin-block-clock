# -*- coding: utf-8 -*-
import numpy as np
import trimesh
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

m = trimesh.load('boitier_bitcoinclock.stl')
light = np.array([0.5, -1.0, 0.8])
light /= np.linalg.norm(light)

views = [
    ('face',  (0, 0, 1), (0, 1, 0)),
    ('arriere', (0, 0, -1), (0, 1, 0)),
    ('gauche', (-1, 0, 0), (0, 0, 1)),
    ('droite', (1, 0, 0), (0, 0, 1)),
    ('haut', (0, 1, 0), (0, 0, 1)),
    ('iso', (1, -1, 1), (0, 0, 1)),
]

fig = plt.figure(figsize=(18, 12))
for idx, (name, azim, elev) in enumerate(views):
    ax = fig.add_subplot(2, 3, idx + 1, projection='3d')
    verts = m.vertices[m.faces]
    normals = np.cross(verts[:, 1] - verts[:, 0], verts[:, 2] - verts[:, 0])
    nlen = np.linalg.norm(normals, axis=1, keepdims=True)
    nlen[nlen == 0] = 1
    normals /= nlen
    shades = np.clip((normals @ light) * 0.4 + 0.5, 0.2, 0.9)
    colors = np.column_stack([shades * 0.95, shades * 0.95, shades])
    coll = Poly3DCollection(verts, facecolors=colors, edgecolors='k', lw=0.05)
    ax.add_collection3d(coll)
    lim = m.bounds
    for axis in (ax.xaxis, ax.yaxis, ax.zaxis):
        axis.set_tick_params(labelbottom=False, labelleft=False)
    ax.set_xlim(lim[0, 0], lim[1, 0])
    ax.set_ylim(lim[0, 1], lim[1, 1])
    ax.set_zlim(lim[0, 2], lim[1, 2])
    ax.view_init(elev=np.degrees(np.arccos(elev[2])), azim=np.degrees(np.arctan2(azim[1], azim[0])))
    ax.set_title(name)

plt.tight_layout()
plt.savefig('preview.png', dpi=120)
print('preview.png')
