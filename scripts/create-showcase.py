"""Lay out real installed-app captures without redrawing their contents."""
import base64
import math
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
output = root / "docs/images/roclick-showcase.svg"
captures = [
    ("en", "light", "English / Light"), ("en", "dark", "English / Dark"),
    ("tr", "light", "Türkçe / Açık"), ("tr", "dark", "Türkçe / Koyu"),
]
display_width = 580
max_height = 0
for locale, theme, _ in captures:
    raw = (root / f"docs/screenshots/roclick-{locale}-{theme}.png").read_bytes()
    width, height = struct.unpack(">II", raw[16:24])
    max_height = max(max_height, math.ceil(height * display_width / width))
row_height = max_height + 100
footer_y = 235 + row_height + 25 + max_height + 70
canvas_height = footer_y + 40
parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="{canvas_height}" viewBox="0 0 1400 {canvas_height}">',
         f'<rect width="1400" height="{canvas_height}" fill="#101722"/>',
         '<g font-family="DejaVu Sans,sans-serif" fill="#F1F6FC">',
         '<text x="80" y="90" font-size="46" font-weight="700">RoClick</text>',
         '<text x="80" y="133" font-size="23" fill="#A9B9CE">English + Türkçe · Light + Dark</text>',
         '<text x="80" y="171" font-size="19" fill="#A9B9CE">Actual installed Windows app captures · Scrollable viewport</text>']
for index, (locale, theme, label) in enumerate(captures):
    source = root / f"docs/screenshots/roclick-{locale}-{theme}.png"
    raw = source.read_bytes()
    width, height = struct.unpack(">II", raw[16:24])
    x, y = 80 + (index % 2) * 660, 235 + (index // 2) * row_height
    display_height = height * display_width / width
    parts.append(f'<text x="{x}" y="{y}" font-size="25" font-weight="600">{label}</text>')
    parts.append(f'<image x="{x}" y="{y+25}" width="{display_width}" height="{display_height}" href="data:image/png;base64,{base64.b64encode(raw).decode()}"/>')
parts += [f'<text x="80" y="{footer_y}" font-size="17" fill="#A9B9CE">Release candidate · Windows 10/11 reference-machine acceptance remains pending</text>', '</g></svg>']
output.write_text("\n".join(parts), encoding="utf-8")
subprocess.run(["rsvg-convert", str(output), "-o", str(output.with_suffix(".png"))], check=True)
print(output)
print(output.with_suffix(".png"))
