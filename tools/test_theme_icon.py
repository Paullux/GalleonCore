# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""The file type and icon of `.gctheme` files: what the software installs on Linux and Windows."""
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True  # no __pycache__ in the tree: reuse lint would reject it
sys.path.insert(0, str(ROOT / 'tools/theme-icon'))
import build_icons  # noqa: E402

MIME_FILE = ROOT / 'bosun/mime/galleoncore-theme.xml'
ICON_DIR = ROOT / 'bosun/icons/theme-file'
NAMESPACE = '{http://www.freedesktop.org/standards/shared-mime-info}'
ICON_NAME = 'application-x-galleoncore-theme'


def mime_type():
    return ET.parse(MIME_FILE).getroot().find(f'{NAMESPACE}mime-type')


def ico_sizes(path):
    """The (width, height) of each image listed in an ICO directory (0 means 256)."""
    data = path.read_bytes()
    reserved, kind, count = struct.unpack_from('<HHH', data)
    assert (reserved, kind) == (0, 1), 'not an ICO file'
    entries = [struct.unpack_from('<BB', data, 6 + 16 * i) for i in range(count)]
    return sorted((w or 256, h or 256) for w, h in entries)


class LinuxFileType(unittest.TestCase):
    def test_mime_definition_is_a_theme_type_matching_gctheme_files(self):
        node = mime_type()
        self.assertEqual(node.get('type'), 'application/x-galleoncore-theme')
        self.assertEqual([g.get('pattern') for g in node.findall(f'{NAMESPACE}glob')], ['*.gctheme'])

    def test_mime_definition_is_a_kind_of_zip_archive(self):
        self.assertEqual([s.get('type') for s in mime_type().findall(f'{NAMESPACE}sub-class-of')],
                         ['application/zip'])

    def test_mime_comment_is_translated_into_french_german_and_spanish(self):
        languages = {c.get('{http://www.w3.org/XML/1998/namespace}lang') for c in mime_type().findall(f'{NAMESPACE}comment')}
        self.assertEqual(languages, {None, 'fr', 'de', 'es'})

    def test_mime_icon_name_is_the_one_the_packages_install(self):
        self.assertEqual(mime_type().find(f'{NAMESPACE}icon').get('name'), ICON_NAME)
        cmake = (ROOT / 'bosun/CMakeLists.txt').read_text()
        self.assertIn(f'RENAME {ICON_NAME}.png', cmake)
        self.assertIn('${size}x${size}/mimetypes', cmake)

    @unittest.skipUnless(shutil.which('update-mime-database'), 'shared-mime-info is not installed')
    def test_shared_mime_info_accepts_the_definition_and_maps_the_glob(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / 'packages').mkdir()
            shutil.copy(MIME_FILE, Path(tmp) / 'packages')
            result = subprocess.run(['update-mime-database', tmp], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('application/x-galleoncore-theme:*.gctheme', (Path(tmp) / 'globs2').read_text().replace('50:', ''))

    def test_every_hicolor_size_the_install_loops_over_exists_as_a_png(self):
        cmake = (ROOT / 'bosun/CMakeLists.txt').read_text()
        sizes = tuple(int(n) for n in re.search(r'foreach\(size ([\d ]+)\)', cmake).group(1).split())
        self.assertEqual(sizes, build_icons.PNG_SIZES)
        for size in sizes:
            png = ICON_DIR / build_icons.png_name(size)
            self.assertEqual(png.read_bytes()[:8], b'\x89PNG\r\n\x1a\n', png)
            width, height = struct.unpack('>II', png.read_bytes()[16:24])
            self.assertEqual((width, height), (size, size), png)

    def test_rpm_package_lists_the_mime_definition_and_every_icon(self):
        spec = (ROOT / 'tools/packaging/copr/galleoncore.spec').read_text()
        self.assertIn('%{_datadir}/mime/packages/galleoncore-theme.xml', spec)
        self.assertIn(f'%{{_datadir}}/icons/hicolor/*/mimetypes/{ICON_NAME}.png', spec)

    def test_desktop_entry_declares_no_mime_type_because_bosun_opens_no_argument_file(self):
        self.assertNotIn('MimeType=', (ROOT / 'bosun/galleon-bosun.desktop').read_text())
        self.assertNotIn('addPositionalArgument', (ROOT / 'bosun/main.cpp').read_text())


class WindowsFileType(unittest.TestCase):
    def test_ico_holds_every_size_windows_asks_for(self):
        self.assertEqual(ico_sizes(ICON_DIR / build_icons.ICO_NAME), [(s, s) for s in build_icons.ICO_SIZES])
        self.assertTrue({16, 32, 48, 256} <= set(build_icons.ICO_SIZES))

    def test_setup_registers_the_extension_for_the_current_user_only(self):
        script = (ROOT / 'tools/packaging/windows/galleoncore.nsi').read_text(encoding='utf-8-sig')
        keys = re.findall(r'(?:WriteReg\w+|DeleteReg\w+) (HK\w+) ([^\r\n]*Software\\Classes[^\r\n]*)', script)
        self.assertTrue(keys)
        self.assertEqual({hive for hive, _ in keys}, {'HKCU'})
        self.assertNotIn('HKLM', script)
        self.assertNotIn('shell\\open', script.lower())

    def test_setup_registers_and_removes_the_same_keys(self):
        script = (ROOT / 'tools/packaging/windows/galleoncore.nsi').read_text(encoding='utf-8-sig')
        install, uninstall = script.split('Section "Uninstall"')
        for key in ('Software\\Classes\\.gctheme', 'Software\\Classes\\GalleonCore.Theme'):
            self.assertIn(f'WriteRegStr HKCU "{key}"', install)
            self.assertIn(f'DeleteRegKey HKCU "{key}"', uninstall)
        self.assertIn('DefaultIcon', install)
        self.assertIn('SHChangeNotify', install)
        self.assertIn('SHChangeNotify', uninstall)
        self.assertIn(build_icons.ICO_NAME, script)

    def test_setup_icon_comes_from_the_same_file_the_linux_packages_are_built_from(self):
        script = (ROOT / 'tools/packaging/windows/galleoncore.nsi').read_text(encoding='utf-8-sig')
        self.assertIn('bosun\\icons\\theme-file\\gctheme.ico', script)


class IconBuilder(unittest.TestCase):
    def test_source_that_is_not_square_is_refused(self):
        with self.assertRaises(SystemExit):
            build_icons.check_source(1024, 768)

    def test_source_smaller_than_the_largest_size_is_refused(self):
        with self.assertRaises(SystemExit):
            build_icons.check_source(256, 256)

    def test_svg_renderers_are_given_a_square_output_of_the_master_size(self):
        for renderer in ('rsvg-convert', 'inkscape', 'magick', 'convert'):
            command = build_icons.svg_command(renderer, 'in.svg', 'out.png')
            self.assertIn(str(build_icons.SVG_RENDER_SIZE), ' '.join(command))

    def test_shipped_icons_come_from_the_shipped_source_picture(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest('Pillow is not installed')
        with tempfile.TemporaryDirectory() as tmp:
            build_icons.build(build_icons.DEFAULT_SOURCE, tmp)
            for name in [build_icons.png_name(s) for s in build_icons.PNG_SIZES] + [build_icons.ICO_NAME]:
                self.assertEqual((Path(tmp) / name).read_bytes(), (ICON_DIR / name).read_bytes(), name)


if __name__ == '__main__':
    unittest.main()
