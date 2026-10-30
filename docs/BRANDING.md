# RoClick branding

The blue rounded-square R remains the app identity. The refreshed mark uses a cyan click dot and quarter arc, a white R, and a darker blue base for legibility at small sizes.

## Assets

- `assets/roclick-logo.svg`: editable vector source, mirrored at `roclick-logo.svg` for compatibility.
- `assets/roclick-logo.png`: transparent 1,024 × 1,024 export used in the React Native app.
- `assets/roclick.ico`: Windows icon with 16, 20, 24, 32, 40, 48, 64, 128, and 256 px frames.
- `windows/RoClick/RoClick.ico` and `windows/RoClick/small.ico`: identical Windows resource copies. The installer uses `assets/roclick.ico`.
- `docs/images/roclick-banner.png`: generated README banner. This is brand artwork, not an app screenshot.
- `docs/screenshots/`: actual installed-app captures from Windows CI; these are never generated mockups.

The icon was edited directly as SVG, exported with `rsvg-convert`, and packaged as ICO using Pillow. The banner was made with the built-in image generation tool using the updated PNG logo as its brand reference.

## Banner prompt

> Create a polished wide GitHub README banner for RoClick, a local Windows auto-clicker. Use the attached blue rounded-square R app logo as an EXACT brand reference: white bold R, cyan click dot and upper-right quarter arc. Composition: wide landscape 2.5:1 editorial software brand cover, dark ink navy #101722 background, very subtle blue light, generous negative space. Left side typographic hierarchy with exact large text 'RoClick', smaller line 'On your desktop. Under your control.', and small supporting line 'A local auto-clicker for Windows'. Right side: one large refined version of the supplied app icon on a subtle dark rounded tile, crisp, mostly flat with restrained dimensional depth, thin cyan click ripples extending behind it. Typography is clean modern sans-serif, white and cool blue. Mature, precise Windows utility identity. No fake app UI, no statistics, no speed claims, no badges, no extra text, no photos. All copy fully legible within safe margins. This is a banner illustration, not a screenshot. Final image is intended for the repository README.
