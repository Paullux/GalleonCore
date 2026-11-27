# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""The examples of galleon-key-signal put the colour in double quotes.

A bare `#` starts a comment in bash and in PowerShell: `--color #ff7800` is cut at the `#` and the tool answers "--color needs a value". Double quotes work in bash,
PowerShell and cmd.exe (single quotes do not in cmd.exe). The examples shown to people (the help of the tool, the documentation, the website, the Windows recipe) must
therefore write `--color "#ff7800"`.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = ['tools/key-signal/KeySignalArgs.cpp', 'docs/KEY_SIGNALS.md', 'docs/WINDOWS_ACCEPTANCE.md', 'site/index.html', 'site/key-signals.html']
BARE = re.compile(r'--color\s+#[0-9a-fA-F]{6}\b')
QUOTED = re.compile(r'--color\s+(?:\\)?"#[0-9a-fA-F]{6}(?:\\)?"')


def bare_colours(text):
    """The places where a colour that is a real value (six hex digits) follows --color without quotes."""
    return BARE.findall(text)


class KeySignalExampleTests(unittest.TestCase):
    def test_no_example_gives_the_colour_without_quotes(self):
        for name in FILES:
            text = (ROOT / name).read_text(encoding='utf-8')
            self.assertEqual(bare_colours(text), [], f'{name}: put the colour in double quotes')

    def test_the_examples_are_there_and_quoted(self):
        for name in ('docs/KEY_SIGNALS.md', 'site/key-signals.html', 'tools/key-signal/KeySignalArgs.cpp'):
            text = (ROOT / name).read_text(encoding='utf-8')
            self.assertTrue(QUOTED.search(text), f'{name}: no quoted example any more')

    def test_the_four_languages_of_the_pipe_page_say_why(self):
        html = (ROOT / 'site/key-signals.html').read_text(encoding='utf-8')
        for sentence in ('Put the colour in double quotes', 'Mets la couleur entre guillemets doubles',
                         'Setze die Farbe in doppelte Anführungszeichen', 'Pon el color entre comillas dobles'):
            self.assertIn(sentence, html)

    def test_the_check_itself_finds_a_bare_colour_and_lets_a_quoted_or_a_pattern_through(self):
        self.assertEqual(bare_colours('galleon-key-signal --key F3 --color #ff7800 --seconds 60'), ['--color #ff7800'])
        self.assertEqual(bare_colours('--color   #E53935'), ['--color   #E53935'])
        self.assertEqual(bare_colours('galleon-key-signal --key F3 --color "#ff7800" --seconds 60'), [])
        self.assertEqual(bare_colours('| `--color #rrggbb` | its colour |'), [])  # the name of the option, not an example
        self.assertEqual(bare_colours('--color #ff78'), [])                       # not a colour


if __name__ == '__main__':
    unittest.main()
