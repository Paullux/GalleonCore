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
EVERYWHERE = ("setUpLightingRestore(", "nightMode.resume(", "galleond::NightController nightMode(")


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


if __name__ == "__main__":
    unittest.main()
