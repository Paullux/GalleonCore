# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check shipped UI catalogs without requiring Qt Linguist at build time."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class TranslationTests(unittest.TestCase):
    def test_all_catalogs_are_complete_and_keep_format_arguments(self):
        catalogs = {}
        for language in ('en', 'fr', 'de', 'es'):
            path = ROOT / 'bosun' / 'translations' / (language + '.json')
            pairs = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=list)
            self.assertEqual(len(pairs), len(dict(pairs)), f'{language}: duplicate source text')
            catalogs[language] = dict(pairs)
        for language, catalog in catalogs.items():
            self.assertEqual(catalog.keys(), catalogs['en'].keys(), language)
            for source, translated in catalog.items():
                with self.subTest(language=language, source=source):
                    self.assertTrue(isinstance(translated, str) and translated.strip())
                    self.assertEqual(sorted(re.findall(r'%(?:[1-9][0-9]*|n)', source)),
                                     sorted(re.findall(r'%(?:[1-9][0-9]*|n)', translated)))

    def test_every_static_bosun_translation_has_a_catalog_entry(self):
        catalog = json.loads((ROOT / 'bosun/translations/en.json').read_text(encoding='utf-8'))
        pattern = re.compile(r'\b(?:qsTr|tr|words|setStatus|fail)\(\s*("(?:[^"\\]|\\.)*")')
        for path in (ROOT / 'bosun').rglob('*'):
            if path.suffix not in ('.cpp', '.qml'):
                continue
            for match in pattern.finditer(path.read_text(encoding='utf-8')):
                source = json.loads(match.group(1))
                self.assertIn(source, catalog, f'{path}: {source}')
