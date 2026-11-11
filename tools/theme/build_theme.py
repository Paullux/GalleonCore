#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Packs a theme folder into a `.gc.theme` file (see docs/THEME_FORMAT.md).

The folder holds theme.json and optionally credits.txt, icons/*.png and
wallpapers/*.jpg. Entries are STORED, not compressed, with a fixed date, so the
same folder always gives the same bytes. The result is only a packing: the file
is checked by GalleonCore when it is installed (or by the test that reads the
shipped themes).

    python3 tools/theme/build_theme.py themes/voice-chat build/themes/voice-chat.gc.theme
"""
import sys
import zipfile
from pathlib import Path

FIXED_DATE = (1980, 1, 1, 0, 0, 0)


def entries(folder):
    """The files of a theme folder as (archive name, path), manifest first, in a fixed order."""
    folder = Path(folder)
    names = ['theme.json', 'credits.txt']
    found = [(n, folder / n) for n in names if (folder / n).is_file()]
    for sub in ('icons', 'wallpapers'):
        found += [(f'{sub}/{p.name}', p) for p in sorted((folder / sub).glob('*')) if p.is_file()]
    return found


def build(folder, output):
    files = entries(folder)
    if not files or files[0][0] != 'theme.json':
        raise SystemExit(f'{folder}: no theme.json')
    Path(output).parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_STORED) as archive:
        for name, path in files:
            info = zipfile.ZipInfo(name, FIXED_DATE)
            info.compress_type = zipfile.ZIP_STORED
            info.external_attr = 0o644 << 16
            archive.writestr(info, path.read_bytes())
    return [name for name, _ in files]


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    for entry in build(sys.argv[1], sys.argv[2]):
        print(entry)
