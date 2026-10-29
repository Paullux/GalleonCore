#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep the published website on the prior release until its downloads exist."""
import argparse
import json
import re
from pathlib import Path
from urllib.parse import unquote


def ready(html, version, release):
    required = {unquote(name) for tag, name in re.findall(
        r'https://github\.com/Paullux/GalleonCore/releases/download/v([0-9.]+)/([^"<>\s]+)', html)
        if tag == version}
    available = {item['name'] for item in release.get('assets', []) if isinstance(item, dict) and isinstance(item.get('name'), str)}
    return bool(required) and not release.get('isDraft', True) and not release.get('isPrerelease', True) and required <= available


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('release', type=Path)
    parser.add_argument('html', type=Path)
    args = parser.parse_args()
    print('true' if ready(args.html.read_text(), args.version, json.loads(args.release.read_text())) else 'false')
