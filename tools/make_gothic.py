# Genere src/gothic_data.h : police gothique bitmap pour les titres.
# Source : polices « Jacquard 24 » et « Jacquard 12 » (The Soft Type Project Authors,
# licence SIL Open Font License 1.1), rendues a leur taille pixel native (sans lissage).
# usage : python3 make_gothic.py chemin_jacquard24.woff chemin_jacquard12.woff sortie.h
import sys
from PIL import ImageFont

CHARS = [chr(c) for c in range(32, 127)] + list("ÀÂÄÇÈÉÊËÎÏÔÖÙÛÜàâäçèéêëîïôöùûüÿŒœ«»’…–°")

def build(path, size, tag):
    f = ImageFont.truetype(path, size)
    asc, desc = f.getmetrics()
    glyphs = []; bits = bytearray()
    for ch in CHARS:
        adv = int(round(f.getlength(ch)))
        mask, off = f.getmask2(ch, mode="1", anchor="ls")
        w, h = mask.size
        rows = []
        if w > 0 and h > 0:
            data = bytes(mask)
            # getmask2 renvoie une image 1 bit stockee 1 octet par pixel ici (mode "1" -> 0/255)
            for y in range(h):
                for x0 in range(0, w, 8):
                    b = 0
                    for k in range(8):
                        x = x0 + k
                        if x < w and mask.getpixel((x, y)):
                            b |= 0x80 >> k
                    rows.append(b)
        glyphs.append((ord(ch), off[0], off[1], w, h, adv, len(bits)))
        bits.extend(rows)
    return asc, desc, glyphs, bits

def emit(out, tag, data):
    asc, desc, glyphs, bits = data
    out.write(f"#define {tag}_ASC {asc}\n#define {tag}_DESC {desc}\n")
    out.write(f"static const GGlyph {tag}_G[{len(glyphs)}]={{\n")
    for g in glyphs:
        out.write("  {%d,%d,%d,%d,%d,%d,%d},\n" % g)
    out.write("};\n")
    out.write(f"static const uint8_t {tag}_B[{max(1,len(bits))}]={{")
    for i, b in enumerate(bits):
        if i % 32 == 0: out.write("\n  ")
        out.write(f"{b},")
    out.write("\n};\n")

if __name__ == "__main__":
    a = build(sys.argv[1], 24, "G24")
    b = build(sys.argv[2], 12, "G12")
    with open(sys.argv[3], "w", encoding="utf-8") as out:
        out.write("/* police gothique des titres : genere par tools/make_gothic.py, ne pas modifier a la main.\n")
        out.write("   D'apres « Jacquard 24 » et « Jacquard 12 » - Copyright 2023 The Soft Type Project Authors\n")
        out.write("   (https://github.com/scfried/soft-type-jacquard), licence SIL Open Font License 1.1. */\n")
        out.write("typedef struct { uint32_t cp; int16_t ox, oy; uint8_t w, h; uint8_t adv; uint32_t off; } GGlyph;\n")
        emit(out, "G24", a); emit(out, "G12", b)
    print("ok", len(a[2]), len(a[3]), len(b[3]))
