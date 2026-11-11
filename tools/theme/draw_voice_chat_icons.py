#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draws the key pictures of themes/voice-chat: plain pictograms, made from
rectangles, ellipses and arcs. Nothing is copied from another work.

    python3 tools/theme/draw_voice_chat_icons.py themes/voice-chat/icons
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

SIZE = 112
SCALE = 4  # drawn four times larger, then reduced: smooth edges
WHITE = (242, 244, 246, 255)
RED = (255, 82, 82, 255)


def canvas():
    image = Image.new('RGBA', (SIZE * SCALE, SIZE * SCALE), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def s(*values):
    return [v * SCALE for v in values]


def microphone(d, dx=0):
    d.rounded_rectangle(s(40 + dx, 14, 72 + dx, 62), radius=16 * SCALE, fill=WHITE)
    d.arc(s(28 + dx, 30, 84 + dx, 78), 0, 180, fill=WHITE, width=6 * SCALE)
    d.line(s(56 + dx, 78, 56 + dx, 94), fill=WHITE, width=6 * SCALE)
    d.line(s(42 + dx, 96, 70 + dx, 96), fill=WHITE, width=6 * SCALE)


def slash(d):
    d.line(s(22, 22, 90, 90), fill=RED, width=9 * SCALE)


def headphones(d):
    d.arc(s(22, 18, 90, 86), 180, 360, fill=WHITE, width=8 * SCALE)
    d.rounded_rectangle(s(16, 52, 36, 90), radius=8 * SCALE, fill=WHITE)
    d.rounded_rectangle(s(76, 52, 96, 90), radius=8 * SCALE, fill=WHITE)


def waves(d):
    d.arc(s(70, 28, 100, 70), -60, 60, fill=WHITE, width=5 * SCALE)
    d.arc(s(80, 20, 112, 78), -60, 60, fill=WHITE, width=5 * SCALE)


def camera(d):
    d.rounded_rectangle(s(12, 30, 72, 82), radius=10 * SCALE, fill=WHITE)
    d.polygon(s(78, 46, 102, 32, 102, 80, 78, 66), fill=WHITE)


def screen_share(d):
    d.rounded_rectangle(s(12, 18, 100, 78), radius=8 * SCALE, outline=WHITE, width=6 * SCALE)
    d.line(s(56, 94, 56, 80), fill=WHITE, width=6 * SCALE)
    d.line(s(36, 96, 76, 96), fill=WHITE, width=6 * SCALE)
    d.polygon(s(56, 30, 40, 50, 72, 50), fill=WHITE)  # an arrow going up
    d.rectangle(s(51, 48, 61, 66), fill=WHITE)


PICTURES = {
    'mute.png': lambda d: (microphone(d), slash(d)),
    'deafen.png': lambda d: (headphones(d), slash(d)),
    'talk.png': lambda d: (microphone(d, dx=-12), waves(d)),
    'camera.png': camera,
    'share.png': screen_share,
}


def main(folder):
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    for name, draw in PICTURES.items():
        image, d = canvas()
        draw(d)
        image.resize((SIZE, SIZE), Image.LANCZOS).save(folder / name, optimize=True)
        print(name)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
