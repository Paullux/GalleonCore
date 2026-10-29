# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
import unittest
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

    def test_other_release_assets_and_empty_links_do_not_unlock_publication(self):
        release = {'isDraft': False, 'isPrerelease': False, 'assets': [{'name': 'GalleonCore-0.2.2-x86_64.AppImage'}]}
        self.assertFalse(ready(self.html, '0.2.3', release))
        self.assertFalse(ready('', '0.2.3', release))
