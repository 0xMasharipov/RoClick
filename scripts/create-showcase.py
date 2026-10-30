"""Lay out real installed-app captures without redrawing their contents."""
import base64
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
output = root / "docs/images/roclick-showcase.svg"
parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="1400" height="2110" viewBox="0 0 1400 2110">',
         '<rect width="1400" height="2110" fill="#101722"/>',
         '<g font-family="DejaVu Sans,sans-serif" fill="#F1F6FC">',
         '<text x="80" y="90" font-size="46" font-weight="700">RoClick</text>',
         '<text x="80" y="133" font-size="23" fill="#A9B9CE">English + Türkçe · Light + Dark</text>',
         '<text x="80" y="171" font-size="19" fill="#A9B9CE">Actual installed Windows app captures · Scrollable viewport</text>']
for index, (locale, theme, label) in enumerate([
    ("en", "light", "English / Light"), ("en", "dark", "English / Dark"),
    ("tr", "light", "Türkçe / Açık"), ("tr", "dark", "Türkçe / Koyu"),
]):
    source = root / f"docs/screenshots/roclick-{locale}-{theme}.png"
    raw = source.read_bytes()
    width, height = struct.unpack(">II", raw[16:24])
    x, y = 80 + (index % 2) * 660, 235 + (index // 2) * 920
    display_width = 580
    display_height = height * display_width / width
    parts.append(f'<text x="{x}" y="{y}" font-size="25" font-weight="600">{label}</text>')
    parts.append(f'<image x="{x}" y="{y+25}" width="{display_width}" height="{display_height}" href="data:image/png;base64,{base64.b64encode(raw).decode()}"/>')
parts += ['<text x="80" y="2070" font-size="17" fill="#A9B9CE">Release candidate · Windows 10/11 reference-machine acceptance remains pending</text>', '</g></svg>']
output.write_text("\n".join(parts), encoding="utf-8")
subprocess.run(["rsvg-convert", str(output), "-o", str(output.with_suffix(".png"))], check=True)
print(output)
print(output.with_suffix(".png"))
