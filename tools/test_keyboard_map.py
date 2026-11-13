#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests of tools/keyboard-map/build_map.py: the generated files are up to date, they are faithful to the measured
lamps, and the see-through drawing really has holes.
Run with: python3 -m unittest discover -s tools -p 'test_*.py'"""

import csv
import importlib.util
import json
import unittest
import xml.dom.minidom
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("build_map", ROOT / "tools/keyboard-map/build_map.py")
build_map = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build_map)


def measured_lamps():
    with open(ROOT / "docs/protocol/lamps-2b0c.csv", newline="") as handle:
        return {int(r["Id"]): (int(r["x_um"]) / 1000.0, int(r["y_um"]) / 1000.0) for r in csv.DictReader(handle)}


class GeneratedFiles(unittest.TestCase):
    def test_the_committed_files_are_what_the_script_makes_today(self) -> None:
        self.assertEqual(build_map.main(["--check"]), 0, "run python3 tools/keyboard-map/build_map.py and commit the result")

    def test_the_data_file_holds_the_same_map_as_the_script(self) -> None:
        text = build_map.OUT_JS.read_text()
        self.assertEqual(json.loads(text.split("var map = ", 1)[1]), build_map.build())

    def test_both_drawings_are_well_formed_xml(self) -> None:
        for path in (build_map.OUT_SVG, build_map.OUT_OVERLAY):
            with self.subTest(path=path.name):
                xml.dom.minidom.parse(str(path))


class FaithfulToTheMeasurements(unittest.TestCase):
    def setUp(self) -> None:
        self.data = build_map.build()

    def test_every_key_lamp_is_there_at_its_measured_place(self) -> None:
        lamps = measured_lamps()
        cells = {cell["id"]: cell for cell in self.data["cells"]}
        self.assertEqual(sorted(cells), list(range(94)))
        for lamp_id, cell in cells.items():
            self.assertAlmostEqual(cell["x"], lamps[lamp_id][0], delta=0.01)
            self.assertAlmostEqual(cell["y"], lamps[lamp_id][1], delta=0.01)

    def test_the_lamps_that_are_not_keys_are_not_drawn(self) -> None:
        # Lamps 94..107 sit at Y = 0, which may be a placeholder, and nothing lit when they were tried.
        self.assertTrue(all(cell["id"] <= 93 for cell in self.data["cells"]))

    def test_every_lamp_belongs_to_exactly_one_cap(self) -> None:
        owned = [lamp for cap in self.data["caps"] for lamp in cap["lamps"]]
        self.assertEqual(sorted(owned), list(range(94)))

    def test_a_lamp_goes_to_a_cap_it_is_close_to(self) -> None:
        # An LED is a few millimetres off the centre of its key, never a whole key away.
        by_id = {cell["id"]: cell for cell in self.data["cells"]}
        pitch = self.data["keyPitchMm"]
        for cap in self.data["caps"]:
            centre = (cap["x"] + cap["w"] / 2, cap["y"] + cap["h"] / 2)
            for lamp in cap["lamps"]:
                cell = by_id[lamp]
                self.assertLess(abs(cell["y"] - centre[1]), pitch, f"lamp {lamp} under {cap['label']!r}")
                self.assertLess(abs(cell["x"] - centre[0]), cap["w"] / 2 + pitch / 2, f"lamp {lamp} under {cap['label']!r}")

    def test_the_key_pitch_comes_from_the_lamps_not_from_the_standard(self) -> None:
        self.assertAlmostEqual(self.data["keyPitchMm"], 20.0, delta=0.5)
        self.assertNotAlmostEqual(self.data["keyPitchMm"], 19.05, delta=0.5)

    def test_no_two_caps_overlap(self) -> None:
        caps = self.data["caps"]
        for i, a in enumerate(caps):
            for b in caps[i + 1:]:
                overlap_x = min(a["x"] + a["w"], b["x"] + b["w"]) - max(a["x"], b["x"])
                overlap_y = min(a["y"] + a["h"], b["y"] + b["h"]) - max(a["y"], b["y"])
                self.assertFalse(overlap_x > 0.01 and overlap_y > 0.01, f"{a['label']!r} overlaps {b['label']!r}")

    def test_everything_is_inside_the_body(self) -> None:
        body = self.data["body"]
        shapes = list(self.data["caps"]) + [self.data["strip"], self.data["module"]["screen"]]
        shapes += [{"x": k["x"], "y": k["y"], "w": k["size"], "h": k["size"]} for k in self.data["module"]["keys"]]
        for shape in shapes:
            self.assertGreaterEqual(shape["x"], body["x"])
            self.assertGreaterEqual(shape["y"], body["y"])
            self.assertLessEqual(shape["x"] + shape["w"], body["x"] + body["w"])
            self.assertLessEqual(shape["y"] + shape["h"], body["y"] + body["h"])

    def test_the_module_has_twelve_keys_in_three_columns_at_the_measured_pitch(self) -> None:
        keys = self.data["module"]["keys"]
        self.assertEqual(len(keys), 12)
        xs = sorted({k["x"] for k in keys})
        device = json.loads((ROOT / "data/devices/galleon-100-sd.json").read_text())
        self.assertEqual(len(xs), device["keys"]["columns"])
        self.assertAlmostEqual(xs[1] - xs[0], device["keys"]["pitchMm"], delta=0.01)
        self.assertAlmostEqual(self.data["module"]["screen"]["w"], device["statusScreen"]["widthMm"], delta=0.01)


class SeeThrough(unittest.TestCase):
    def test_the_overlay_has_holes_and_no_light_behind_it(self) -> None:
        text = build_map.OUT_OVERLAY.read_text()
        self.assertIn('<mask id="holes"', text)
        self.assertIn('mask="url(#holes)"', text)
        self.assertNotIn("#e8eaed", text, "the light belongs to the preview, not to the overlay")
        self.assertNotIn("#2a3a6a", text)

    def test_the_preview_has_the_light_behind_the_dark_layer(self) -> None:
        text = build_map.OUT_SVG.read_text()
        self.assertIn("#e8eaed", text)
        self.assertLess(text.index("#e8eaed"), text.index('mask="url(#holes)"'), "the light must be drawn first, under the dark layer")

    def test_the_legends_and_the_module_windows_are_cut_out(self) -> None:
        text = build_map.OUT_OVERLAY.read_text()
        mask = text[text.index('<mask id="holes"'):text.index("</mask>")]
        data = build_map.build()
        for cap in data["caps"]:
            if cap["label"]:
                self.assertIn(">" + build_map.escape(cap["label"]) + "</text>", mask)
        # Three groups of cuts: the caps (before the dark bodies are put back smaller, which leaves the outlines), the
        # legends, and the module (its screen, its 12 keys and the rings round the dials).
        self.assertEqual(mask.count('fill="black"'), 3)
        module = mask[mask.index("<!-- white keeps") :]
        self.assertEqual(module.count("<circle"), 4, "each dial: the ring cut, then the dial put back")

    def test_labels_with_xml_characters_are_escaped(self) -> None:
        text = build_map.OUT_OVERLAY.read_text()
        self.assertIn("&amp;", text)
        self.assertIn("&lt;", text)


if __name__ == "__main__":
    unittest.main()
