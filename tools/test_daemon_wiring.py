# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

"""Source checks on daemon/main.cpp: what no unit test can see.

main() wires the daemon. A call put by mistake under a platform #if is compiled on one system and silently absent on the other: the
restore of the live lighting once landed inside the Windows-only block, so on Linux galleond never wrote its memory file, and every
test passed. These checks read the preprocessor structure of the file and fail for such a call.
"""

import pathlib
import re
import unittest

MAIN = pathlib.Path(__file__).resolve().parent.parent / "daemon" / "main.cpp"

# Calls that every platform must make.
EVERYWHERE = ("setUpLightingRestore(", "nightMode.resume(", "galleond::NightController nightMode(", "lighting.setNight(", "galleond::KeySignalsService keySignals(", "lighting.setSessionEndingCheck(")


def platform_conditions_around(lines, index):
    """The preprocessor conditions (as text) that enclose line `index`, outermost first. An #else keeps the condition it belongs to."""
    stack = []
    for line in lines[:index]:
        text = line.strip()
        if re.match(r"#\s*if(n?def)?\b", text):
            stack.append(text)
        elif re.match(r"#\s*elif\b", text) or re.match(r"#\s*else\b", text):
            if stack:
                stack[-1] += " / " + text
        elif re.match(r"#\s*endif\b", text):
            if stack:
                stack.pop()
    return stack


class TestDaemonWiring(unittest.TestCase):
    def setUp(self):
        self.lines = MAIN.read_text(encoding="utf-8").splitlines()

    def test_the_conditions_around_a_line_are_found(self):
        lines = ["a", "#ifdef Q_OS_WIN", "b", "#else", "c", "#endif", "d", "#ifdef X", "e", "#endif"]
        self.assertEqual(platform_conditions_around(lines, 0), [])
        self.assertEqual(platform_conditions_around(lines, 2), ["#ifdef Q_OS_WIN"])
        self.assertEqual(platform_conditions_around(lines, 4), ["#ifdef Q_OS_WIN / #else"])  # the else of a platform block is still under it
        self.assertEqual(platform_conditions_around(lines, 6), [])
        self.assertEqual(platform_conditions_around(lines, 8), ["#ifdef X"])

    def test_the_calls_every_platform_must_make_are_not_under_a_platform_condition(self):
        for call in EVERYWHERE:
            found = [i for i, line in enumerate(self.lines) if call in line and not line.strip().startswith("//")]
            self.assertTrue(found, f"daemon/main.cpp no longer calls {call}")
            for i in found:
                platform = [c for c in platform_conditions_around(self.lines, i) if "Q_OS_" in c]
                self.assertEqual(platform, [], f"daemon/main.cpp line {i + 1}: {call} is under a platform condition {platform}: it would be missing on the other system")

    def test_windows_reaches_the_key_signals_through_the_local_endpoint_and_only_there(self):
        calls = [i for i, line in enumerate(self.lines) if "connectKeySignals(" in line and not line.strip().startswith("//")]
        self.assertEqual(len(calls), 1, "daemon/main.cpp must connect the key signals to the local endpoint exactly once")
        self.assertTrue(any("Q_OS_WIN" in c for c in platform_conditions_around(self.lines, calls[0])), "the local endpoint is the way in on Windows only")

    def code_lines(self, needle):
        return [i for i, line in enumerate(self.lines) if needle in line and not line.strip().startswith("//")]

    def test_the_key_glow_is_started_by_its_driver_once_and_only_where_there_is_a_source_of_key_presses(self):
        # The glow reads the key presses (docs/KEY_GLOW.md): the driver is the one place that starts the source, so it is built exactly once, and only on Linux for now.
        made = self.code_lines("galleond::KeyGlowDriver keyGlow(")
        self.assertEqual(len(made), 1, "daemon/main.cpp must build the key glow driver exactly once")
        conditions = platform_conditions_around(self.lines, made[0])
        self.assertTrue(any(c.startswith("#ifndef Q_OS_WIN") for c in conditions), f"the source of key presses exists on Linux only: {conditions}")
        self.assertEqual(len(self.code_lines("galleond::HidrawKeyPressSource ")), 1, "one source of key presses")
        # The source is named twice only: where it is made and where the driver is given it. Nothing else of main() touches it, so nothing else starts it.
        self.assertEqual(len(self.code_lines("keyGlowSource")), 2, "only the driver may start the source of key presses")

    def test_the_driver_looks_again_when_the_lighting_goes_on_or_off_and_the_choice_is_the_file_bosun_writes(self):
        refresh = [i for i in self.code_lines("keyGlow.refresh()") if "activeChanged" in "".join(self.lines[max(0, i - 2) : i + 1])]
        self.assertTrue(refresh, "the glow must be looked at again whenever the lighting goes on or off")
        driver = self.code_lines("galleond::KeyGlowDriver keyGlow(")[0]
        self.assertIn("keyGlowAllowed(galleond::keyGlowAccessPath())", self.lines[driver], "the choice is read from key-glow.json, written by Bosun only")

    def test_the_lighting_trace_is_paused_while_the_keys_glow(self):
        paused = [i for i in self.code_lines("setPaused(watching)")]
        self.assertEqual(len(paused), 1, "the diagnostic trace must be paused while the key glow watches the keys")
        self.assertTrue(any("lightingTrace" in line for line in self.lines[paused[0] - 2 : paused[0] + 1]))


if __name__ == "__main__":
    unittest.main()
