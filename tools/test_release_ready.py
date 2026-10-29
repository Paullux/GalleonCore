# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
import re
from pathlib import Path
from website.release_ready import ready


class ReleaseWebsiteTests(unittest.TestCase):
    html = '<a href="https://github.com/Paullux/GalleonCore/releases/download/v0.2.3/GalleonCore-0.2.3-x86_64.AppImage">AppImage</a><a href="https://github.com/Paullux/GalleonCore/releases/download/v0.2.3/GalleonCore-0.2.3-Windows-x86_64-Setup.exe">Setup</a>'

    def test_waits_for_all_linked_assets(self):
        release = {'isDraft': False, 'isPrerelease': False, 'assets': [{'name': 'GalleonCore-0.2.3-x86_64.AppImage'}]}
        self.assertFalse(ready(self.html, '0.2.3', release))
        release['assets'].append({'name': 'GalleonCore-0.2.3-Windows-x86_64-Setup.exe'})
        self.assertTrue(ready(self.html, '0.2.3', release))
        release['isDraft'] = True
        self.assertFalse(ready(self.html, '0.2.3', release))

    def test_debian_website_links_match_github_asset_normalization(self):
        root = Path(__file__).resolve().parents[1]
        version = re.search(r'project\(GalleonCore VERSION ([0-9.]+)', (root / 'CMakeLists.txt').read_text()).group(1)
        html = (root / 'site/index.html').read_text()
        for series in ('noble', 'resolute'):
            # GitHub's published v0.2.2 assets use a dot where dpkg's local
            # filenames have a tilde. Download links must use the published name.
            self.assertIn(f'galleoncore_{version}-1.{series}_amd64.deb', html)
            self.assertNotIn(f'galleoncore_{version}-1~{series}_amd64.deb', html)

    def test_other_release_assets_and_empty_links_do_not_unlock_publication(self):
        release = {'isDraft': False, 'isPrerelease': False, 'assets': [{'name': 'GalleonCore-0.2.2-x86_64.AppImage'}]}
        self.assertFalse(ready(self.html, '0.2.3', release))
        self.assertFalse(ready('', '0.2.3', release))
