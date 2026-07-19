# gen_smooth_font.py — génère smooth_font.h : chiffres anti-aliasés (alpha 4 bpp)
# Usage : python gen_smooth_font.py   (nécessite Pillow)
from PIL import Image, ImageDraw, ImageFont

CHARS = "0123456789 ,.%:-+/"
FONT_PATH = "C:/Windows/Fonts/arialbd.ttf"
SIZE = 46                      # ~34 px de hauteur de chiffre
OUT = "smooth_font.h"

font = ImageFont.truetype(FONT_PATH, SIZE)
glyphs = []   # (char, w, h, xoff, yoff, adv, offset)
data = bytearray()

for ch in CHARS:
    img = Image.new("L", (SIZE * 2, SIZE * 2), 0)
    d = ImageDraw.Draw(img)
    d.text((SIZE // 2, 0), ch, font=font, fill=255)
    bbox = img.getbbox()
    adv = int(round(font.getlength(ch)))
    if bbox is None:                       # espace : pas de pixels
        glyphs.append((ch, 0, 0, 0, 0, adv, 0))
        continue
    crop = img.crop(bbox)
    w, h = crop.size
    xoff = bbox[0] - SIZE // 2
    yoff = bbox[1]
    off = len(data)
    px = crop.load()
    for yy in range(h):                    # 2 pixels par octet (4 bpp, MSB d'abord)
        for xx in range(0, w, 2):
            hi = px[xx, yy] >> 4
            lo = (px[xx + 1, yy] >> 4) if xx + 1 < w else 0
            data.append((hi << 4) | lo)
    glyphs.append((ch, w, h, xoff, yoff, adv, off))

line_h = max(g[2] + g[4] for g in glyphs if g[1])

with open(OUT, "w", encoding="ascii", errors="replace") as f:
    f.write("// smooth_font.h — chiffres lisses (alpha 4 bpp) generes par gen_smooth_font.py\n")
    f.write("// Font : Arial Bold %dpx · %d glyphes · hauteur ligne %dpx\n".replace("·", "-") % (SIZE, len(glyphs), line_h))
    f.write("#pragma once\n#include <pgmspace.h>\n\n")
    f.write("#define SF_COUNT %d\n" % len(glyphs))
    f.write("#define SF_LINEH %d\n\n" % line_h)
    f.write("const uint8_t SF_CHARS[] PROGMEM = {")
    f.write(",".join("'%s'" % g[0] for g in glyphs))
    f.write("};\n\n")
    f.write("struct SFGlyph { uint16_t offset; uint8_t w, h; int8_t xoff, yoff, adv; };\n")
    f.write("const SFGlyph SF_GLYPHS[] PROGMEM = {\n")
    for g in glyphs:
        f.write("  {%d,%d,%d,%d,%d,%d}, // '%s'\n" % (g[6], g[1], g[2], g[3], g[4], g[5], g[0]))
    f.write("};\n\n")
    f.write("const uint8_t SF_DATA[] PROGMEM = {\n")
    for i in range(0, len(data), 16):
        f.write("  " + ",".join("0x%02X" % b for b in data[i:i + 16]) + ",\n")
    f.write("};\n")

print("OK: %s (%d octets de données, hauteur %dpx)" % (OUT, len(data), line_h))
for g in glyphs:
    print("  '%s' %dx%d adv=%d" % (g[0], g[1], g[2], g[5]))
