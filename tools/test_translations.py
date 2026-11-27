# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check shipped UI catalogs without requiring Qt Linguist at build time."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


OFFICIAL = ('en', 'fr', 'de', 'es')
PLACEHOLDER = re.compile(r'%(?:[1-9][0-9]*|n)')


def problems(english, catalog, complete):
    """What is wrong with `catalog` next to the English one, as a list of messages (empty when it is fine).

    An official language is complete: it has exactly the keys of English. A community language may be a subset of them: a missing
    key is shown in English, which is the source text itself. In both cases no key may be absent from English (it would never be
    looked up), every value is a non-empty string and the `%1`/`%n` arguments of the source are all there.
    """
    found = []
    if complete:
        found += [f'missing: {source}' for source in english if source not in catalog]
    found += [f'unknown source text: {source}' for source in catalog if source not in english]
    for source, translated in catalog.items():
        if source not in english:
            continue
        if not (isinstance(translated, str) and translated.strip()):
            found.append(f'empty: {source}')
        elif sorted(PLACEHOLDER.findall(source)) != sorted(PLACEHOLDER.findall(translated)):
            found.append(f'arguments differ: {source}')
    return found


def load(path):
    pairs = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=list)
    if len(pairs) != len(dict(pairs)):
        raise AssertionError(f'{path.name}: duplicate source text')
    return dict(pairs)


class TranslationTests(unittest.TestCase):
    def test_official_catalogs_are_complete_and_keep_format_arguments(self):
        directory = ROOT / 'bosun' / 'translations'
        english = load(directory / 'en.json')
        for language in OFFICIAL:
            self.assertEqual(problems(english, load(directory / (language + '.json')), True), [], language)

    def test_community_catalogs_may_be_partial_but_stay_valid(self):
        directory = ROOT / 'bosun' / 'translations'
        english = load(directory / 'en.json')
        for path in sorted(directory.glob('*.json')):
            if path.stem not in OFFICIAL:
                self.assertEqual(problems(english, load(path), False), [], path.name)

    def test_a_partial_catalog_is_accepted_only_as_a_community_language(self):
        english = {'Open': 'Open', 'Close %1': 'Close %1', 'Save': 'Save'}
        partial = {'Open': 'Ouvrir'}
        self.assertEqual(problems(english, partial, False), [])
        self.assertEqual(problems(english, {}, False), [])
        self.assertEqual(problems(english, partial, True), ['missing: Close %1', 'missing: Save'])

    def test_a_community_catalog_keeps_the_other_rules(self):
        english = {'Open': 'Open', 'Close %1': 'Close %1', 'Left %n': 'Left %n'}
        self.assertEqual(problems(english, {'Nope': 'x'}, False), ['unknown source text: Nope'])
        self.assertEqual(problems(english, {'Open': ' '}, False), ['empty: Open'])
        self.assertEqual(problems(english, {'Open': ''}, False), ['empty: Open'])
        self.assertEqual(problems(english, {'Open': 3}, False), ['empty: Open'])
        self.assertEqual(problems(english, {'Close %1': 'Fermer'}, False), ['arguments differ: Close %1'])
        self.assertEqual(problems(english, {'Close %1': 'Fermer %2'}, False), ['arguments differ: Close %1'])
        self.assertEqual(problems(english, {'Left %n': 'Reste'}, False), ['arguments differ: Left %n'])
        self.assertEqual(problems(english, {'Close %1': 'Fermer %1', 'Left %n': '%n restant'}, False), [])

    def test_every_static_bosun_translation_has_a_catalog_entry(self):
        catalog = json.loads((ROOT / 'bosun/translations/en.json').read_text(encoding='utf-8'))
        pattern = re.compile(r'\b(?:qsTr|tr|words|setStatus|fail)\(\s*("(?:[^"\\]|\\.)*")')
        for path in (ROOT / 'bosun').rglob('*'):
            if path.suffix not in ('.cpp', '.qml'):
                continue
            for match in pattern.finditer(path.read_text(encoding='utf-8')):
                source = json.loads(match.group(1))
                self.assertIn(source, catalog, f'{path}: {source}')
