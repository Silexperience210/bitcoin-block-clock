# Rendu d'aperçu du boîtier V3 (rasteriseur numpy, sans OpenGL)
import sys, os, math, numpy as np, trimesh
from PIL import Image, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_case_v3 as C

def rot_x(a):
    c, s = math.cos(a), math.sin(a); return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
def rot_y(a):
    c, s = math.cos(a), math.sin(a); return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])

def mesh_of(m):
    mm = m.to_mesh()
    return np.asarray(mm.vert_properties)[:, :3].astype(np.float64), np.asarray(mm.tri_verts).astype(np.int64)

def render(objs, yaw, pitch, out, size=(1600, 1150), dist=420.0, fov=28.0, screen_tex=None, bg=(20, 22, 26)):
    Wd, Hd = size
    img = np.zeros((Hd, Wd, 3), np.float32); img[:] = np.array(bg, np.float32) / 255
    # fond dégradé
    yy = np.linspace(0, 1, Hd)[:, None]
    img[:] = (np.array([34, 36, 42]) * (1 - yy) + np.array([12, 13, 16]) * yy)[:, None, :].reshape(Hd, 1, 3) / 255
    zbuf = np.full((Hd, Wd), np.inf, np.float32)
    Rw = rot_x(math.radians(-C.TILT))                  # posé sur la table (dessous horizontal)
    Rc = rot_x(math.radians(pitch)) @ rot_y(math.radians(yaw))
    f = (Hd / 2) / math.tan(math.radians(fov / 2))
    light = np.array([-0.45, 0.75, 0.55]); light /= np.linalg.norm(light)
    fill = np.array([0.6, 0.2, -0.4]); fill /= np.linalg.norm(fill)
    for (V, F, col, kind) in objs:
        P = (Rc @ (Rw @ V.T)).T
        P[:, 2] -= dist
        tri = P[F]
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        nl = np.linalg.norm(n, axis=1); ok = nl > 1e-12
        n[ok] /= nl[ok, None]
        # projection
        z = -tri[:, :, 2]
        sx = Wd / 2 + f * tri[:, :, 0] / z
        sy = Hd / 2 - f * tri[:, :, 1] / z
        vis = (n[:, 2] > 0) & ok if kind != 'screen' else ok
        view = np.array([0, 0, 1.0])
        diff = np.clip(n @ light, 0, 1) * 0.75 + np.clip(n @ fill, 0, 1) * 0.18 + 0.22
        h = light + view; h /= np.linalg.norm(h)
        spec = np.clip(n @ h, 0, 1) ** 40 * (0.45 if kind != 'screen' else 0.25)
        base = np.array(col, np.float32) / 255
        for t in np.where(vis)[0]:
            x0, x1 = int(max(0, math.floor(sx[t].min()))), int(min(Wd - 1, math.ceil(sx[t].max())))
            y0, y1 = int(max(0, math.floor(sy[t].min()))), int(min(Hd - 1, math.ceil(sy[t].max())))
            if x1 < x0 or y1 < y0: continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            (ax, bx, cx), (ay, by, cy) = sx[t], sy[t]
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9: continue
            w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
            w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
            w2 = 1 - w0 - w1
            m = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
            if not m.any(): continue
            zz = w0 * z[t, 0] + w1 * z[t, 1] + w2 * z[t, 2]
            sub = zbuf[y0:y1 + 1, x0:x1 + 1]
            m &= zz < sub
            if not m.any(): continue
            sub[m] = zz[m]
            if kind == 'screen' and screen_tex is not None:
                uv = screen_tex['uv'][F[t]]
                u = w0 * uv[0, 0] + w1 * uv[1, 0] + w2 * uv[2, 0]
                v = w0 * uv[0, 1] + w1 * uv[1, 1] + w2 * uv[2, 1]
                tx = screen_tex['img']
                iu = np.clip((u * (tx.shape[1] - 1)).astype(int), 0, tx.shape[1] - 1)
                iv = np.clip((v * (tx.shape[0] - 1)).astype(int), 0, tx.shape[0] - 1)
                c = tx[iv, iu] * 0.92 + spec[t] * 0.35
                img[y0:y1 + 1, x0:x1 + 1][m] = np.clip(c[m], 0, 1)
            else:
                c = base * diff[t] + spec[t]
                img[y0:y1 + 1, x0:x1 + 1][m] = np.clip(c, 0, 1)
    im = Image.fromarray((img * 255).astype(np.uint8))
    im = im.resize((Wd // 2 * 2 // 2, Hd // 2 * 2 // 2), Image.LANCZOS) if False else im
    im.save(out)
    return im

def screen_quad(frame_png):
    """écran à fleur : quad z = 0 (légèrement devant) aux dimensions du module."""
    w, h = C.MOD_W, C.MOD_H
    V = np.array([[-w / 2, -h / 2, 0.02], [w / 2, -h / 2, 0.02], [w / 2, h / 2, 0.02], [-w / 2, h / 2, 0.02]], float)
    F = np.array([[0, 1, 2], [0, 2, 3]])
    tex = np.asarray(Image.open(frame_png).convert('RGB'), np.float32) / 255
    # zone active 73,4 x 49 centrée dans un verre noir
    hh, ww = 620, 945
    glass = np.zeros((hh, ww, 3), np.float32) + 0.02
    aw, ah = int(ww * 73.4 / w), int(hh * 49.0 / h)
    act = np.asarray(Image.fromarray((tex * 255).astype(np.uint8)).resize((aw, ah), Image.LANCZOS), np.float32) / 255
    oy, ox = (hh - ah) // 2, (ww - aw) // 2
    glass[oy:oy + ah, ox:ox + aw] = act
    uv = np.array([[0, 1], [1, 1], [1, 0], [0, 0]], float)
    return V, F, {'img': glass, 'uv': uv}

if __name__ == '__main__':
    face = C.build_face(); body = C.build_body()
    Vf, Ff = mesh_of(face); Vb, Fb = mesh_of(body)
    Vs, Fs, tex = screen_quad(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'images', 'screens-v5', 'page1_onchain.png'))
    GRAPHITE, ORANGE = (58, 60, 66), (236, 132, 28)
    # vue héros 3/4 avant
    render([(Vf, Ff, GRAPHITE, 'm'), (Vb, Fb, ORANGE, 'm'), (Vs, Fs, None, 'screen')], yaw=-32, pitch=14,
           out='/tmp/v3_front.png', screen_tex=tex)
    # vue 3/4 arrière
    render([(Vf, Ff, GRAPHITE, 'm'), (Vb, Fb, ORANGE, 'm')], yaw=145, pitch=16, out='/tmp/v3_rear.png')
    # vue éclatée (façade avancée de 40 mm)
    Vf2 = Vf + np.array([0, 0, 45.0]); Vs2 = Vs + np.array([0, 0, 45.0])
    render([(Vf2, Ff, GRAPHITE, 'm'), (Vb, Fb, ORANGE, 'm'), (Vs2, Fs, None, 'screen')], yaw=-58, pitch=18,
           out='/tmp/v3_exploded.png', screen_tex=tex, dist=520)
    # côté droit (grille)
    render([(Vf, Ff, GRAPHITE, 'm'), (Vb, Fb, ORANGE, 'm')], yaw=-100, pitch=6, out='/tmp/v3_side.png')
    print("ok")
