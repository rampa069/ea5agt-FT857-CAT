#!/usr/bin/env python3
"""Convierte fuentes TrueType a fuentes suaves de TFT_eSPI (formato .vlw) como cabecera C.

Genera src/ui/fonts/fonts.h y un .h por fuente con `const uint8_t <nombre>[] PROGMEM`, que se
cargan en el firmware con tft.loadFont(<nombre>). Necesita Pillow:

    python3 -m venv /tmp/v && /tmp/v/bin/pip install pillow
    /tmp/v/bin/python tools/fonts/make_vlw.py

Formato (Processing .vlw, enteros de 32 bits big-endian): cabecera de 6 valores
(nº de glifos, versión, tamaño, 0, ascent, descent), 7 valores por glifo (unicode, alto, ancho,
avance, dY sobre la línea base, dX, 0) y después los mapas alfa de 8 bits de cada glifo.
"""

import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
TTF = ROOT / "tools/fonts/ttf"
OUT = ROOT / "src/ui/fonts"

ASCII = "".join(chr(c) for c in range(0x21, 0x7F))
LATIN1 = "¡¿ÁÉÍÓÚÜÑáéíóúüñºª·°±"
TEXT = ASCII + LATIN1
DIGITS = "0123456789.,:-+ "

# (nombre en C, fichero, variación (eje de peso) o None, tamaño en píxeles, caracteres)
FONTS = [
    ("oswald13", "Oswald.ttf", "Regular", 13, TEXT),
    ("oswald10", "Oswald.ttf", "Regular", 10, TEXT),
    ("oswaldSemi20", "Oswald.ttf", "SemiBold", 20, TEXT),
    ("oswaldSemi24", "Oswald.ttf", "SemiBold", 24, TEXT),
    ("nixie54", "NixieOne-Regular.ttf", None, 54, DIGITS),
    ("nixie24", "NixieOne-Regular.ttf", None, 24, TEXT),
    ("elite18", "SpecialElite-Regular.ttf", None, 18, TEXT),
    ("elite13", "SpecialElite-Regular.ttf", None, 13, TEXT),
    ("elite10", "SpecialElite-Regular.ttf", None, 10, TEXT),
]


def load(file, variation, size):
    font = ImageFont.truetype(str(TTF / file), size)
    if variation:
        font.set_variation_by_name(variation)
    return font


def glyph(font, ch):
    left, top, right, bottom = font.getbbox(ch, anchor="ls")
    w, h = max(0, right - left), max(0, bottom - top)
    advance = round(font.getlength(ch))
    bitmap = b""
    if w and h:
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-left, -top), ch, font=font, fill=255, anchor="ls")
        bitmap = img.tobytes()
    return {"code": ord(ch), "w": w, "h": h, "adv": advance, "dy": -top, "dx": left, "bitmap": bitmap}


def build(name, file, variation, size, chars):
    font = load(file, variation, size)
    ascent, descent = font.getmetrics()
    glyphs = sorted((glyph(font, c) for c in set(chars) if c != " "), key=lambda g: g["code"])
    data = struct.pack(">6i", len(glyphs), 11, size, 0, ascent, descent)
    for g in glyphs:
        data += struct.pack(">7i", g["code"], g["h"], g["w"], g["adv"], g["dy"], g["dx"], 0)
    for g in glyphs:
        data += g["bitmap"]
    lines = [f"// Generado por tools/fonts/make_vlw.py desde {file} ({variation or 'Regular'}, {size} px). No editar.",
             "#pragma once", "#include <pgmspace.h>", "",
             f"const uint8_t {name}[] PROGMEM = {{"]
    for i in range(0, len(data), 24):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i + 24]) + ",")
    lines += ["};", ""]
    (OUT / f"{name}.h").write_text("\n".join(lines))
    return len(data)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    total = 0
    for spec in FONTS:
        n = build(*spec)
        total += n
        print(f"{spec[0]:14s} {n:7d} bytes")
    header = ["// Fuentes suaves de los temas. Generado por tools/fonts/make_vlw.py. No editar.", "#pragma once", ""]
    header += [f'#include "{spec[0]}.h"' for spec in FONTS]
    (OUT / "fonts.h").write_text("\n".join(header) + "\n")
    print(f"total          {total:7d} bytes")


if __name__ == "__main__":
    main()
