# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""The home page says that its keyboard-lighting section is under construction, until v1.0.0 is published.

The marks are the elements of site/index.html that carry the attribute `data-under-construction` (a badge and a note, each in the four languages of the site).
ROADMAP.md asks for them to be removed the day v1.0.0 is published; this test makes that impossible to forget: once the version of CMakeLists.txt is 1.0.0 or
more, it fails until they are gone, and before that it fails if they are missing.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MARK = re.compile(r'<[^<>]*\bdata-under-construction\b[^<>]*>')
PHRASES = ('Under construction', 'En construction', 'Im Aufbau', 'En construcción')


def project_version():
    text = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
    match = re.search(r'project\(GalleonCore VERSION ([0-9]+)\.([0-9]+)\.([0-9]+)', text)
    assert match, 'the version of the project was not found in CMakeLists.txt'
    return tuple(int(part) for part in match.groups())


def marks(html):
    """The opening tags that carry the mark, without what the page's comments and its style sheet say about it."""
    html = re.sub(r'<!--.*?-->', '', html, flags=re.S)
    html = re.sub(r'<style\b.*?</style>', '', html, flags=re.S)
    return MARK.findall(html)


def problem(html, version):
    """What is wrong with the marks of the page for this version of the project, or None."""
    found = marks(html)
    if version >= (1, 0, 0):
        if found:
            return ('v1.0.0: remove the "under construction" marks (the elements with data-under-construction) from site/index.html, '
                    'as ROADMAP.md says')
        return None
    if len(found) < 2:
        return 'before v1.0.0 the lighting section carries its badge and its note ("under construction"): at least two elements with data-under-construction'
    for words in PHRASES:
        if words not in html:
            return f'"{words}" is missing: every language of the site has the mark'
    return None


MARKED = ('<span class="lx-uc" data-under-construction><span lang="en">Under construction</span><span lang="fr">En construction</span>'
          '<span lang="de">Im Aufbau</span><span lang="es">En construcción</span></span><p class="note" data-under-construction>note</p>')


class UnderConstructionTests(unittest.TestCase):
    def test_the_page_of_the_project_is_marked_while_the_version_is_below_1_0_0_and_unmarked_from_1_0_0(self):
        html = (ROOT / 'site/index.html').read_text(encoding='utf-8')
        self.assertIsNone(problem(html, project_version()))

    def test_before_1_0_0_a_page_without_the_marks_is_refused(self):
        self.assertIsNone(problem(MARKED, (0, 3, 0)))
        self.assertIsNotNone(problem('<p>nothing</p>', (0, 3, 0)))
        self.assertIsNotNone(problem(MARKED.replace('<p class="note" data-under-construction>note</p>', ''), (0, 3, 0)))  # the note is missing

    def test_before_1_0_0_a_mark_missing_in_one_language_is_refused(self):
        for words in PHRASES:
            self.assertIsNotNone(problem(MARKED.replace(words, 'x'), (0, 3, 0)), words)

    def test_from_1_0_0_a_page_that_still_has_a_mark_is_refused(self):
        self.assertIsNotNone(problem(MARKED, (1, 0, 0)))
        self.assertIsNotNone(problem(MARKED, (1, 2, 3)))
        self.assertIsNotNone(problem(MARKED, (2, 0, 0)))
        self.assertIsNotNone(problem('<p data-under-construction>x</p>', (1, 0, 0)))

    def test_from_1_0_0_a_page_without_the_marks_is_accepted(self):
        self.assertIsNone(problem('<p>nothing</p>', (1, 0, 0)))
        self.assertIsNone(problem('<p>nothing</p>', (1, 4, 0)))

    def test_the_last_version_below_1_0_0_still_wants_the_marks(self):
        self.assertIsNotNone(problem('<p>nothing</p>', (0, 99, 99)))
        self.assertIsNone(problem(MARKED, (0, 99, 99)))

    def test_a_comment_or_the_style_sheet_that_names_the_attribute_is_not_a_mark(self):
        page = '<!-- <p data-under-construction>x</p> --><style>p[data-under-construction] { color: red }</style><p class="note" data-under-construction>y</p>'
        self.assertEqual(marks(page), ['<p class="note" data-under-construction>'])
        self.assertEqual(marks('<p>nothing</p>'), [])
        self.assertEqual(marks('<!-- data-under-construction --><style>[data-under-construction]{}</style>'), [])


if __name__ == '__main__':
    unittest.main()
