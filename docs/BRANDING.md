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

## Character artwork and localized UI

- `assets/reference/banner-reference.jpg`: supplied 474 × 266 character reference.
- `assets/gaming-header.png`: text-free AI extension for the native app header. Native text overlays translate immediately.
- `docs/images/roclick-banner.png`: matching character-led README banner.
- `assets/reference/generation-prompts.json`: exact prompts and generation method (built-in imagegen).
- `docs/images/roclick-showcase.svg` and `.png`: presentation sheet assembled from the four actual installed-app captures.

The R app icon remains unchanged by the character-art refresh. The app uses navy dark surfaces, light neutral surfaces, cyan actions, and restrained red in the artwork and error state. High contrast hides the decorative image and uses Windows system colors.

Run `python3 scripts/create-showcase.py` after replacing the four screenshots to regenerate the showcase. This documentation tool requires `rsvg-convert`; it adds no dependency to the installed app.
