# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests of tools/theme/build_theme.py."""
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from theme import build_theme

ROOT = Path(__file__).resolve().parents[1]


def make_folder(root):
    (root / 'icons').mkdir(parents=True)
    (root / 'theme.json').write_text(json.dumps({'formatVersion': 1}))
    (root / 'credits.txt').write_text('credits')
    (root / 'icons' / 'b.png').write_bytes(b'b')
    (root / 'icons' / 'a.png').write_bytes(b'a')
    return root


class BuildThemeTests(unittest.TestCase):
    def test_entries_are_stored_with_the_manifest_first_and_the_rest_sorted(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = make_folder(Path(tmp) / 'theme')
            output = Path(tmp) / 'out' / 'x.gc.theme'
            names = build_theme.build(folder, output)
            self.assertEqual(names, ['theme.json', 'credits.txt', 'icons/a.png', 'icons/b.png'])
            with zipfile.ZipFile(output) as archive:
                self.assertEqual([i.filename for i in archive.infolist()], names)
                self.assertTrue(all(i.compress_type == zipfile.ZIP_STORED for i in archive.infolist()))
                self.assertEqual(archive.read('icons/a.png'), b'a')

    def test_the_same_folder_gives_the_same_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = make_folder(Path(tmp) / 'theme')
            build_theme.build(folder, Path(tmp) / 'one.gc.theme')
            build_theme.build(folder, Path(tmp) / 'two.gc.theme')
            self.assertEqual((Path(tmp) / 'one.gc.theme').read_bytes(), (Path(tmp) / 'two.gc.theme').read_bytes())

    def test_a_folder_without_a_manifest_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit):
                build_theme.build(Path(tmp), Path(tmp) / 'x.gc.theme')

    def test_the_shipped_theme_is_packed_and_named_after_its_folder(self):
        folder = ROOT / 'themes' / 'voice-chat'
        manifest = json.loads((folder / 'theme.json').read_text())
        self.assertEqual(manifest['id'], folder.name)
        used = {k['icon'] for k in manifest['keys'] if isinstance(k.get('icon'), str) and '.' in k['icon']}
        self.assertEqual(used, {p.name for p in (folder / 'icons').glob('*.png')})

    def test_the_packed_file_next_to_the_shipped_theme_is_up_to_date(self):
        # themes/voice-chat.gc.theme is what people open; it must be what the folder packs to.
        with tempfile.TemporaryDirectory() as tmp:
            build_theme.build(ROOT / 'themes' / 'voice-chat', Path(tmp) / 'x.gc.theme')
            self.assertEqual((ROOT / 'themes' / 'voice-chat.gc.theme').read_bytes(), (Path(tmp) / 'x.gc.theme').read_bytes(),
                             'run: python3 tools/theme/build_theme.py themes/voice-chat themes/voice-chat.gc.theme')


if __name__ == '__main__':
    unittest.main()
