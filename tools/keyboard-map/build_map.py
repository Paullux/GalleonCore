#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds the drawing of the Galleon 100 SD (keyboard and Stream Deck module) from this repository's own data.

Nothing here comes from a picture of the product. The sources are:
  - docs/protocol/lamps-2b0c.csv   the 108 lamps of the keyboard as the device reports them, in micrometres
    (measured 2026-10-18, see docs/protocol/README.md "Per-lamp attributes");
  - docs/protocol/lamps-2b18.csv   the two lamps around the dials;
  - data/devices/galleon-100-sd.json   the module's screen size, key pitch and gaps (measured 2026-10-19);
  - data/devices/galleon-100-sd-keymap.json   which LED of the live frame lights which key (calibration of the owner,
    2026-11-15): this is what gives every cap its HID usage and its LED, and every LED its position.

What is measured and what is not (also written into the output, `notes`):
  - cells  = the keyboard lamps 0..93 AT THEIR MEASURED POSITIONS. Each lamp is given to the nearest cap
             (`lamps` of the cap): that cap is what a colour would go to. The light is drawn UNDER an opaque dark
             cap whose legend and outline are see-through, as on the real keys.
             Lamps 94..107 are not drawn: their Y = 0 may be a placeholder and nothing lit when tried.
  - caps   = the key caps. DRAWN FROM THE STANDARD FRENCH (AZERTY, ISO) TENKEYLESS LAYOUT, row by row in whole
             and half key units, at the key pitch the lamps give (20.1 mm, not the standard 19.05); they are
             decoration, not measurements. The device gives no key names, so which
             lamp belongs under which cap is only as good as their positions (an LED is a few millimetres off the
             centre of its key).
  - module = the screen, the 12 keys and the two dials. Sizes from data/devices (measured); the position of the
             module against the keyboard (its centre, the height of the dials) is ESTIMATED from the proportions
             of the product and has to be measured with a ruler.
  - leds   = the position of every LED of the live frame that lights a key (the mean of the MEASURED lamps of its cap)
             or a section of the light strip (the strip is drawn, not measured: ESTIMATE). Written into the map and into
             data/devices/galleon-100-sd-led-positions.json, which the effects of galleond will read.
Run:  python3 tools/keyboard-map/build_map.py          (writes the generated files)
      python3 tools/keyboard-map/build_map.py --check  (fails if they are out of date)
"""
import csv
import json
import sys
from pathlib import Path
from xml.sax.saxutils import escape

# The licence lines written into the generated files (not the licence of this script: see the header above).
# REUSE-IgnoreStart
SPDX_LINES = ["SPDX-FileCopyrightText: 2026 Paul Woisard", "SPDX-License-Identifier: GPL-3.0-or-later"]
# REUSE-IgnoreEnd

ROOT = Path(__file__).resolve().parents[2]
LAMPS_KEYBOARD = ROOT / "docs/protocol/lamps-2b0c.csv"
LAMPS_MODULE = ROOT / "docs/protocol/lamps-2b18.csv"
DEVICE = ROOT / "data/devices/galleon-100-sd.json"
KEYMAP = ROOT / "data/devices/galleon-100-sd-keymap.json"
OUT_LEDS = ROOT / "data/devices/galleon-100-sd-led-positions.json"
OUT_JS = ROOT / "bosun/qml/KeyboardMapData.js"
OUT_SVG = ROOT / "bosun/icons/galleon-100-sd-map.svg"          # to look at: with a light behind
OUT_OVERLAY = ROOT / "bosun/icons/galleon-100-sd-overlay.svg"  # the real thing: the holes are alpha 0

NUMBER_ROW = range(12, 22)  # lamps 12..21: the ten keys "1" to "0", side by side
GAP = 1.6             # mm between two caps
ROW_Y = [43.0, 63.5, 84.0, 104.0, 125.0, 146.0]  # mm, the Y of each row of lamps as measured
NAV_X = [345.0, 365.0, 385.0]                     # mm, the three columns right of the main block (measured)
LAST_KEY_LAMP = 93                                 # lamps 0..93 are keys

# The main block, row by row: (label, width in units). A row is 15 units wide.
MAIN_ROWS = [
    # row 0 is the function row, placed on its own measured X (see below)
    None,
    [("²", 1), ("&", 1), ("é", 1), ('"', 1), ("'", 1), ("(", 1), ("-", 1), ("è", 1), ("_", 1), ("ç", 1), ("à", 1), (")", 1), ("=", 1), ("⌫", 2)],
    [("⇥", 1.5), ("A", 1), ("Z", 1), ("E", 1), ("R", 1), ("T", 1), ("Y", 1), ("U", 1), ("I", 1), ("O", 1), ("P", 1), ("^", 1), ("$", 1), ("", 1.5, 0x28)],
    [("⇪", 1.75), ("Q", 1), ("S", 1), ("D", 1), ("F", 1), ("G", 1), ("H", 1), ("J", 1), ("K", 1), ("L", 1), ("M", 1), ("ù", 1), ("*", 1), ("", 1.25, 0x28)],
    [("⇧", 1.25, 0xE1), ("<", 1), ("W", 1), ("X", 1), ("C", 1), ("V", 1), ("B", 1), ("N", 1), (",", 1), (";", 1), (":", 1), ("!", 1), ("⇧", 2.75, 0xE5)],
    [("Ctrl", 1.25), ("❖", 1.25), ("Alt", 1.25), ("", 6.25, 0x2C), ("AltGr", 1.25), ("Fn", 1.25), ("☰", 1.25), ("◉", 1.25)],
]
FUNCTION_ROW = [("Esc", 37.0)] + [(f"F{n}", x) for n, x in zip(range(1, 13), [70, 90, 110, 130, 165, 185, 205, 225, 259, 279, 299, 319])]
# The HID usage of each cap that has a label of its own (the keyboard is the French ISO one drawn here, so the labels are those of
# an AZERTY keyboard; the usages are those of the physical keys, as the calibration recorded them). The caps whose label is not
# unique (the two Shift keys, the two parts of Enter, the space bar) carry their usage in MAIN_ROWS. The ISO "*" key carries the
# identity 0x31, because its code is the one of the ANSI backslash (see the keymap file). Fn and Copilot have identities of this
# project (FnKey = 0xFF01, CopilotKey = 0xFF02 in lib/keyboard/include/galleon/keyboard/KeyIdentity.h).
LABEL_USAGE = {
    "Esc": 0x29, **{f"F{n}": 0x39 + n for n in range(1, 13)},
    "²": 0x35, "&": 0x1E, "é": 0x1F, '"': 0x20, "'": 0x21, "(": 0x22, "-": 0x23, "è": 0x24, "_": 0x25, "ç": 0x26, "à": 0x27,
    ")": 0x2D, "=": 0x2E, "⌫": 0x2A,
    "⇥": 0x2B, "A": 0x14, "Z": 0x1A, "E": 0x08, "R": 0x15, "T": 0x17, "Y": 0x1C, "U": 0x18, "I": 0x0C, "O": 0x12, "P": 0x13,
    "^": 0x2F, "$": 0x30,
    "⇪": 0x39, "Q": 0x04, "S": 0x16, "D": 0x07, "F": 0x09, "G": 0x0A, "H": 0x0B, "J": 0x0D, "K": 0x0E, "L": 0x0F, "M": 0x33,
    "ù": 0x34, "*": 0x31,
    "<": 0x64, "W": 0x1D, "X": 0x1B, "C": 0x06, "V": 0x19, "B": 0x05, "N": 0x11, ",": 0x10, ";": 0x36, ":": 0x37, "!": 0x38,
    "Ctrl": 0xE0, "❖": 0xE3, "Alt": 0xE2, "AltGr": 0xE6, "Fn": 0xFF01, "☰": 0x65, "◉": 0xFF02,
    "PrtSc": 0x46, "ScrLk": 0x47, "Pause": 0x48, "Ins": 0x49, "Home": 0x4A, "PgUp": 0x4B, "Del": 0x4C, "End": 0x4D, "PgDn": 0x4E,
    "↑": 0x52, "←": 0x50, "↓": 0x51, "→": 0x4F,
}
STRIP_SECTIONS = 7  # LEDs 139 to 145 of the live frame, from the left

NAV_ROWS = [
    (ROW_Y[0], ["PrtSc", "ScrLk", "Pause"]),
    (ROW_Y[1], ["Ins", "Home", "PgUp"]),
    (ROW_Y[2], ["Del", "End", "PgDn"]),
]
ARROWS = [(ROW_Y[4], [None, "↑", None]), (ROW_Y[5], ["←", "↓", "→"])]

# The module (estimates are marked): all in mm, in the keyboard's frame.
MODULE_CENTRE_X = 440.0     # ESTIMATE
DIALS_Y = 23.7              # ESTIMATE
SCREEN_TOP = DIALS_Y + 13.8 # ESTIMATE (the gap between the dials and the screen)
STRIP_Y, STRIP_H = 14.0, 5.5  # ESTIMATE: the light bar above the function row (not the lamps 94..107, whose use is unknown)
BODY = {"x": 17.0, "y": 5.0, "w": 465.0, "h": 154.0, "radius": 6.0}  # ESTIMATE of the outline


def read_lamps(path):
    lamps = {}
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle):
            lamps[int(row["Id"])] = (int(row["x_um"]) / 1000.0, int(row["y_um"]) / 1000.0)
    return lamps


def key_pitch(lamps):
    """The distance between two neighbouring keys, from the lamps themselves (median over the number row): the
    standard pitch is 19.05 mm but this keyboard's lamps are 20.1 mm apart, and the caps must follow the lamps."""
    xs = [lamps[i][0] for i in NUMBER_ROW]
    steps = sorted(b - a for a, b in zip(xs, xs[1:]))
    return steps[len(steps) // 2]


def cap(label, centre_x, centre_y, width_units, unit, usage=None):
    width = width_units * unit - GAP
    usage = LABEL_USAGE.get(label) if usage is None else usage
    if usage is None:
        raise ValueError(f"the cap {label!r} has no HID usage: add it to LABEL_USAGE or to MAIN_ROWS")
    return {"label": label, "usage": usage, "x": round(centre_x - width / 2, 2), "y": round(centre_y - (unit - GAP) / 2, 2),
            "w": round(width, 2), "h": round(unit - GAP, 2)}


def read_keymap():
    """The calibration (LED -> what it lights), as data/devices/galleon-100-sd-keymap.json holds it."""
    return {entry["led"]: entry for entry in json.loads(KEYMAP.read_text())["leds"]}


def build_leds(caps, keyboard):
    """Gives every cap its LED, and every LED that lights a key or a section of the light strip its position.

    The caps and the calibration have to agree, and a disagreement stops the build: every cap has a HID usage whose LED is in the
    calibration (except none), and every key of the calibration has a cap. A key may have several caps (the two parts of Enter)
    and several lamps (the space bar has five): its LED then sits at the mean of the measured lamps of all its caps."""
    keymap = read_keymap()
    led_of_usage = {e["key"]: led for led, e in keymap.items() if e["kind"] == "key"}
    for c in caps:
        led = led_of_usage.get(c["usage"])
        if led is None:
            raise ValueError(f"the cap {c['label']!r} (usage {c['usage']:#x}) has no LED in {KEYMAP.name}")
        c["led"] = led
    without_cap = sorted(set(led_of_usage) - {c["usage"] for c in caps})
    if without_cap:
        raise ValueError(f"keys of {KEYMAP.name} with no cap in the drawing: {[hex(u) for u in without_cap]}")
    leds = []
    for usage, led in sorted(led_of_usage.items(), key=lambda item: item[1]):
        lamps = [lamp for c in caps if c["usage"] == usage for lamp in c["lamps"]]
        if not lamps:
            raise ValueError(f"the key {usage:#x} (LED {led}) has no measured lamp")
        x = sum(keyboard[lamp][0] for lamp in lamps) / len(lamps)
        y = sum(keyboard[lamp][1] for lamp in lamps) / len(lamps)
        leds.append({"led": led, "kind": "key", "usage": usage, "x": round(x, 2), "y": round(y, 2), "lamps": sorted(lamps)})
    first_cap = min(c["x"] for c in caps)
    last_cap = max(c["x"] + c["w"] for c in caps)
    sections = sorted((e["position"], led) for led, e in keymap.items() if e["kind"] == "strip")
    if [position for position, _ in sections] != list(range(1, STRIP_SECTIONS + 1)):
        raise ValueError(f"the calibration has to give the strip sections 1 to {STRIP_SECTIONS}: {sections}")
    width = (last_cap - first_cap) / STRIP_SECTIONS
    for position, led in sections:
        leds.append({"led": led, "kind": "strip", "position": position,
                     "x": round(first_cap + (position - 0.5) * width, 2), "y": round(STRIP_Y + STRIP_H / 2, 2)})
    return sorted(leds, key=lambda entry: entry["led"])


def build():
    keyboard = read_lamps(LAMPS_KEYBOARD)
    unit = key_pitch(keyboard)
    left = keyboard[0][0] - unit / 2  # Esc is one unit wide and its lamp is at its centre
    module_lamps = read_lamps(LAMPS_MODULE)
    device = json.loads(DEVICE.read_text())

    caps = []
    for label, x in FUNCTION_ROW:
        caps.append(cap(label, x, ROW_Y[0], 1, unit))
    for row_index in range(1, 6):
        x = left
        for label, units, *usage in MAIN_ROWS[row_index]:
            caps.append(cap(label, x + units * unit / 2, ROW_Y[row_index], units, unit, usage[0] if usage else None))
            x += units * unit
    for y, labels in NAV_ROWS:
        for label, x in zip(labels, NAV_X):
            caps.append(cap(label, x, y, 1, unit))
    for y, labels in ARROWS:
        for label, x in zip(labels, NAV_X):
            if label:
                caps.append(cap(label, x, y, 1, unit))

    cells = [{"id": i, "x": round(keyboard[i][0], 2), "y": round(keyboard[i][1], 2)} for i in range(LAST_KEY_LAMP + 1)]
    # Every lamp goes to the cap it is nearest to (0 when it lies inside the cap); a cap may end with several lamps
    # (the space bar) or with none.
    for c in caps:
        c["lamps"] = []
    for cell in cells:
        def distance(c):
            dx = max(c["x"] - cell["x"], 0, cell["x"] - (c["x"] + c["w"]))
            dy = max(c["y"] - cell["y"], 0, cell["y"] - (c["y"] + c["h"]))
            centre = abs(cell["x"] - (c["x"] + c["w"] / 2)) + abs(cell["y"] - (c["y"] + c["h"] / 2))
            return (dx * dx + dy * dy, centre)
        min(caps, key=distance)["lamps"].append(cell["id"])

    leds = build_leds(caps, keyboard)

    keys = device["keys"]
    pitch = keys["pitchMm"]
    key_size = keys["keyWidthMm"]
    screen_w = device["statusScreen"]["widthMm"]
    screen_h = device["statusScreen"]["heightMm"]
    gap_to_keys = device["statusScreen"]["gapToKeysMm"]
    grid_left = MODULE_CENTRE_X - 1.5 * pitch
    grid_top = SCREEN_TOP + screen_h + gap_to_keys
    module_keys = []
    for row in range(keys["rows"]):
        for column in range(keys["columns"]):
            module_keys.append({"index": row * keys["columns"] + column,
                                "x": round(grid_left + column * pitch + (pitch - key_size) / 2, 2),
                                "y": round(grid_top + row * pitch + (pitch - key_size) / 2, 2),
                                "size": key_size})
    dial_spacing = abs(module_lamps[1][0] - module_lamps[0][0])
    module = {
        "screen": {"x": round(MODULE_CENTRE_X - screen_w / 2, 2), "y": round(SCREEN_TOP, 2), "w": screen_w, "h": screen_h},
        "keys": module_keys,
        "dials": [{"x": round(MODULE_CENTRE_X - dial_spacing / 2, 2), "y": DIALS_Y, "r": 7.0},
                  {"x": round(MODULE_CENTRE_X + dial_spacing / 2, 2), "y": DIALS_Y, "r": 7.0}],
    }
    first_cap = min(c["x"] for c in caps)
    last_cap = max(c["x"] + c["w"] for c in caps)
    strip = {"x": round(first_cap, 2), "y": STRIP_Y, "w": round(last_cap - first_cap, 2), "h": STRIP_H}
    return {
        "version": 1,
        "unit": "mm",
        "body": BODY,
        "strip": strip,
        "caps": caps,
        "cells": cells,
        "leds": leds,
        "module": module,
        "keyPitchMm": round(unit, 2),
        "notes": ["strip: the light bar above the keys, decoration only (its lamps are not the ones listed in cells)",
                  "cells: measured lamp positions (docs/protocol/lamps-2b0c.csv)",
                  "key pitch: the median distance between the lamps of the number row (the standard 19.05 mm does not fit them)",
                  "caps: standard French ISO tenkeyless layout, decoration, not measured",
                  "module: sizes measured (data/devices), position against the keyboard estimated"],
    }


def to_leds_json(data):
    return json.dumps({
        "version": 1,
        "unit": "mm",
        "device": "1b1c:2b0c",
        "source": "GENERATED by tools/keyboard-map/build_map.py from docs/protocol/lamps-2b0c.csv (lamp positions, measured 2026-10-18) and "
                  "data/devices/galleon-100-sd-keymap.json (which LED lights which key, calibration of 2026-11-15). A key's position is the "
                  "mean of the measured lamps of its caps; the seven light strip sections are placed along the drawn strip (an estimate, "
                  "not a measurement). LEDs that light nothing have no position. Do not edit: change the script and run it again.",
        "leds": data["leds"],
    }, indent=2, ensure_ascii=False) + "\n"


def to_js(data):
    return ("".join(f"// {line}\n" for line in SPDX_LINES) + "\n"
            "// GENERATED by tools/keyboard-map/build_map.py from the lamp positions and device data of this repository.\n"
            "// Do not edit: change the script or its data and run it again (a test fails when this file is out of date).\n"
            ".pragma library\n\n"
            "var map = " + json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def to_svg(data, backing):
    """The drawing with REAL transparency. The dark layer (the body, the caps) has holes cut by a mask: the legends,
    a thin outline round every cap and the light bar, the screen and the 12 keys of the module, and a ring round
    each dial. Whatever is behind the file shows through them: a colour, a picture, the keys of a theme.
    With `backing` False the file is only that layer (the holes are transparent, alpha 0); with True it also carries
    a white light under the caps and dark windows for the module, to be looked at on its own."""
    body = data["body"]
    margin = 4
    view = (body["x"] - margin, body["y"] - margin, body["w"] + 2 * margin, body["h"] + 2 * margin)
    rim = 0.55   # mm, the see-through outline of a cap
    ring = 0.9   # mm, the see-through ring round a dial
    caps = data["caps"]
    strip = data["strip"]
    m = data["module"]
    shapes = caps + [dict(strip, label="", lamps=[])]

    def rect(x, y, w, h, r):
        return f'<rect x="{round(x, 2)}" y="{round(y, 2)}" width="{round(w, 2)}" height="{round(h, 2)}" rx="{round(r, 2)}"/>'

    if backing:
        note = "  Preview: the dark layer over a white light and dark module windows."
    else:
        note = ("  Overlay: only the dark layer. The legends, the outlines, the module's screen and 12 keys and the rings of the\n"
                "  dials are real holes (alpha 0): put any colour or picture behind the file and it shows through.")
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        "<!--",
        *[f"  {line}" for line in SPDX_LINES],
        "  GENERATED by tools/keyboard-map/build_map.py from this repository's own measurements. Do not edit.",
        note,
        "  Caps are decoration (standard French layout); the lamps are at the measured positions (data of the app).",
        "-->",
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{view[0]} {view[1]} {view[2]} {view[3]}" width="{view[2] * 3}" height="{view[3] * 3}">',
        "  <defs>",
        f'    <mask id="holes" maskUnits="userSpaceOnUse" x="{view[0]}" y="{view[1]}" width="{view[2]}" height="{view[3]}">',
        "      <!-- white keeps the dark layer, black cuts it away -->",
        '      <g fill="white">',
        "        " + rect(body["x"], body["y"], body["w"], body["h"], body["radius"]),
        "      </g>",
        '      <g fill="black">',
    ]
    for c in shapes:  # the whole cap goes, then its dark body is put back smaller: what is left is the outline
        out.append("        " + rect(c["x"], c["y"], c["w"], c["h"], 1.6))
    out.append("      </g>")
    out.append('      <g fill="white">')
    for c in shapes:
        out.append("        " + rect(c["x"] + rim, c["y"] + rim, c["w"] - 2 * rim, c["h"] - 2 * rim, 1.2))
    out.append("      </g>")
    out.append('      <g fill="black" font-family="sans-serif" font-size="5.2" text-anchor="middle">')
    for c in caps:
        if c["label"]:
            out.append(f'        <text x="{round(c["x"] + c["w"] / 2, 2)}" y="{round(c["y"] + c["h"] / 2 + 1.8, 2)}">{escape(c["label"])}</text>')
    out.append("      </g>")
    out.append('      <g fill="black">  <!-- the module: its screen and its 12 keys are windows -->')
    s_ = m["screen"]
    out.append("        " + rect(s_["x"], s_["y"], s_["w"], s_["h"], 2))
    for k in m["keys"]:
        out.append("        " + rect(k["x"], k["y"], k["size"], k["size"], 1.8))
    for d in m["dials"]:
        out.append(f'        <circle cx="{d["x"]}" cy="{d["y"]}" r="{d["r"] + ring}"/>')
    out.append("      </g>")
    out.append('      <g fill="white">  <!-- the dials themselves are put back: only a ring round them is see-through -->')
    for d in m["dials"]:
        out.append(f'        <circle cx="{d["x"]}" cy="{d["y"]}" r="{d["r"]}"/>')
    out.append("      </g>")
    out.append("    </mask>")
    out.append("  </defs>")
    if backing:
        out.append("  <!-- behind: the light of the keys and the windows of the module -->")
        out.append('  <g fill="#e8eaed">')
        for c in shapes:
            out.append("    " + rect(c["x"], c["y"], c["w"], c["h"], 1.6))
        for d in m["dials"]:
            out.append(f'    <circle cx="{d["x"]}" cy="{d["y"]}" r="{d["r"] + ring}"/>')
        out.append("  </g>")
        out.append('  <g fill="#2a3a6a">')
        out.append("    " + rect(s_["x"], s_["y"], s_["w"], s_["h"], 2))
        for k in m["keys"]:
            out.append("    " + rect(k["x"], k["y"], k["size"], k["size"], 1.8))
        out.append("  </g>")
    out.append(f'  <rect x="{view[0]}" y="{view[1]}" width="{view[2]}" height="{view[3]}" fill="#1a1c20" mask="url(#holes)"/>')
    out.append("</svg>")
    return "\n".join(out) + "\n"


def main(argv):
    data = build()
    outputs = {OUT_JS: to_js(data), OUT_SVG: to_svg(data, True), OUT_OVERLAY: to_svg(data, False), OUT_LEDS: to_leds_json(data)}
    if "--check" in argv:
        stale = [str(path.relative_to(ROOT)) for path, text in outputs.items() if not path.exists() or path.read_text() != text]
        if stale:
            print("out of date, run tools/keyboard-map/build_map.py:", *stale)
            return 1
        return 0
    for path, text in outputs.items():
        path.write_text(text)
        print("wrote", path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
