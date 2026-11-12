"""Draws the Zap icon (a lightning bolt on a dark rounded square) and writes win/zap.ico.

Needs Pillow:  python scripts/make-icon.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 1024  # drawn large, then scaled down for each icon size
ROOT = Path(__file__).resolve().parent.parent


def vertical_gradient(top, bottom):
    img = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(img)
    for y in range(SIZE):
        t = y / (SIZE - 1)
        draw.line([(0, y), (SIZE, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    return img


def mask(draw_fn):
    m = Image.new("L", (SIZE, SIZE), 0)
    draw_fn(ImageDraw.Draw(m))
    return m


def build():
    pad = 40
    tile = mask(lambda d: d.rounded_rectangle([pad, pad, SIZE - pad, SIZE - pad], radius=220, fill=255))

    # The bolt, as fractions of the canvas.
    bolt_points = [(0.585, 0.115), (0.265, 0.560), (0.465, 0.560), (0.405, 0.885), (0.735, 0.425), (0.530, 0.425)]
    bolt = mask(lambda d: d.polygon([(x * SIZE, y * SIZE) for x, y in bolt_points], fill=255))

    icon = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    icon.paste(vertical_gradient((52, 54, 78), (20, 21, 30)), (0, 0), tile)

    # A soft glow behind the bolt, kept inside the tile.
    glow = Image.new("RGBA", (SIZE, SIZE), (255, 190, 60, 0))
    glow.putalpha(bolt.filter(ImageFilter.GaussianBlur(60)).point(lambda v: int(v * 0.55)))
    glow_layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    glow_layer.paste(glow, (0, 0), tile)
    icon = Image.alpha_composite(icon, glow_layer)

    bolt_layer = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    bolt_layer.paste(vertical_gradient((255, 236, 110), (255, 150, 30)), (0, 0), bolt)
    icon = Image.alpha_composite(icon, bolt_layer)

    out = ROOT / "win" / "zap.ico"
    icon.save(out, sizes=[(s, s) for s in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
    icon.resize((256, 256), Image.LANCZOS).save(ROOT / "docs" / "zap-icon.png")
    print(f"wrote {out}")


if __name__ == "__main__":
    build()
