#!/usr/bin/env python3
"""Generate the app icons in resources/icons/.

An open book on Bootstrap blue: a Latin "a" on the left page and the Kurdish
letter "ک" (the first letter of کوردی, "Kurdish") on the right.

Needs Pillow and the Noto Serif / Noto Naskh Arabic fonts
(`sudo apt install python3-pil fonts-noto-core`). Run from the repo root:

    python3 tools/make_icons.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONTS = Path("/usr/share/fonts/truetype/noto")
OUT = Path(__file__).resolve().parent.parent / "resources" / "icons"

BLUE = (13, 110, 253)  # Bootstrap primary; also theme-color in addHeadTags()
DARK_BLUE = (10, 88, 202)
WHITE = (255, 255, 255)
INK = (33, 37, 41)

S = 2048  # drawn large, then downscaled for smooth edges


def draw_centered(d, text, font, fill, x, y):
    """Draws text with its visible ink centred on (x, y). Centring on the
    font's metrics instead puts "a" low (no ascender) and "ک" high (Naskh's
    deep descender space)."""
    left, top, right, bottom = d.textbbox((0, 0), text, font=font)
    d.text((x - (left + right) / 2, y - (top + bottom) / 2), text, font=font,
           fill=fill)


def draw_icon(size, rounded, scale=1.0):
    """rounded: rounded-square background (browsers, Android launchers);
    otherwise full-bleed, for platforms that apply their own mask.
    scale: shrinks the book, e.g. to fit a maskable icon's safe zone."""
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    if rounded:
        d.rounded_rectangle([0, 0, S - 1, S - 1], radius=S * 0.22, fill=BLUE)
    else:
        d.rectangle([0, 0, S, S], fill=BLUE)

    k = scale
    cx, cy = S / 2, S / 2
    top, bottom = cy - S * 0.24 * k, cy + S * 0.22 * k
    sag = S * 0.035 * k  # pages dip towards the spine
    for side in (-1, 1):
        inner = cx + side * S * 0.015 * k
        outer = cx + side * S * 0.37 * k
        d.polygon([(inner, top + sag), (outer, top), (outer, bottom),
                   (inner, bottom + sag)], fill=WHITE)
    d.line([(cx, top + sag), (cx, bottom + sag)], fill=DARK_BLUE,
           width=int(S * 0.014 * k))

    latin = ImageFont.truetype(str(FONTS / "NotoSerif-Bold.ttf"),
                               int(S * 0.24 * k))
    kurdish = ImageFont.truetype(str(FONTS / "NotoNaskhArabic-Bold.ttf"),
                                 int(S * 0.30 * k))
    page_y = (top + bottom + sag) / 2  # vertical middle of a page
    page_x = S * (0.015 + 0.37) / 2 * k  # horizontal middle, from the spine
    draw_centered(d, "a", latin, INK, cx - page_x, page_y)
    draw_centered(d, "ک", kurdish, INK, cx + page_x, page_y)

    return img.resize((size, size), Image.LANCZOS)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    icons = {
        "icon-192.png": draw_icon(192, rounded=True),
        "icon-512.png": draw_icon(512, rounded=True),
        # Maskable icons may be cropped to a circle of 80% of the width.
        "icon-maskable-512.png": draw_icon(512, rounded=False, scale=0.85),
        # iOS rounds the corners itself.
        "apple-touch-icon.png": draw_icon(180, rounded=False),
        "favicon-32.png": draw_icon(32, rounded=True),
    }
    for name, img in icons.items():
        img.save(OUT / name, optimize=True)
        print(OUT / name)


if __name__ == "__main__":
    main()
