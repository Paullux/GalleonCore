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


class LedsOfTheKeys(unittest.TestCase):
    """Every cap of the drawing knows its LED of the live frame, and every LED that lights a key or a section of the light strip knows
    where it is. The drawing and the calibration (data/devices/galleon-100-sd-keymap.json) have to agree."""

    def setUp(self) -> None:
        self.data = build_map.build()
        self.keymap = {e["led"]: e for e in json.loads(build_map.KEYMAP.read_text())["leds"]}
        self.caps = self.data["caps"]

    def cap(self, label: str, nth: int = 0) -> dict:
        return [c for c in self.caps if c["label"] == label][nth]

    def test_every_cap_has_a_usage_and_the_led_the_calibration_gives_it(self) -> None:
        for c in self.caps:
            with self.subTest(cap=c["label"], x=c["x"]):
                entry = self.keymap[c["led"]]
                self.assertEqual(entry["kind"], "key")
                self.assertEqual(entry["key"], c["usage"])

    def test_a_cap_is_found_by_where_the_key_is_not_by_the_letter_on_a_qwerty(self) -> None:
        # The drawing is the French AZERTY: the key engraved Q lights LED 4 (the A of a QWERTY), the one engraved A lights LED 20.
        self.assertEqual((self.cap("Q")["usage"], self.cap("Q")["led"]), (0x04, 4))
        self.assertEqual((self.cap("A")["usage"], self.cap("A")["led"]), (0x14, 20))
        self.assertEqual(self.cap("M")["led"], 51)    # the key next to L is the semicolon of a QWERTY
        self.assertEqual(self.cap("*")["led"], 50)    # the ISO key next to Enter
        self.assertEqual(self.cap("Fn")["led"], 122)
        self.assertEqual(self.cap("◉")["led"], 129)   # Copilot

    def test_the_two_shift_keys_the_two_parts_of_enter_and_the_space_bar_are_told_apart(self) -> None:
        shifts = [c for c in self.caps if c["label"] == "⇧"]
        self.assertEqual(sorted(c["led"] for c in shifts), [106, 110])
        self.assertLess(min(shifts, key=lambda c: c["x"])["x"], 40)       # the left one is at the left edge
        enters = [c for c in self.caps if c["usage"] == 0x28]
        self.assertEqual(len(enters), 2)
        self.assertEqual({c["led"] for c in enters}, {40})
        space = [c for c in self.caps if c["usage"] == 0x2C]
        self.assertEqual([c["led"] for c in space], [44])

    def test_every_key_of_the_calibration_has_a_cap(self) -> None:
        keys = {e["key"] for e in self.keymap.values() if e["kind"] == "key"}
        self.assertEqual(keys, {c["usage"] for c in self.caps})

    def test_the_six_extra_lamps_belong_to_enter_right_shift_and_the_space_bar(self) -> None:
        # 94 lamps for 88 keys: the space bar has 5 lamps, Enter 2 and the right Shift 2, each lit by one LED.
        lamps_per_led: dict[int, int] = {}
        for c in self.caps:
            lamps_per_led[c["led"]] = lamps_per_led.get(c["led"], 0) + len(c["lamps"])
        many = {led: n for led, n in lamps_per_led.items() if n > 1}
        self.assertEqual(many, {40: 2, 44: 5, 110: 2})
        self.assertEqual(sum(lamps_per_led.values()), 94)

    def test_a_cap_without_a_known_usage_stops_the_build(self) -> None:
        with self.assertRaises(ValueError):
            build_map.cap("?", 100.0, 50.0, 1, 20.0)

    def test_every_led_that_lights_something_has_a_position_and_the_others_have_none(self) -> None:
        lit = {led for led, e in self.keymap.items() if e["kind"] in ("key", "strip")}
        self.assertEqual({e["led"] for e in self.data["leds"]}, lit)
        self.assertEqual(len(lit), 88 + 7)

    def test_a_key_led_sits_on_the_mean_of_its_measured_lamps(self) -> None:
        lamps = measured_lamps()
        for e in self.data["leds"]:
            if e["kind"] != "key":
                continue
            with self.subTest(led=e["led"]):
                self.assertAlmostEqual(e["x"], sum(lamps[l][0] for l in e["lamps"]) / len(e["lamps"]), delta=0.01)
                self.assertAlmostEqual(e["y"], sum(lamps[l][1] for l in e["lamps"]) / len(e["lamps"]), delta=0.01)

    def test_a_key_led_is_inside_the_cap_it_belongs_to_or_close_to_it(self) -> None:
        pitch = self.data["keyPitchMm"]
        for e in self.data["leds"]:
            if e["kind"] != "key":
                continue
            caps = [c for c in self.caps if c["led"] == e["led"]]
            nearest = min(max(abs(e["x"] - (c["x"] + c["w"] / 2)) - c["w"] / 2, abs(e["y"] - (c["y"] + c["h"] / 2)) - c["h"] / 2) for c in caps)
            self.assertLess(nearest, pitch, f"LED {e['led']}")

    def test_the_strip_sections_run_from_left_to_right_inside_the_drawn_strip(self) -> None:
        strip = self.data["strip"]
        sections = sorted((e for e in self.data["leds"] if e["kind"] == "strip"), key=lambda e: e["position"])
        self.assertEqual([e["led"] for e in sections], list(range(139, 146)))
        xs = [e["x"] for e in sections]
        self.assertEqual(xs, sorted(xs))
        self.assertGreater(xs[0], strip["x"])
        self.assertLess(xs[-1], strip["x"] + strip["w"])
        self.assertTrue(all(strip["y"] <= e["y"] <= strip["y"] + strip["h"] for e in sections))

    def test_the_data_file_for_the_effects_is_the_same_list(self) -> None:
        shipped = json.loads(build_map.OUT_LEDS.read_text())
        self.assertEqual(shipped["leds"], self.data["leds"])
        self.assertEqual(shipped["unit"], "mm")


class LightStrip(unittest.TestCase):
    """The strip above the function row is seven equal sections, the first at the left end, the fourth in the middle and the seventh at
    the right end; each is lit by the LED the calibration found for its position."""

    def setUp(self) -> None:
        self.data = build_map.build()
        self.strip = self.data["strip"]
        self.sections = self.strip["sections"]

    def test_there_are_seven_sections_in_order(self) -> None:
        self.assertEqual([s["position"] for s in self.sections], list(range(1, 8)))
        self.assertEqual([s["led"] for s in self.sections], list(range(139, 146)))

    def test_the_sections_are_equal_and_regularly_spaced(self) -> None:
        widths = {s["w"] for s in self.sections}
        self.assertEqual(len(widths), 1)
        steps = [b["x"] - a["x"] for a, b in zip(self.sections, self.sections[1:])]
        self.assertLess(max(steps) - min(steps), 0.05, steps)  # equal up to the rounding of the data to 0.01 mm

    def test_the_first_is_at_the_left_end_the_fourth_in_the_middle_and_the_seventh_at_the_right_end(self) -> None:
        gap = build_map.STRIP_GAP
        self.assertAlmostEqual(self.sections[0]["x"], self.strip["x"] + gap / 2, delta=0.02)
        last = self.sections[-1]
        self.assertAlmostEqual(last["x"] + last["w"], self.strip["x"] + self.strip["w"] - gap / 2, delta=0.02)
        fourth = self.sections[3]
        self.assertAlmostEqual(fourth["x"] + fourth["w"] / 2, self.strip["x"] + self.strip["w"] / 2, delta=0.02)

    def test_each_section_is_centred_on_the_position_of_its_led(self) -> None:
        by_led = {e["led"]: e for e in self.data["leds"] if e["kind"] == "strip"}
        for s in self.sections:
            with self.subTest(position=s["position"]):
                self.assertAlmostEqual(s["x"] + s["w"] / 2, by_led[s["led"]]["x"], delta=0.02)

    def test_a_section_has_a_drawing_id_outside_the_real_lamps(self) -> None:
        lamps = [s["lamp"] for s in self.sections]
        self.assertEqual(lamps, list(range(1001, 1008)))
        self.assertTrue(all(lamp > 93 for lamp in lamps))   # the 94 real lamps are 0 to 93

    def test_the_sections_sit_above_the_function_row_and_inside_the_body(self) -> None:
        body = self.data["body"]
        top_of_keys = min(c["y"] for c in self.data["caps"])
        for s in self.sections:
            self.assertLess(s["y"] + s["h"], top_of_keys)
            self.assertGreaterEqual(s["x"], body["x"])
            self.assertLessEqual(s["x"] + s["w"], body["x"] + body["w"])
