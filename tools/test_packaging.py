# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Release source integrity: each package must build the same source bytes."""
import hashlib
import os
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class DistributionSources(unittest.TestCase):
    def test_debian_tests_get_a_writable_home_and_remove_it_after_failure(self):
        # Launchpad deliberately provides a nonexistent, unwritable HOME.
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / 'obj-test').mkdir()
            commands = work / 'bin'
            commands.mkdir()
            ctest = commands / 'ctest'
            ctest.write_text('#!/bin/sh\nset -eu\nprintf "%s" "$HOME" > observed-home\n'
                             'touch "$HOME/test-write"\nexit 7\n')
            ctest.chmod(0o755)
            env = dict(os.environ, HOME='/sbuild-nonexistent',
                       PATH=str(commands) + os.pathsep + os.environ['PATH'])
            result = subprocess.run(['make', '-f', str(ROOT / 'tools/packaging/debian/rules'),
                                     'override_dh_auto_test'], cwd=work, env=env,
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            observed = Path((work / 'observed-home').read_text())
            self.assertNotEqual(str(observed), env['HOME'])
            self.assertFalse(observed.exists(), 'The temporary test home must be removed')

    def test_sources_are_repeatable_and_aur_checksums_match(self):
        # A minimal checkout also works in tools/check.py's clean copy, without .git.
        with tempfile.TemporaryDirectory() as tmp:
            checkout = Path(tmp)
            for name in ('.gitignore', 'CMakeLists.txt', 'lib/src/Protocol.cpp',
                         'tools/packaging/prepare.py', 'tools/packaging/build-deb.sh',
                         'tools/packaging/debian/rules', 'tools/packaging/aur/PKGBUILD.in'):
                target = checkout / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / name, target)
            ignored = checkout / 'build-deb/private-file'
            ignored.parent.mkdir()
            ignored.write_text('Must never be shipped')
            subprocess.run(['git', 'init', '-q', str(checkout)], check=True)
            outputs = [checkout / 'build-deb' / name for name in ('first', 'second')]
            for output in outputs:
                subprocess.run(['python3', '-B', str(checkout / 'tools/packaging/prepare.py'),
                                str(output)], check=True, capture_output=True)
            first = next(outputs[0].glob('*.tar.gz'))
            second = next(outputs[1].glob('*.tar.gz'))
            self.assertEqual(first.read_bytes(), second.read_bytes())
            digest = hashlib.sha256(first.read_bytes()).hexdigest()
            self.assertIn(digest, (outputs[0] / 'PKGBUILD').read_text())
            self.assertNotIn('@VERSION@', (outputs[0] / 'PKGBUILD').read_text())
            with tarfile.open(first) as archive:
                paths = [Path(member.name) for member in archive.getmembers()]
            relative = {str(Path(*path.parts[1:])) for path in paths}
            self.assertIn('tools/packaging/build-deb.sh', relative)
            self.assertIn('lib/src/Protocol.cpp', relative)
            self.assertIn('tools/packaging/debian/rules', relative)
            self.assertFalse(any('.git' in path.parts for path in paths))
            self.assertFalse(any('build-deb' in path.parts for path in paths))



class LiveLightingWatchdogIsShipped(unittest.TestCase):
    """galleond refuses the live lighting unless galleon-keyboard-watchdog is installed beside it, so every package
    must carry it, and nothing may kill it together with galleond."""

    def read(self, path):
        return (ROOT / path).read_text(encoding='utf-8-sig')

    def test_cmake_builds_it_with_the_desktop_and_installs_it_in_the_folder_of_galleond(self):
        top = self.read('CMakeLists.txt')
        self.assertIn('option(GALLEON_BUILD_KEYBOARD_WATCHDOG', top)
        self.assertIn('${GALLEON_BUILD_DESKTOP})', top.split('option(GALLEON_BUILD_KEYBOARD_WATCHDOG', 1)[1].split('\n', 1)[0])
        tool = self.read('tools/keyboard-watchdog/CMakeLists.txt')
        self.assertIn('install(TARGETS galleon-keyboard-watchdog RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})', tool)
        self.assertIn('install(TARGETS galleond RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})', self.read('daemon/CMakeLists.txt'))

    def test_galleond_looks_for_it_beside_its_own_executable(self):
        self.assertIn('applicationDirPath()', self.read('daemon/ProcessWatchdog.cpp'))

    def test_the_rpm_lists_it(self):
        self.assertIn('%{_bindir}/galleon-keyboard-watchdog', self.read('tools/packaging/copr/galleoncore.spec'))

    def test_the_appimage_build_fails_if_it_is_missing(self):
        self.assertIn('test -x "$work/AppDir/usr/bin/galleon-keyboard-watchdog"', self.read('tools/packaging/appimage/build.sh'))

    def test_the_windows_setup_test_checks_it_after_install_and_after_upgrade_and_that_uninstall_removes_it(self):
        setup = self.read('tools/packaging/windows/test-setup.ps1')
        self.assertEqual(setup.count('Assert-Watchdog\n'), 2)  # after the install, after the upgrade
        self.assertIn("bin/galleon-keyboard-watchdog.exe", setup.split('$markers = ', 1)[1].split('\n', 1)[0])

    def test_the_watchdog_outlives_galleond_in_every_user_service(self):
        # With the default KillMode=control-group systemd kills every process of the service when galleond stops or
        # crashes, the watchdog included, at the very moment it is needed.
        for path in ('tools/systemd/galleond.service', 'tools/systemd/galleond-dev.service', 'tools/packaging/appimage/install-user-service.sh'):
            with self.subTest(path=path):
                text = self.read(path)
                lines = [line.strip() for line in text.splitlines() if not line.lstrip().startswith('#')]
                self.assertIn('KillMode=process', lines)
                self.assertNotIn('KillMode=control-group', lines)
                self.assertNotIn('KillMode=mixed', lines)



class InstalledProgramsFindTheirLibrary(unittest.TestCase):
    """A program installed in a prefix of the user's (~/.local, /opt/...) must find libgalleon by itself; the packages,
    which install under /usr, must not get a RUNPATH."""

    def test_the_install_runpath_is_relative_and_set_only_outside_usr(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        block = cmake.split('if(UNIX AND NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr")', 1)[1].split('endif()', 1)[0]
        self.assertIn('\\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}', block)
        self.assertNotIn('CMAKE_INSTALL_PREFIX}', block, 'the RUNPATH must not depend on the prefix: it is relative to the program')

    def test_every_package_installs_under_usr(self):
        for path in ('tools/packaging/appimage/build.sh', 'tools/packaging/aur/PKGBUILD.in'):
            self.assertIn('-DCMAKE_INSTALL_PREFIX=/usr', (ROOT / path).read_text(encoding='utf-8'), path)


if __name__ == '__main__':
    unittest.main()
