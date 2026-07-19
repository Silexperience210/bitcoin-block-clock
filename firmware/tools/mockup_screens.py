# -*- coding: utf-8 -*-
"""
Génère des mockups fidèles des 9 pages du firmware bitcoin-block-clock V4
(+ flash NEW BLOCK) à partir des fonctions de dessin réelles du .ino
(mêmes couleurs RGB565, mêmes coordonnées, même logique).

Sortie : images/screens/page*.png (480x320 natif rendu x2 = 960x640)
Usage  : python firmware/tools/mockup_screens.py   (depuis la racine du repo)
"""
import math
import os
import random
from PIL import Image, ImageDraw, ImageFont

S = 2  # facteur d'échelle (rendu 960x640 pour des captures nettes)
SCR_W, SCR_H = 480, 320
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "images", "screens")

# ---------------------------------------------------------------- palette
def rgb565(c):
    r = (c >> 11) & 31
    g = (c >> 5) & 63
    b = c & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))

C_BG      = rgb565(0x0841)
C_PANEL   = rgb565(0x10A2)
C_LINE    = rgb565(0x2965)
C_ORANGE  = rgb565(0xFBE0)
C_ORANGE_D = rgb565(0x8A60)
C_WHITE   = rgb565(0xFFFF)
C_GREY    = rgb565(0x8C51)
C_DGREY   = rgb565(0x4A49)
C_GREEN   = rgb565(0x2E68)
C_RED     = rgb565(0xD186)
C_GREEN_D = rgb565(0x03E0)
C_RED_D   = rgb565(0x8800)
C_YELLOW  = rgb565(0xFE60)
C_BLUE    = rgb565(0x2A7F)

FONT      = "C:/Windows/Fonts/arial.ttf"
FONT_BOLD = "C:/Windows/Fonts/arialbd.ttf"

def _font(px, bold=False):
    return ImageFont.truetype(FONT_BOLD if bold else FONT, max(6, int(px)))

def _tpx(size):
    """Taille de police pour un setTextSize(size) Arduino (cellule 6x8*size)."""
    return 11 * size * S

def _smooth_font():
    """Chiffres lissés : Arial Bold ~46 px (cf. gen_smooth_font.py)."""
    return _font(46 * S, bold=True)

# ---------------------------------------------------------------- wrappers
class GFX:
    def __init__(self):
        self.im = Image.new("RGB", (SCR_W * S, SCR_H * S), C_BG)
        self.d = ImageDraw.Draw(self.im)

    def _xy(self, *v):
        return [int(round(x * S)) for x in v]

    def fillRect(self, x, y, w, h, c):
        self.d.rectangle(self._xy(x, y, x + w - 1, y + h - 1), fill=c)

    def drawRect(self, x, y, w, h, c):
        self.d.rectangle(self._xy(x, y, x + w - 1, y + h - 1), outline=c, width=max(1, S - 1))

    def fillRoundRect(self, x, y, w, h, r, c):
        self.d.rounded_rectangle(self._xy(x, y, x + w - 1, y + h - 1), radius=int(r * S), fill=c)

    def drawRoundRect(self, x, y, w, h, r, c):
        self.d.rounded_rectangle(self._xy(x, y, x + w - 1, y + h - 1), radius=int(r * S),
                                 outline=c, width=max(1, S - 1))

    def fillCircle(self, x, y, r, c):
        self.d.ellipse(self._xy(x - r, y - r, x + r, y + r), fill=c)

    def drawCircle(self, x, y, r, c):
        self.d.ellipse(self._xy(x - r, y - r, x + r, y + r), outline=c, width=max(1, S - 1))

    def fillTriangle(self, x0, y0, x1, y1, x2, y2, c):
        self.d.polygon(self._xy(x0, y0, x1, y1, x2, y2), fill=c)

    def drawLine(self, x0, y0, x1, y1, c, w=1):
        self.d.line(self._xy(x0, y0, x1, y1), fill=c, width=max(1, int(w * S / 2)))

    def drawPixel(self, x, y, c):
        self.d.point(self._xy(x, y), fill=c)

    def drawFastHLine(self, x, y, w, c):
        self.fillRect(x, y, w, 1, c)

    def drawFastVLine(self, x, y, h, c):
        self.fillRect(x, y, 1, h, c)

    def text(self, x, y, s, size, color, bold=False):
        f = _font(_tpx(size), bold)
        self.d.text(self._xy(x, y), s, font=f, fill=color, anchor="la")

    def textCenter(self, s, y, size, color):
        tw = len(s) * 6 * size                     # métrique firmware (6*size/char)
        self.text((SCR_W - tw) / 2, y, s, size, color)

    def smooth(self, x, y, s, fg):
        f = _smooth_font()
        self.d.text(self._xy(x, y), s, font=f, fill=fg, anchor="la")
        return self.textlength(f, s) / S

    def textlength(self, f, s):
        return self.d.textlength(s, font=f)

    def fillScreen(self, c):
        self.d.rectangle([0, 0, SCR_W * S, SCR_H * S], fill=c)

# ---------------------------------------------------------------- helpers
def drawPill(g, x, y, w, h, bg, fg, txt, size=1):
    g.fillRoundRect(x, y, w, h, h // 2, bg)
    tw = len(txt) * 6 * size
    g.text(x + (w - tw) // 2, y + (h - 8 * size) // 2, txt, size, fg)

def drawArrow(g, x, y, up, color):
    if up:
        g.fillTriangle(x, y + 8, x + 5, y, x + 10, y + 8, color)
    else:
        g.fillTriangle(x, y, x + 5, y + 8, x + 10, y, color)

def prettyNum(v):
    return f"{int(v):,}".replace(",", " ")

# ---------------------------------------------------------------- header/footer
PG = {"PRICE": 0, "CHAIN": 1, "CUBE": 2, "POOLS": 3, "LN": 4, "NODE": 5, "AI": 6, "SIG": 7, "DOOM": 8}
NAV_Y = SCR_H - 26
TAB_W = SCR_W // 9

def drawTabIcon(g, i, cx, cy, c):
    if i == 0:
        g.drawLine(cx - 8, cy + 5, cx - 3, cy, c); g.drawLine(cx - 3, cy, cx + 1, cy + 3, c)
        g.drawLine(cx + 1, cy + 3, cx + 8, cy - 6, c)
    elif i == 1:
        g.drawRect(cx - 7, cy - 3, 10, 10, c)
        g.drawLine(cx - 7, cy - 3, cx - 3, cy - 7, c); g.drawLine(cx + 3, cy - 3, cx + 7, cy - 7, c)
        g.drawLine(cx - 3, cy - 7, cx + 7, cy - 7, c); g.drawLine(cx + 7, cy - 7, cx + 7, cy + 3, c)
        g.drawLine(cx + 3, cy + 7, cx + 7, cy + 3, c)
    elif i == 2:
        g.drawRect(cx - 6, cy - 7, 12, 14, c); g.fillRect(cx - 6, cy + 1, 12, 6, c)
    elif i == 3:
        g.fillRect(cx - 9, cy - 1, 5, 8, c); g.fillRect(cx - 2, cy - 6, 5, 13, c)
        g.fillRect(cx + 5, cy + 2, 5, 5, c)
    elif i == 4:
        g.fillTriangle(cx + 2, cy - 7, cx - 5, cy + 1, cx, cy + 1, c)
        g.fillTriangle(cx + 2, cy - 7, cx, cy + 1, cx + 5, cy - 1, c)
        g.fillTriangle(cx - 2, cy + 7, cx, cy - 1, cx + 5, cy - 1, c)
    elif i == 5:
        a = 0.0
        while a < math.pi:
            g.drawPixel(cx + int(math.cos(a) * 7), cy + 4 - int(math.sin(a) * 7), c)
            a += 0.28
        g.drawLine(cx, cy + 4, cx + 4, cy - 1, c)
    elif i == 6:
        g.drawLine(cx - 5, cy + 4, cx, cy - 5, c); g.drawLine(cx, cy - 5, cx + 5, cy + 4, c)
        g.drawLine(cx - 5, cy + 4, cx + 5, cy + 4, c)
        g.fillCircle(cx - 5, cy + 4, 2, c); g.fillCircle(cx + 5, cy + 4, 2, c)
        g.fillCircle(cx, cy - 5, 2, c)
    elif i == 7:
        g.drawLine(cx - 8, cy + 4, cx + 6, cy - 4, c)
        g.fillTriangle(cx + 6, cy - 4, cx + 1, cy - 5, cx + 5, cy, c)
        g.drawLine(cx - 8, cy - 4, cx + 6, cy + 4, c)
        g.fillTriangle(cx + 6, cy + 4, cx + 5, cy - 1, cx + 1, cy + 5, c)
    elif i == 8:
        g.drawCircle(cx, cy, 6, c); g.fillCircle(cx, cy, 2, c)

def drawHeader(g):
    g.text(10, 8, "21:37", 2, C_WHITE)
    g.text(78, 12, "19/07", 1, C_DGREY)
    # wifi : dot 1 = état (vert), dots 2-3 = force du signal
    g.fillCircle(384, 13, 3, C_GREEN)
    g.fillCircle(393, 13, 3, C_GREEN)
    g.fillCircle(402, 13, 3, C_GREEN)
    # batterie : icône avec % intégré
    bps = "80%"
    g.drawRect(444, 7, 28, 12, C_GREY)
    g.fillRect(472, 10, 3, 6, C_GREY)
    g.fillRect(446, 9, int(24 * 0.80), 8, C_GREEN)
    g.text(444 + (28 - len(bps) * 6) // 2, 9, bps, 1, C_WHITE)
    g.drawFastHLine(0, 26, SCR_W, C_LINE)

def drawFooter(g, page):
    g.drawFastHLine(0, NAV_Y, SCR_W, C_LINE)
    for i in range(9):
        cx = i * TAB_W + TAB_W // 2
        act = (i == page)
        drawTabIcon(g, i, cx, SCR_H - 14, C_ORANGE if act else C_DGREY)
        if act:
            g.fillRect(i * TAB_W + TAB_W // 2 - 16, SCR_H - 3, 32, 3, C_ORANGE)

# ---------------------------------------------------------------- données d'exemple
BLOCK = 912345
MEMPOOL = 12450
FEES = (12, 8, 5, 2)
PRICE = 97432
random.seed(21)

# ---------------------------------------------------------------- page 0
def page_price():
    g = GFX()
    drawHeader(g)
    # logo BTC (asset du firmware, magenta = transparent)
    try:
        logo = Image.open(os.path.join(os.path.dirname(__file__), "..",
                                       "bitcoin-block-clock", "btc_logo_src.png")).convert("RGBA")
        px = logo.load()
        for yy in range(logo.height):
            for xx in range(logo.width):
                r, gg, b, a = px[xx, yy]
                if r > 200 and gg < 90 and b > 200:
                    px[xx, yy] = (0, 0, 0, 0)
        logo = logo.resize((64 * S, 64 * S), Image.LANCZOS)
        g.im.paste(logo, (int(52 * S), int(34 * S)), logo)
    except FileNotFoundError:
        g.fillCircle(52 + 32, 34 + 32, 32, C_ORANGE)
        g.text(52 + 24, 34 + 18, "B", 3, C_WHITE, bold=True)
    g.text(52, 104, "BTC / EUR", 1, C_GREY)
    g.smooth(10, 118, prettyNum(PRICE), C_WHITE)
    drawPill(g, 10, 166, 96, 20, C_GREEN_D, C_GREEN, "+1.85%", 1)
    drawArrow(g, 114, 172, True, C_GREEN)
    drawPill(g, 10, 194, 70, 22, C_PANEL, C_ORANGE, "EUR", 2)
    g.text(86, 200, "< tap", 1, C_DGREY)
    g.text(10, 226, "alerte > 100 000", 1, C_DGREY)
    g.text(10, 240, "alerte < 90 000", 1, C_DGREY)
    g.text(10, 262, "baleine: 312 BTC", 1, C_ORANGE)
    TF = ["1H", "24H", "7J", "30J"]
    GX, GY, GW, GH = 186, 66, 284, 190
    for i, lbl in enumerate(TF):
        act = (i == 1)
        drawPill(g, GX + i * 72, 34, 66, 22, C_ORANGE if act else C_PANEL,
                 C_BG if act else C_GREY, lbl, 2)
    # ---- graphe
    g.fillRoundRect(GX - 6, GY - 8, GW + 12, GH + 40, 8, C_PANEL)
    n = 90
    closes = [95500.0]
    for i in range(1, n):
        closes.append(closes[-1] * (1 + random.gauss(0.0006, 0.0035)))
    off = float(PRICE) - closes[-1]          # termine au prix, sans cassure
    closes = [c + off for c in closes]
    mn, mx = min(closes), max(closes)
    rng = max(mx - mn, 0.01)
    yOf = lambda v: GY + GH - 6 - int((v - mn) / rng * (GH - 14))
    xOf = lambda i: GX + 4 + int(i * (GW - 10) / (n - 1))
    yOpen = yOf(closes[0])
    for x in range(GX + 4, GX + GW - 6, 6):
        g.fillRect(x, yOpen, 3, 1, C_DGREY)
    base = GY + GH - 4
    for i in range(n - 1):
        x0, x1 = xOf(i), xOf(i + 1)
        y0, y1 = yOf(closes[i]), yOf(closes[i + 1])
        for x in range(x0, x1 + 1, 2):
            y = y0 + int((y1 - y0) * (x - x0) / max(1, x1 - x0))
            h = base - y
            if h <= 0:
                continue
            h1 = h // 3
            if h1 > 0:
                g.drawFastVLine(x, y, h1, C_ORANGE_D)
            if h - h1 > 0:
                g.drawFastVLine(x, y + h1, h - h1, C_PANEL)
    up = closes[-1] >= closes[0]
    lc = C_GREEN if up else C_RED
    for i in range(n - 1):
        g.drawLine(xOf(i), yOf(closes[i]), xOf(i + 1), yOf(closes[i + 1]), lc)
        g.drawLine(xOf(i), yOf(closes[i]) + 1, xOf(i + 1), yOf(closes[i + 1]) + 1, lc)
    g.fillCircle(xOf(n - 1), yOf(closes[-1]), 4, lc)
    g.fillCircle(xOf(n - 1), yOf(closes[-1]), 2, C_WHITE)
    g.text(GX + 4, GY + 2, prettyNum(mx), 1, C_GREY)
    g.text(GX + 4, GY + GH - 10, prettyNum(mn), 1, C_GREY)
    drawFooter(g, 0)
    return g

# ---------------------------------------------------------------- page 1
def page_chain():
    g = GFX()
    drawHeader(g)
    g.text(12, 36, "BLOCK HEIGHT", 1, C_ORANGE)
    g.smooth(10, 50, str(BLOCK), C_WHITE)
    g.text(12, 96, "il y a 3m42s  ·  Foundry USA  ·  3122 tx", 1, C_GREY)
    g.fillRoundRect(10, 108, 280, 8, 4, C_PANEL)
    g.fillRoundRect(10, 108, 104, 8, 4, C_ORANGE)
    g.fillRoundRect(306, 34, 164, 84, 8, C_PANEL)
    g.text(316, 42, "MEMPOOL", 1, C_GREY)
    g.text(316, 56, prettyNum(MEMPOOL), 3, C_WHITE)
    g.text(316, 82, "TX en attente", 1, C_GREY)
    g.text(316, 98, f"halving ~{(1050000 - BLOCK) * 10 // 1440} j", 1, C_GREY)
    g.text(12, 132, "FEES sat/vB", 1, C_GREY)
    fl = ("rapide", "30min", "1h", "eco")
    for i in range(4):
        fx = 10 + i * 78
        g.fillRoundRect(fx, 144, 72, 36, 6, C_PANEL)
        g.text(fx + 8, 148, str(FEES[i]), 2, C_ORANGE if i == 0 else C_WHITE)
        g.text(fx + 8, 166, fl[i], 1, C_DGREY)
    g.text(12, 196, "ajustement difficulte : 1234 blocs", 1, C_GREY)
    g.text(12, 210, "estimation +2.34 %", 1, C_GREEN)
    g.fillRoundRect(10, 234, 460, 56, 8, C_PANEL)
    g.text(20, 242, "WHALE WATCH (mempool)", 1, C_ORANGE)
    g.text(20, 258, "312.4 BTC en transit !", 2, C_WHITE)
    drawFooter(g, 1)
    return g

# ---------------------------------------------------------------- page 2 (cube)
def page_cube():
    g = GFX()
    drawHeader(g)
    CX, CY, H, ZH = 240, 214, 40, 80

    def proj(x, y, z):
        return (int(CX + (x - y) * 0.866), int(CY + (x + y) * 0.433 - z))

    V = [(-H, -H, 0), (H, -H, 0), (H, H, 0), (-H, H, 0),
         (-H, -H, ZH), (H, -H, ZH), (H, H, ZH), (-H, H, ZH)]
    E = [(0, 1), (1, 2), (2, 3), (3, 0), (4, 5), (5, 6), (6, 7), (7, 4),
         (0, 4), (1, 5), (2, 6), (3, 7)]
    for e0, e1 in E:
        for s2 in range(0, 17, 2):
            t = s2 / 16.0
            px, py = proj(V[e0][0] + (V[e1][0] - V[e0][0]) * t,
                          V[e0][1] + (V[e1][1] - V[e0][1]) * t,
                          V[e0][2] + (V[e1][2] - V[e0][2]) * t)
            g.drawPixel(px, py, C_ORANGE)

    def slot(k):
        gx, gy, gz = k % 6, (k // 6) % 6, k // 36
        return -33 + gx * 13.2, -33 + gy * 13.2, 6 + gz * 13.2

    for k in range(110):                      # particules empilées (~55 %)
        x, y, z = slot(k)
        px, py = proj(x, y, z)
        g.fillRect(px, py, 2, 2, C_YELLOW if k % 5 == 0 else C_ORANGE)
    for _ in range(8):                        # quelques-unes en chute
        px = CX + random.randint(-90, 90)
        py = random.randint(28, 110)
        g.fillRect(px, py, 2, 2, C_ORANGE)
    # HUD
    g.text(10, 34, "MEMPOOL", 1, C_GREY)
    g.text(10, 46, prettyNum(MEMPOOL), 2, C_WHITE)
    g.text(SCR_W - 70, 34, "FEE", 1, C_GREY)
    g.text(SCR_W - 70, 46, str(FEES[0]), 2, C_ORANGE)
    g.text(10, 282, f"bloc {BLOCK}", 1, C_GREY)
    g.text(SCR_W - 130, 282, "Foundry USA", 1, C_GREY)
    drawFooter(g, 2)
    return g

# ---------------------------------------------------------------- page 3
def page_pools():
    g = GFX()
    drawHeader(g)
    g.text(12, 36, "POOLS WAR — blocs mines sur 7 jours", 1, C_ORANGE)
    g.text(360, 36, "total 996", 1, C_GREY)
    POOL_COL = [C_ORANGE, C_GREEN, C_YELLOW, C_BLUE, C_RED, C_GREY]
    pools = [("Foundry USA", 312), ("AntPool", 187), ("ViaBTC", 141),
             ("F2Pool", 108), ("MARA Pool", 86), ("SpiderPool", 62)]
    total, mx = 996, 312
    for i, (nm, bl) in enumerate(pools):
        y = 62 + i * 38
        w = int(268.0 * bl / mx)
        g.text(10, y + 6, nm[:12], 1, C_ORANGE if i == 0 else C_WHITE)
        g.fillRoundRect(140, y, 272, 20, 5, C_PANEL)
        if w > 8:
            g.fillRoundRect(140, y, w, 20, 5, POOL_COL[i])
        if i == 0:
            g.fillTriangle(146, y - 2, 152, y - 8, 158, y - 2, C_ORANGE)
        g.text(420, y + 6, f"{bl} {bl * 100 // total}%", 1, C_GREY)
    drawFooter(g, 3)
    return g

# ---------------------------------------------------------------- page 4
def page_ln():
    g = GFX()
    drawHeader(g)
    g.text(12, 36, "LIGHTNING NETWORK", 1, C_ORANGE)
    cw = g.smooth(24, 56, "5 123", C_WHITE)
    g.text(24 + cw + 8, 74, "BTC", 2, C_GREY)
    g.text(26, 100, "capacite totale du reseau", 1, C_DGREY)
    bolt = [(18, 0), (40, 0), (24, 29), (35, 29), (5, 71), (13, 37), (3, 37)]
    bx, by = 330, 44
    px = [bx + p[0] for p in bolt]
    py = [by + p[1] for p in bolt]
    for i in range(1, 6):
        g.fillTriangle(px[0] + 3, py[0] + 3, px[i] + 3, py[i] + 3, px[i + 1] + 3, py[i + 1] + 3, C_ORANGE_D)
    for i in range(1, 6):
        g.fillTriangle(px[0], py[0], px[i], py[i], px[i + 1], py[i + 1], C_YELLOW)
    for i in range(7):
        j = (i + 1) % 7
        g.drawLine(px[i], py[i], px[j], py[j], C_ORANGE)
    g.drawLine(px[6], py[6], px[0], py[0], C_WHITE)
    g.drawLine(px[0], py[0], px[1], py[1], C_WHITE)

    def tile(x, y, label, val, unit):
        g.fillRoundRect(x, y, 204, 66, 8, C_PANEL)
        g.text(x + 10, y + 10, label, 1, C_GREY)
        g.text(x + 10, y + 30, val, 2, C_WHITE)
        if unit:
            g.text(x + 12 + len(val) * 12, y + 36, unit, 1, C_DGREY)

    tile(24, 132, "CHANNELS", prettyNum(84321), None)
    tile(252, 132, "NODES", prettyNum(16432), None)
    tile(24, 212, "CAPACITE MOYENNE", prettyNum(5950000), "sats")
    tile(252, 212, "FEE RATE MOYEN", "312", "ppm")
    drawFooter(g, 4)
    return g

# ---------------------------------------------------------------- page 5
def drawGauge(g, cx, cy, r, value):
    segC = [C_RED, C_ORANGE, C_YELLOW, C_GREEN, C_GREEN_D]
    r0, r1 = r - 15, r
    step = 0.035
    for seg in range(5):
        a0 = math.pi - seg * math.pi / 5
        a1 = math.pi - (seg + 1) * math.pi / 5
        a = a0
        while a > a1:
            an = max(a - step, a1)
            x0a = cx + int(math.cos(a) * r0); y0a = cy - int(math.sin(a) * r0)
            x1a = cx + int(math.cos(a) * r1); y1a = cy - int(math.sin(a) * r1)
            x0b = cx + int(math.cos(an) * r0); y0b = cy - int(math.sin(an) * r0)
            x1b = cx + int(math.cos(an) * r1); y1b = cy - int(math.sin(an) * r1)
            g.fillTriangle(x0a, y0a, x1a, y1a, x1b, y1b, segC[seg])
            g.fillTriangle(x0a, y0a, x1b, y1b, x0b, y0b, segC[seg])
            a -= step
    ang = math.pi - (value / 100.0) * math.pi
    nx = cx + int(math.cos(ang) * (r - 26)); ny = cy - int(math.sin(ang) * (r - 26))
    g.drawLine(cx, cy, nx, ny, C_WHITE)
    g.drawLine(cx + 1, cy, nx + 1, ny, C_WHITE)
    g.drawLine(cx, cy + 1, nx, ny + 1, C_WHITE)
    g.fillCircle(cx, cy, 5, C_WHITE)
    g.fillCircle(cx, cy, 2, C_BG)

def page_node():
    g = GFX()
    drawHeader(g)
    g.text(20, 36, "FEAR & GREED INDEX", 1, C_GREY)
    drawGauge(g, 110, 170, 92, 72)
    f = _smooth_font()
    w = g.textlength(f, "72") / S
    g.smooth(110 - w / 2, 176, "72", C_WHITE)
    lbl = "GREED"
    g.text(110 - len(lbl) * 3, 224, lbl, 1, C_GREY)
    g.fillRoundRect(236, 34, 234, 120, 8, C_PANEL)
    g.text(248, 44, "MON NOEUD UMBREL", 1, C_ORANGE)
    g.fillCircle(252, 74, 5, C_GREEN)
    g.text(266, 66, "en ligne", 2, C_WHITE)
    g.text(248, 96, "192.168.1.110 :2105", 1, C_GREY)
    g.text(248, 112, "Bitcoin Knots", 1, C_GREY)
    g.text(248, 132, "detail sync: bientot", 1, C_DGREY)
    g.fillRoundRect(236, 164, 234, 126, 8, C_PANEL)
    g.text(248, 174, "RESEAU", 1, C_ORANGE)
    g.text(248, 192, f"bloc : {BLOCK}", 1, C_GREY)
    g.text(248, 208, f"mempool : {MEMPOOL} TX", 1, C_GREY)
    g.text(248, 224, f"fee rapide : {FEES[0]} sat/vB", 1, C_GREY)
    g.text(248, 248, "baleine : 312 BTC", 1, C_ORANGE)
    drawFooter(g, 5)
    return g

# ---------------------------------------------------------------- page 6
def page_ai():
    g = GFX()
    drawHeader(g)
    g.text(12, 36, "IA LOCALE", 1, C_ORANGE)
    g.text(96, 36, "calcul on-device, rien ne sort", 1, C_DGREY)
    # panneau prochain bloc
    g.fillRoundRect(10, 48, 224, 140, 8, C_PANEL)
    g.text(20, 56, "PROCHAIN BLOC", 1, C_GREY)
    ew = g.smooth(20, 66, "5:40", C_GREEN)
    g.text(24 + ew, 92, "min est.", 1, C_DGREY)
    g.fillRoundRect(20, 116, 196, 8, 4, C_BG)
    g.fillRoundRect(20, 116, 72, 8, 4, C_ORANGE)
    g.text(20, 132, "rythme reel ~9 min (6 blocs)", 1, C_GREY)
    g.text(20, 150, "P(bloc) :  1m    5m    10m", 1, C_GREY)
    lam = 540.0
    for i, pm in enumerate((60, 300, 600)):
        pr = 1.0 - math.exp(-pm / lam)
        bw = int(44 * pr)
        g.fillRoundRect(58 + i * 48, 162, 44, 10, 4, C_BG)
        if bw > 5:
            g.fillRoundRect(58 + i * 48, 162, bw, 10, 4, C_GREEN if pr > 0.63 else C_ORANGE)
        g.text(60 + i * 48, 176, f"{int(pr * 100)}%", 1, C_DGREY)
    # panneau fees
    g.fillRoundRect(246, 48, 224, 140, 8, C_PANEL)
    g.text(256, 56, "FEES : TENDANCE", 1, C_GREY)
    drawArrow(g, 262, 80, False, C_GREEN)
    sw2 = g.smooth(282, 66, "-2.3", C_GREEN)
    g.text(286 + sw2, 92, "sat/vB/h", 1, C_DGREY)
    g.text(256, 118, "en baisse : attends un peu", 1, C_GREY)
    g.text(256, 134, "regression sur 16 echantillons", 1, C_DGREY)
    g.text(256, 150, "creneau : -32% vs norme", 1, C_GREEN)
    g.text(256, 164, "creux probable dans ~4h", 1, C_GREY)
    g.text(256, 178, "appris ici : 1 204 echantillons", 1, C_DGREY)
    # panneau cycles 24 h
    g.fillRoundRect(10, 196, 460, 94, 8, C_PANEL)
    g.text(20, 204, "CYCLES DE FEES — 24 h apprises", 1, C_GREY)
    g.text(220, 204, "(barre orange = maintenant)", 1, C_DGREY)
    day = [0.30, 0.25, 0.22, 0.20, 0.22, 0.30, 0.45, 0.60, 0.75, 0.85, 0.90, 0.95,
           1.00, 0.92, 0.80, 0.70, 0.72, 0.80, 0.88, 0.95, 0.90, 0.75, 0.55, 0.40]
    for i, v in enumerate(day):
        bh = int(40 * v) if v > 0 else 2
        g.fillRect(22 + i * 19, 272 - bh, 14, bh, C_ORANGE if i == 21 else C_DGREY)
    g.drawFastHLine(22, 272, 24 * 19 - 5, C_GREY)
    g.text(22, 278, "0h", 1, C_DGREY)
    g.text(22 + 11 * 19, 278, "11h", 1, C_DGREY)
    g.text(22 + 22 * 19, 278, "22h", 1, C_DGREY)
    drawFooter(g, 6)
    return g

# ---------------------------------------------------------------- page 7
def page_sig():
    g = GFX()
    drawHeader(g)
    g.text(12, 36, "SIGNAUX", 1, C_ORANGE)
    g.text(86, 36, "indicateurs, pas des promesses", 1, C_DGREY)
    # divergence
    g.fillRoundRect(10, 48, 296, 116, 8, C_PANEL)
    g.text(20, 56, "TENDANCE 1D vs 1S", 1, C_GREY)
    drawArrow(g, 28, 78, True, C_GREEN)
    g.text(50, 72, "HAUSSIER", 2, C_GREEN)
    for i in range(3):
        g.fillRoundRect(170 + i * 26, 86 - i * 8, 20, 8 + i * 8, 3, C_GREEN)
    g.text(20, 100, "signal FORT", 1, C_GREY)
    g.text(20, 118, "div : +1.82 ecarts", 1, C_GREY)
    g.text(20, 134, "1D +1.2%  1S -0.8%", 1, C_GREY)
    g.text(20, 150, "divergence normalisee (cache 7J)", 1, C_DGREY)
    # squeeze
    g.fillRoundRect(318, 48, 152, 116, 8, C_PANEL)
    g.text(328, 56, "SQUEEZE 30J", 1, C_GREY)
    f = _smooth_font()
    g.smooth(328, 70, "4.2", C_ORANGE)
    g.text(332 + g.textlength(f, "4.2") / S, 96, "% bw", 1, C_DGREY)
    g.fillRoundRect(328, 118, 130, 22, 6, C_ORANGE_D)
    g.text(338, 125, "COMPRESSION", 1, C_ORANGE)
    g.text(328, 148, "ref 7.8%", 1, C_DGREY)
    # indicateur technique
    g.fillRoundRect(10, 172, 460, 64, 8, C_PANEL)
    g.text(20, 180, "INDICATEUR TECHNIQUE", 1, C_GREY)
    g.fillRoundRect(20, 198, 260, 12, 6, C_BG)
    g.fillRoundRect(20, 198, int(260 * 0.58), 12, 6, C_YELLOW)
    g.smooth(300, 184, "58", C_YELLOW)
    g.text(300, 226, "neutre", 1, C_WHITE)
    g.text(20, 216, "RSI14 54  mom7j +1.2%  pos30j 62%", 1, C_GREY)
    # anomalies
    g.fillRoundRect(10, 244, 460, 46, 8, C_PANEL)
    g.text(20, 252, "ANOMALIES", 1, C_GREY)
    g.text(110, 260, "aucune - tout est dans la norme", 1, C_GREEN)
    drawFooter(g, 7)
    return g

# ---------------------------------------------------------------- page 8 (DOOM)
DM_MAP = [
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
    [1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1],
    [1,0,1,1,0,1,1,0,1,0,1,1,1,1,0,1],
    [1,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1],
    [1,0,1,0,1,0,1,1,1,0,1,0,1,1,0,1],
    [1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,1],
    [1,1,1,0,1,1,1,0,1,1,1,0,1,0,1,1],
    [1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,1],
    [1,0,1,1,1,0,1,1,1,0,1,1,1,1,0,1],
    [1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,1],
    [1,0,1,0,1,1,1,0,1,0,1,1,0,1,0,1],
    [1,0,1,0,0,0,0,0,0,0,1,0,0,1,0,1],
    [1,0,1,1,1,1,1,0,1,0,1,0,1,1,0,1],
    [1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1],
    [1,0,1,0,1,1,0,0,0,0,1,1,1,0,0,1],
    [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
]
DOOM_RAYS, DOOM_TOP, DOOM_BOT = 120, 28, SCR_H - 28
DOOM_FOV = math.pi / 3.0

def shade565(c, pct):
    return (int(c[0] * pct / 100), int(c[1] * pct / 100), int(c[2] * pct / 100))

def page_doom():
    g = GFX()
    drawHeader(g)
    dmX, dmY, dmA = 1.5, 1.5, 0.0
    enemies = [(4.5, 1.5, 1), (6.5, 1.5, 1), (10.5, 9.5, 1), (3.5, 13.5, 1), (13.5, 13.5, 1), (14.5, 5.5, 1)]
    rh = DOOM_BOT - DOOM_TOP
    lvl_pct = (100, 76, 56, 38)
    dmShade = [[shade565(C_ORANGE, p) for p in lvl_pct],
               [shade565(C_ORANGE, p * 2 // 3) for p in lvl_pct]]
    dmShadeE = [shade565(C_RED, lvl_pct[i]) for i in range(4)]
    # ciel + sol
    g.fillRect(0, DOOM_TOP, SCR_W, rh // 2, C_BG)
    g.fillRect(0, DOOM_TOP + rh // 2, SCR_W, rh - rh // 2, C_PANEL)
    zbuf = [0.0] * DOOM_RAYS
    for i in range(DOOM_RAYS):
        ra = dmA - DOOM_FOV / 2 + DOOM_FOV * i / DOOM_RAYS
        rdx, rdy = math.cos(ra), math.sin(ra)
        mx, my = int(dmX), int(dmY)
        ddx = abs(1.0 / (rdx if rdx != 0 else 1e-6))
        ddy = abs(1.0 / (rdy if rdy != 0 else 1e-6))
        if rdx < 0: stx, sdx = -1, (dmX - mx) * ddx
        else:       stx, sdx = 1, (mx + 1 - dmX) * ddx
        if rdy < 0: sty, sdy = -1, (dmY - my) * ddy
        else:       sty, sdy = 1, (my + 1 - dmY) * ddy
        side = 0
        for _ in range(32):
            if sdx < sdy: sdx += ddx; mx += stx; side = 0
            else:         sdy += ddy; my += sty; side = 1
            if mx < 0 or my < 0 or mx >= 16 or my >= 16 or DM_MAP[my][mx]:
                break
        dist = (sdx - ddx if side == 0 else sdy - ddy) * math.cos(ra - dmA)
        dist = max(dist, 0.05)
        zbuf[i] = dist
        h = int(rh / dist)
        lvl = 3 if dist > 5 else 2 if dist > 3 else 1 if dist > 1.5 else 0
        y0 = DOOM_TOP + (rh - h) // 2
        g.fillRect(i * 4, max(y0, DOOM_TOP), 4, min(h, rh), dmShade[side][lvl])
    # démons (tri peintre : loin -> près)
    def normAng(a):
        while a > math.pi: a -= 2 * math.pi
        while a < -math.pi: a += 2 * math.pi
        return a
    es = []
    for (ex, ey, st) in enemies:
        dx, dy = ex - dmX, ey - dmY
        es.append((dx * dx + dy * dy, ex, ey, st))
    es.sort(key=lambda t: -t[0])
    for d2, ex, ey, st in es:
        dx, dy = ex - dmX, ey - dmY
        dist = math.hypot(dx, dy)
        ang = normAng(math.atan2(dy, dx) - dmA)
        if abs(ang) > DOOM_FOV / 2 + 0.25:
            continue
        perp = dist * math.cos(ang)
        if perp < 0.15:
            continue
        sh = int(rh * 0.75 / perp)
        if sh < 4:
            continue
        sx = int((ang + DOOM_FOV / 2) / DOOM_FOV * SCR_W)
        yb = DOOM_TOP + (rh + int(rh / perp)) // 2
        cyE = yb - sh // 2
        lvl = 3 if perp > 5 else 2 if perp > 3 else 1 if perp > 1.5 else 0
        rw = max(2, sh // 3)
        for x in range(max(0, sx - rw), min(SCR_W - 1, sx + rw) + 1):
            if perp >= zbuf[x // 4]:
                continue
            u = (x - sx) / rw
            hh = int(sh / 2 * math.sqrt(max(0.0, 1.0 - u * u)))
            if hh > 0:
                g.drawFastVLine(x, cyE - hh, hh * 2, dmShadeE[lvl])
        if perp < 4 and st == 1:
            eyesz = max(2, sh // 20)
            g.fillRect(sx - sh // 8, cyE - sh // 6, eyesz, eyesz, C_WHITE)
            g.fillRect(sx + sh // 8 - eyesz, cyE - sh // 6, eyesz, eyesz, C_WHITE)
    # joysticks (repères fixes)
    g.drawCircle(70, 252, 34, C_DGREY)
    g.drawCircle(288, 252, 34, C_DGREY)
    # arme + viseur
    g.fillRect(SCR_W // 2 - 12, DOOM_BOT - 44, 24, 44, C_DGREY)
    g.fillRect(SCR_W // 2 - 5, DOOM_BOT - 58, 10, 20, C_GREY)
    g.drawFastHLine(SCR_W // 2 - 6, DOOM_TOP + rh // 2, 12, C_WHITE)
    g.drawFastVLine(SCR_W // 2, DOOM_TOP + rh // 2 - 6, 12, C_WHITE)
    # minimap
    for y in range(16):
        for x in range(16):
            if DM_MAP[y][x]:
                g.fillRect(8 + x * 3, DOOM_TOP + 4 + y * 3, 3, 3, C_DGREY)
    for (ex, ey, st) in enemies:
        if st == 1:
            g.fillRect(8 + int(ex * 3) - 1, DOOM_TOP + 4 + int(ey * 3) - 1, 3, 3, C_RED)
    g.fillRect(8 + int(dmX * 3) - 1, DOOM_TOP + 4 + int(dmY * 3) - 1, 4, 4, C_ORANGE)
    g.text(64, DOOM_TOP + 6, "SCORE", 1, C_GREY)
    g.smooth(64, DOOM_TOP + 14, "300", C_WHITE)
    # FIRE + quitter
    g.fillCircle(356 + 56, 216 + 38, 30, C_RED_D)
    g.text(356 + 56 - 22, 216 + 38 - 8, "FIRE", 2, C_WHITE)
    g.drawRect(SCR_W - 40, 32, 32, 26, C_GREY)
    g.drawLine(SCR_W - 40 + 8, 32 + 6, SCR_W - 40 + 32 - 9, 32 + 26 - 7, C_RED)
    g.drawLine(SCR_W - 40 + 32 - 9, 32 + 6, SCR_W - 40 + 8, 32 + 26 - 7, C_RED)
    drawFooter(g, 8)
    return g

# ---------------------------------------------------------------- flash
def page_newblock():
    g = GFX()
    g.fillScreen(C_ORANGE)
    g.textCenter("NEW BLOCK", 110, 4, C_BG)
    g.textCenter(str(BLOCK + 1), 170, 3, C_BG)
    g.textCenter("Foundry USA", 220, 2, C_GREY)
    return g

# ---------------------------------------------------------------- main
def main():
    os.makedirs(OUT, exist_ok=True)
    pages = [
        ("page0_prix.png", page_price),
        ("page1_onchain.png", page_chain),
        ("page2_cube.png", page_cube),
        ("page3_pools.png", page_pools),
        ("page4_lightning.png", page_ln),
        ("page5_noeud.png", page_node),
        ("page6_ia.png", page_ai),
        ("page7_signaux.png", page_sig),
        ("page8_doom.png", page_doom),
        ("page_newblock.png", page_newblock),
    ]
    for name, fn in pages:
        g = fn()
        g.im.save(os.path.join(OUT, name))
        print("écrit", name)

if __name__ == "__main__":
    main()
