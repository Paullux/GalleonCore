#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds the icons of `.gctheme` files from one source picture.

The picture is shipped by the software, never stored inside a theme file. One
source gives everything the packages and the Windows installer install:

* `gctheme-<n>.png` for the hicolor sizes (installed by bosun/CMakeLists.txt as
  `application-x-galleoncore-theme.png`);
* `gctheme.ico` (installed by the Windows Setup as the `.gctheme` DefaultIcon).

    python3 tools/theme-icon/build_icons.py [SOURCE] [OUTPUT_DIR]

SOURCE is a square PNG with a transparent background, at least 512 x 512
(default: tools/theme-icon/source/galleoncore-theme.png), or an SVG, rendered
with rsvg-convert, inkscape or ImageMagick, whichever is installed.
OUTPUT_DIR defaults to bosun/icons/theme-file. The only Python dependency is
Pillow. The output is deterministic: the same source gives the same bytes.
See tools/theme-icon/README.md.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = ROOT / 'tools/theme-icon/source/galleoncore-theme.png'
DEFAULT_OUTPUT = ROOT / 'bosun/icons/theme-file'
PNG_SIZES = (16, 22, 24, 32, 48, 64, 128, 256, 512)
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)
MINIMUM_SOURCE = 512
SVG_RENDER_SIZE = 1024


def png_name(size):
    return f'gctheme-{size}.png'


ICO_NAME = 'gctheme.ico'


def svg_command(renderer, svg, png):
    """The command line that renders `svg` to a square `png`, for a known renderer."""
    size = str(SVG_RENDER_SIZE)
    if renderer == 'rsvg-convert':
        return [renderer, '-w', size, '-h', size, '-o', str(png), str(svg)]
    if renderer == 'inkscape':
        return [renderer, str(svg), '-w', size, '-h', size, '-o', str(png)]
    return [renderer, '-background', 'none', '-density', '384', str(svg), '-resize', f'{size}x{size}', str(png)]


def render_svg(svg, png):
    for renderer in ('rsvg-convert', 'inkscape', 'magick', 'convert'):
        if shutil.which(renderer):
            subprocess.run(svg_command(renderer, svg, png), check=True)
            return
    raise SystemExit('No SVG renderer found (rsvg-convert, inkscape or ImageMagick): render the SVG to a PNG yourself and pass that.')


def check_source(width, height):
    """Raises SystemExit unless the picture is square and big enough to be reduced, not enlarged."""
    if width != height:
        raise SystemExit(f'The source must be square, not {width} x {height}.')
    if width < MINIMUM_SOURCE:
        raise SystemExit(f'The source must be at least {MINIMUM_SOURCE} x {MINIMUM_SOURCE}, not {width}.')


def build(source, output):
    try:
        from PIL import Image
    except ImportError:
        raise SystemExit('Pillow is required: pip install pillow') from None
    source = Path(source)
    output = Path(output)
    with tempfile.TemporaryDirectory() as tmp:
        if source.suffix.lower() == '.svg':
            rendered = Path(tmp) / 'source.png'
            render_svg(source, rendered)
            source = rendered
        with Image.open(source) as opened:
            image = opened.convert('RGBA')
        check_source(*image.size)
        output.mkdir(parents=True, exist_ok=True)
        for size in PNG_SIZES:
            image.resize((size, size), Image.LANCZOS).save(output / png_name(size), optimize=True)
        image.resize((max(ICO_SIZES),) * 2, Image.LANCZOS).save(
            output / ICO_NAME, format='ICO', sizes=[(size, size) for size in ICO_SIZES])
    return [png_name(size) for size in PNG_SIZES] + [ICO_NAME]


if __name__ == '__main__':
    if len(sys.argv) > 3 or any(arg in ('-h', '--help') for arg in sys.argv[1:]):
        raise SystemExit(__doc__)
    for name in build(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SOURCE,
                      sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUTPUT):
        print(name)
