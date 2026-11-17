"""Colour calibration of the VFX kit: reads NS_GSTN_Swatch in a side frame of a PIE_SWATCH=1 capture (pie_capture_vfx.py) and writes what
each emissive value (setup_presentation_vfx.SWATCH_ROWS x gains) shows on screen.
    python vfx_swatch.py <frame.png> <out.json> [--px=1.8]
The grid is centred in the frame (the swatch sits on the line from the side camera to the look-at point); --px = pixels per cm at the
swatch (480 px, FOV 50, 0.55 x the camera distance: 1.8 for the 520 cm 'jump' sequence). Columns are flipped if the grey row is not rising.
Prints one line.
"""
import json
import math
import sys

from PIL import Image

STEP = 26.0
ROWS = ["grey", "dust", "dust_shade", "hit_end", "hit_mid", "accent"]
GAINS = {"grey": [0.25, 0.5, 1.0, 2.0, 4.0, 8.0]}
DEFAULT_GAINS = [0.7, 1.0, 1.5, 2.2, 3.0, 4.5]


def main():
    frame, out = sys.argv[1], sys.argv[2]
    px = next((float(a.split("=")[1]) for a in sys.argv[3:] if a.startswith("--px=")), 1.8)
    img = Image.open(frame).convert("RGB")
    w, h = img.size
    grid = []
    for j in range(6):
        row = []
        for i in range(6):
            x = w / 2 + (i - 2.5) * STEP * px
            y = h / 2 - (2.5 - j) * STEP * px
            samples = sorted((img.getpixel((int(x) + dx, int(y) + dy)) for dx in range(-3, 4) for dy in range(-3, 4)), key=sum)
            row.append(samples[len(samples) // 2])
        grid.append(row)
    grey = [sum(c) for c in grid[0]]
    flipped = grey[0] > grey[-1]
    if flipped:
        grid = [list(reversed(r)) for r in grid]
    result = {}
    for name, row in zip(ROWS, grid):
        gains = GAINS.get(name, DEFAULT_GAINS)
        result[name] = [{"gain": k, "screen": "#%02X%02X%02X" % c} for k, c in zip(gains, row)]
    result["_flipped"] = flipped
    json.dump(result, open(out, "w"), indent=1)
    print(f"swatch: {out} grey {[r['screen'] for r in result['grey']]}")


if __name__ == "__main__":
    main()
