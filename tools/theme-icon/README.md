<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Icon of `.gctheme` files

The icon of theme files is **shipped by the software**, never stored inside a `.gctheme` file (the format, [docs/THEME_FORMAT.md](../../docs/THEME_FORMAT.md), has no icon entry and must not get one).

| Platform | What is installed | By |
|---|---|---|
| Linux | `application/x-galleoncore-theme` (`bosun/mime/galleoncore-theme.xml`, glob `*.gctheme`, a kind of `application/zip`) and the icon `application-x-galleoncore-theme` in hicolor (`<n>x<n>/mimetypes`, 16 to 512 px) | `bosun/CMakeLists.txt`, so the Debian/PPA, AUR and AppImage packages get it from `cmake --install`; the RPM/COPR `%files` lists it. There is no Flatpak or Snap recipe. |
| Windows | `HKCU\Software\Classes\.gctheme` → ProgID `GalleonCore.Theme` (label "GalleonCore theme" / "Thème GalleonCore"), `DefaultIcon` → `<install folder>\gctheme.ico`, then `SHChangeNotify` | `tools/packaging/windows/galleoncore.nsi` (current user only; no open verb; uninstall removes exactly these keys) |

Bosun does not open a file given on its command line, so `galleon-bosun.desktop` has no `MimeType=` and the file type has no "open with" action: it is a name and an icon only.

## Files

- `source/galleoncore-theme.png`: the master picture (square, transparent, at least 512 × 512; currently 1024 × 1024). **This is the file to replace.**
- `source/galleoncore-theme.svg`: the SVG supplied by the owner, from which the master is rendered (1024 px, transparent, e.g. with Chromium or `rsvg-convert -w 1024 -h 1024`). Keep it in step with the master.
- `build_icons.py`: makes everything below from the master.
- `../../bosun/icons/theme-file/gctheme-{16,22,24,32,48,64,128,256,512}.png` and `gctheme.ico` (16, 24, 32, 48, 64, 128, 256): the generated, committed result, so building the packages needs neither Python nor Pillow.

## Replacing the picture

1. Put the new square, transparent PNG (at least 512 × 512) at `tools/theme-icon/source/galleoncore-theme.png`. The provenance goes in [ASSETS.md](../../ASSETS.md) (tool, date, prompt, licence).
2. `python3 tools/theme-icon/build_icons.py` (needs Pillow). It rewrites the ten files of `bosun/icons/theme-file/`; a given picture always gives the same bytes. An SVG can be passed instead of the PNG if `rsvg-convert`, `inkscape` or ImageMagick is installed: `python3 tools/theme-icon/build_icons.py my.svg`.
3. Replace or delete `source/galleoncore-theme.svg` so it matches the master.
4. `python3 -m unittest tools/test_theme_icon.py`: it also checks that the committed icons are exactly what the script makes from the master.

Nothing else changes: the MIME file, CMake, the RPM `%files` and the NSIS script only refer to these file names.
