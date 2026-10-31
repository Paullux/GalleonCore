# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Launcher fixtures only: never touch installed games or the user's home."""
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import fps_setup as fps

REAL_NATIVE_INSTALLED = fps.native_installed

STEAM = b'''// retain comments and casing
"UserLocalConfigStore" { "Software" { "Valve" { "Steam" { "apps" {
"730" { "LastPlayed" "123" "LaunchOptions" "" }
"440" { "LaunchOptions" "custom --args %command%" }
} } } } }
'''


class SetupTests(unittest.TestCase):
    def setUp(self):
        previous_umask = os.umask(0o022)
        self.addCleanup(os.umask, previous_umask)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        native = patch('fps_setup.native_installed', return_value=True)
        native.start()
        self.addCleanup(native.stop)
        self.home = Path(self.temp.name)
        self.setup = fps.Setup(self.home, self.home / '.config', self.home / '.local/share')

    def put(self, path, data):
        path = self.home / path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if isinstance(data, bytes) else data.encode())
        return path

    def scan(self):
        with patch('fps_setup.subprocess.run', side_effect=FileNotFoundError):
            return self.setup.scan()

    def apply(self, rows):
        with patch('fps_setup.subprocess.run', side_effect=FileNotFoundError), patch.object(self.setup, 'running', return_value=False):
            return self.setup.apply(rows)

    def test_reports_keep_kind_for_translated_labels(self):
        common = next(row for row in self.scan() if row['kind'] == 'common')
        result = self.apply([common])[0]
        self.assertEqual(result['kind'], 'common')
        self.assertEqual(result['label'], common['label'])
        self.assertEqual(result['status'], 'changed')

    def test_uninstalled_launchers_with_old_configs_are_hidden(self):
        self.put('.PlayOnLinux/shortcuts/OldGame', 'old shortcut')
        self.put('.local/share/bottles/bottles/old/bottle.yml', 'Name: Old\n')
        self.put('.var/app/com.usebottles.bottles/data/bottles/bottles/old/bottle.yml', 'Name: Old\n')
        self.put('.config/heroic/config.json', '{}')
        self.put('.cxoffice/Old/cxbottle.conf', '[Bottle]\n')
        with patch('fps_setup.native_installed', return_value=False):
            rows = self.scan()
        self.assertEqual([r['kind'] for r in rows], ['common'])

    def test_steam_account_zero_and_directory_aliases_do_not_duplicate_games(self):
        self.steam_fixture()
        self.put('.local/share/Steam/userdata/0/config/localconfig.vdf', STEAM)
        alias = self.home / '.steam/root'
        alias.parent.mkdir()
        alias.symlink_to(self.home / '.local/share/Steam', target_is_directory=True)
        rows = [r for r in self.scan() if r['kind'] == 'steam']
        self.assertEqual(len(rows), 2)
        self.assertFalse(any('/userdata/0/' in r['path'] for r in rows))
        self.assertEqual(len({r['id'] for r in rows}), 2)

    def test_repeated_flatpak_ids_are_listed_once_and_native_is_separate(self):
        self.put('.var/app/com.usebottles.bottles/data/bottles/bottles/game/bottle.yml', 'Name: Game\n')
        self.put('.local/share/bottles/bottles/old/bottle.yml', 'Name: Old\n')
        result = type('Result', (), dict(returncode=0, stdout=b'com.usebottles.bottles\ncom.usebottles.bottles\n'))()
        with patch('fps_setup.subprocess.run', return_value=result), patch('fps_setup.native_installed', return_value=False):
            rows = self.setup.scan()
        self.assertEqual([r['group'] for r in rows], ['Commun', 'Bottles · Flatpak', 'Permissions Flatpak'])
        self.setup.add('Another alias', 'duplicate', rows[1]['kind'], rows[1]['path'])
        self.assertEqual(len(self.setup.rows), 3)

    def test_native_appimage_desktop_is_recognized_but_not_flatpak_shortcut(self):
        binary = self.put('Heroic.AppImage', '#!/bin/sh\n')
        binary.chmod(0o755)
        desktop = self.put('.local/share/applications/heroic.desktop', '[Desktop Entry]\nExec="' + str(binary) + '" %U\n')
        def which(name):
            return str(binary) if name == str(binary) else None
        with patch('fps_setup.shutil.which', side_effect=which):
            self.assertTrue(REAL_NATIVE_INSTALLED('heroic', self.home, self.setup.data))
            desktop.write_text('[Desktop Entry]\nExec=flatpak run com.heroicgameslauncher.hgl\n')
            self.assertFalse(REAL_NATIVE_INSTALLED('heroic', self.home, self.setup.data))

    def test_crossover_configuration_preserves_unrelated_fields_and_restores(self):
        original = b'[Bottle]\n"Name"="Game"\n; user comment\n[EnvironmentVariables]\n"KEEP"="yes"\n"MANGOHUD"="0"\n[Other]\n"Value"="unchanged"\n'
        path = self.put('.cxoffice/Game/cxbottle.conf', original)
        results = self.apply(self.scan())
        result = next(r for r in results if r['label'].startswith('Game'))
        self.assertEqual(result['status'], 'changed')
        updated = path.read_bytes()
        self.assertIn(b'"KEEP"="yes"', updated)
        self.assertIn(b'[Other]\n"Value"="unchanged"', updated)
        self.assertIn(b'"MANGOHUD" = "1"', updated)
        self.assertEqual(updated, fps.patch_crossover(updated, self.setup.mango))
        with patch.object(self.setup, 'running', return_value=False):
            self.setup.restore(result['backup'])
        self.assertEqual(path.read_bytes(), original)

    def test_crossover_ambiguous_or_personal_configuration_is_preserved(self):
        for data in (b'[Bottle]\n[EnvironmentVariables]\n"MANGOHUD"="1"\nMANGOHUD=0\n',
                     b'[Bottle]\n[Bottle]\n', b'[EnvironmentVariables]\n'):
            with self.assertRaises(fps.SetupError):
                fps.patch_crossover(data, self.setup.mango)
        with self.assertRaises(fps.Preserved):
            fps.patch_crossover(b'[Bottle]\n[EnvironmentVariables]\n"MANGOHUD_CONFIG"="custom"\n', self.setup.mango)
        with self.assertRaises(fps.SetupError):
            fps.patch_crossover(b'[Bottle]\n', '/tmp/bad\npath')
        updated = fps.patch_crossover(b'[Bottle]\n"Name"="Game"', '/tmp/a"b/MangoHud.conf')
        self.assertIn(b'a\\"b', updated)

    def test_steam_shortcuts_delegating_to_launchers_or_links_are_omitted(self):
        targets = [('/usr/games/lutris', 'lutris:rungameid/2', 'Lutris'),
                   (str(self.home / '.cxoffice/AlterEGo/desktopdata/AlterEgo.lnk'), '', 'CrossOver'),
                   ('/some/shortcut.lnk', '', 'indirect')]
        for target, options, expected in targets:
            self.steam_fixture()
            data = (b'\0shortcuts\0\0' + b'0\0\x01AppName\0Test\0'
                    + b'\x01Exe\0' + target.encode() + b'\0\x01LaunchOptions\0'
                    + options.encode() + b'\0\x08\x08\x08')
            path = self.put('.local/share/Steam/userdata/1/config/shortcuts.vdf', data)
            self.assertFalse(any(r['kind'] == 'steam-shortcut' for r in self.scan()))
            self.assertIn(expected, fps.delegated_shortcut(target, options, self.home))
            self.assertEqual(path.read_bytes(), data)
        self.assertEqual(fps.delegated_shortcut('/games/NeuroHell', '', self.home), '')

    def test_large_steam_metadata_does_not_block_existing_correct_options(self):
        command = "MANGOHUD_CONFIG='" + self.setup.options() + "' mangohud %command%"
        data = STEAM.replace(b'"LaunchOptions" ""', b'"LaunchOptions" ' + fps.quote(command).encode(), 1)
        data += b'"LocalizedTagNames_french" "' + b'x' * 24000 + b'"\n'
        row = dict(kind='steam', extra='730', note='mangohud %command%')
        self.assertEqual(self.setup.update(row, data), data)
        unsafe = data.replace(b'mangohud %command%', b'mangohud %command% ; custom', 1)
        with self.assertRaises(fps.Preserved):
            self.setup.update(row, unsafe)
        with self.assertRaises(fps.SetupError):
            fps.vdf(b'"key" "' + b'x' * (fps.MAX_VDF_VALUE_CHARS + 1) + b'"')
        with self.assertRaises(fps.SetupError):
            fps.vdf(b'"' + b'k' * (fps.MAX_VDF_KEY_CHARS + 1) + b'" "value"')

    def test_yaml_hex_escapes_are_supported_without_rewriting_the_path(self):
        exe = self.put('Téléchargements/game.exe', 'game')
        quoted = '"' + str(exe).replace('é', r'\xE9') + '"'
        data = ('game:\n  exe: ' + quoted + '\nsystem: {}\n').encode()
        self.assertTrue(fps.lutris_executable_exists(data))
        updated = self.setup.update(dict(kind='lutris'), data)
        self.assertIn(quoted.encode(), updated)
        self.assertIn(b'mangohud: true', updated)
        self.assertEqual(fps.yaml_string(r'"\u00e9\U0001f600" # comment'), 'é😀')
        for value in (r'"\xZZ"', r'"\uD800"', r'"\U00110000"', r'"\q"'):
            with self.assertRaises(fps.SetupError):
                fps.yaml_string(value)

    def test_missing_lutris_executable_is_not_listed_as_installed(self):
        self.put('.config/lutris/games/removed.yml', 'game:\n  exe: "' + str(self.home / 'removed.exe') + '"\n')
        exe = self.put('Games/working.exe', 'game')
        self.put('.config/lutris/games/working.yml', 'game:\n  exe: "' + str(exe) + '"\n')
        # Unknown runner substitutions must not be declared uninstalled.
        self.put('.config/lutris/games/unknown.yml', 'game:\n  exe: $GAMEDIR/game.exe\n')
        rows = [r for r in self.scan() if r['kind'] == 'lutris']
        self.assertEqual({r['label'] for r in rows}, {'working', 'unknown'})
        self.assertIsNone(fps.lutris_executable_exists(b'game:\n  exe: C:/game.exe\n  prefix: $GAMEDIR\n'))

    def test_heroic_same_install_is_listed_once_and_legacy_profile_is_untouched(self):
        directory = str(self.home / 'Games/Ghostrunner')
        self.put('.config/legendary/installed.json', json.dumps({'epic': {'title': 'Ghostrunner', 'install_path': directory, 'executable': 'Ghostrunner.exe'}}))
        self.put('.config/heroic/sideload_apps/library.json', json.dumps({'games': [{'app_name': 'imported', 'title': 'Ghostrunner', 'is_installed': True, 'install': {'executable': directory + '/Ghostrunner.exe'}}]}))
        legacy = self.put('.config/heroic/GamesConfig/epic.json', '{"epic":{"custom":"keep"},"version":"v0"}')
        original = legacy.read_bytes()
        rows = self.scan()
        games = [r for r in rows if r['kind'] == 'heroic']
        self.assertEqual([r['extra'] for r in games], ['imported'])
        results = self.apply([r for r in rows if r['kind'] in ('common', 'heroic')])
        self.assertTrue(all(r['status'] == 'changed' for r in results))
        self.assertEqual(legacy.read_bytes(), original)
        self.assertTrue((self.home / '.config/heroic/GamesConfig/imported.json').is_file())

    def test_heroic_equal_titles_with_different_installations_remain_separate(self):
        directory = str(self.home / 'Games/Ghostrunner')
        self.put('.config/legendary/installed.json', json.dumps({'epic': {'title': 'Ghostrunner', 'install_path': directory, 'executable': 'Ghostrunner.exe'}}))
        self.put('.config/heroic/sideload_apps/library.json', json.dumps({'games': [{'app_name': 'imported', 'title': 'Ghostrunner', 'is_installed': True, 'install': {'executable': directory + '-other/Ghostrunner.exe'}}]}))
        self.assertEqual(len([r for r in self.scan() if r['kind'] == 'heroic']), 2)

    def test_heroic_same_executable_with_distinct_prefixes_keeps_both_profiles(self):
        directory = str(self.home / 'Games/Ghostrunner')
        self.put('.config/legendary/installed.json', json.dumps({'epic': {'title': 'Ghostrunner', 'install_path': directory, 'executable': 'Ghostrunner.exe'}}))
        self.put('.config/heroic/sideload_apps/library.json', json.dumps({'games': [{'app_name': 'imported', 'title': 'Ghostrunner', 'is_installed': True, 'install': {'executable': directory + '/Ghostrunner.exe'}}]}))
        for app in ('epic', 'imported'):
            self.put('.config/heroic/GamesConfig/' + app + '.json', json.dumps({app: {'winePrefix': str(self.home / app)}}))
        self.assertEqual(len([r for r in self.scan() if r['kind'] == 'heroic']), 2)

    def test_steam_changes_only_empty_options_and_keeps_unrelated_bytes(self):
        updated = fps.patch_steam(STEAM, '730', 'mangohud %command%')
        self.assertEqual(updated.replace(b'"mangohud %command%"', b'""'), STEAM)
        self.assertEqual(updated, fps.patch_steam(updated, '730', 'mangohud %command%'))
        with self.assertRaisesRegex(fps.SetupError, 'personnelles'):
            fps.patch_steam(STEAM, '440', 'mangohud %command%')
        created = fps.patch_steam(STEAM, '100', 'MANGOHUD=1 %command%')
        fps.vdf(created)
        self.assertIn(b'"100"', created)

    def test_invalid_or_ambiguous_steam_configuration_is_not_rewritten(self):
        for data in (b'"a" {', b'"a" "x" "A" "y"', b'"a" [bad]', b'"' + b'x' * 5000):
            with self.assertRaises(fps.SetupError):
                fps.patch_steam(data, '730', 'mangohud %command%')

    def steam_fixture(self):
        for appid in ('730', '440'):
            self.put('.local/share/Steam/steamapps/appmanifest_' + appid + '.acf',
                     '"AppState" { "appid" "' + appid + '" "name" "Game ' + appid + '" }')
        return self.put('.local/share/Steam/userdata/1/config/localconfig.vdf', STEAM)

    def test_multiple_games_in_same_steam_file_and_safe_restore(self):
        path = self.steam_fixture()
        rows = self.scan()
        results = self.apply(rows)
        self.assertEqual([row['status'] for row in results], ['changed', 'preserved', 'changed'])
        changed = [row for row in results if row.get('backup')]
        token = changed[-1]['backup']
        self.assertIn(b'mangohud %command%', path.read_bytes())
        self.setup.restore(token)
        self.assertEqual(path.read_bytes(), STEAM)
        common = changed[0]['backup']
        self.setup.restore(common)
        self.assertFalse(self.setup.mango.exists())

    def test_stale_scan_and_running_launcher_do_not_modify_files(self):
        path = self.steam_fixture()
        rows = self.scan()
        self.apply([rows[0]])
        path.write_bytes(STEAM + b'\n')
        result = self.apply([row for row in rows if row['kind'] == 'steam'])
        self.assertTrue(all('modifié' in row['detail'] for row in result))
        self.assertEqual(path.read_bytes(), STEAM + b'\n')
        with patch('fps_setup.subprocess.run', side_effect=FileNotFoundError), patch.object(self.setup, 'running', return_value=True):
            result = self.setup.apply([row for row in self.scan() if row['kind'] == 'steam'])
        self.assertTrue(all('Fermez' in row['detail'] for row in result))

    def test_heroic_global_and_installed_games_are_independent(self):
        self.put('.config/heroic/config.json', '{"defaultSettings":{"enviromentOptions":[{"key":"KEEP","value":"yes"}]},"other":42}')
        self.put('.config/legendary/installed.json', '{"epic-game":{"title":"Installed"}}')
        self.put('.config/heroic/GamesConfig/epic-game.json', '{"epic-game":{"winePrefix":"keep","enviromentOptions":[{"key":"KEEP","value":"yes"}]},"version":"v0","explicit":true}')
        rows = self.scan()
        self.assertEqual([r['kind'] for r in rows], ['common', 'heroic-global', 'heroic'])
        results = self.apply(rows)
        self.assertTrue(all(r['status'] == 'changed' for r in results))
        game = json.loads((self.home / '.config/heroic/GamesConfig/epic-game.json').read_text())
        self.assertTrue(game['epic-game']['showMangohud'])
        self.assertEqual(game['epic-game']['winePrefix'], 'keep')
        self.assertEqual(game['epic-game']['enviromentOptions'][0], dict(key='KEEP', value='yes'))
        self.assertTrue(game['explicit'])
        self.assertEqual(json.loads((self.home / '.config/heroic/config.json').read_text())['other'], 42)
        results = self.apply(self.scan())
        self.assertTrue(all(r['status'] == 'unchanged' for r in results))

    def test_heroic_personal_environment_is_not_overwritten(self):
        row = dict(kind='heroic', extra='game')
        with self.assertRaisesRegex(fps.SetupError, 'personnel'):
            self.setup.update(row, b'{"game":{"enviromentOptions":[{"key":"MANGOHUD_CONFIG","value":"custom"}]}}')
        with self.assertRaisesRegex(fps.SetupError, 'dupliqu'):
            fps.json_object(b'{"game":{},"game":{}}')

    def test_lutris_and_bottles_edits_preserve_unrelated_settings(self):
        yaml = b'name: Test\ngame:\n  exe: /some/game.exe\nsystem:\n  env:\n    KEEP: "yes"\n  mangohud: false\n# comment\n'
        row = dict(kind='lutris')
        updated = self.setup.update(row, yaml)
        self.assertIn(b'KEEP: "yes"', updated)
        self.assertIn(b'mangohud: true', updated)
        self.assertIn(b'MANGOHUD_CONFIGFILE:', updated)
        self.assertIn(b'# comment', updated)
        self.assertEqual(updated, self.setup.update(row, updated))
        for data in (b'Name: Sample\nParameters: {}\nEnvironment_Variables: {}\n',
                     b'Name: Sample\nParameters:\n  dxvk: true\n'):
            updated = self.setup.update(dict(kind='bottles'), data)
            self.assertIn(b'mangohud: true', updated)
            self.assertEqual(updated, self.setup.update(dict(kind='bottles'), updated))
        empty = self.setup.update(row, b'game:\n  exe: /game.exe\n')
        self.assertIn(b'  env:\n    MANGOHUD_CONFIGFILE:', empty)

    def test_nonsteam_shortcuts_are_configured_without_rewriting_other_bytes(self):
        data = (b'\0shortcuts\0\0' + b'0\0\x01AppName\0NeuroHell\0'
                + b'\x01Exe\0/game/neurohell\0\x01LaunchOptions\0\0\x08\x08\x08')
        changed = fps.patch_shortcut(data, '0', 'mangohud %command%')
        self.assertEqual(changed.replace(b'mangohud %command%', b''), data)
        self.assertEqual(changed, fps.patch_shortcut(changed, '0', 'mangohud %command%'))
        self.assertEqual(fps.binary_shortcuts(changed)['0'][0]['appname'][0], 'NeuroHell')
        with self.assertRaises(fps.SetupError):
            fps.binary_shortcuts(data[:-1])
        with self.assertRaisesRegex(fps.SetupError, 'personnelles'):
            fps.patch_shortcut(changed, '0', 'other %command%')

    def test_new_heroic_game_keeps_inherited_environment_variables(self):
        self.put('.config/heroic/config.json', '{"defaultSettings":{"enviromentOptions":[{"key":"KEEP","value":"yes"}]}}')
        self.put('.config/heroic/sideload_apps/library.json', '{"games":[{"app_name":"personal","title":"NeuroHell","is_installed":true}]}')
        rows = self.scan()
        self.apply([r for r in rows if r['kind'] in ('common', 'heroic')])
        data = json.loads((self.home / '.config/heroic/GamesConfig/personal.json').read_text())
        self.assertEqual(data['personal']['enviromentOptions'][0]['key'], 'KEEP')
        self.assertEqual(data['version'], 'v0.1')

    def test_flatpak_backup_restores_exact_previous_overrides(self):
        path = self.put('.local/share/flatpak/overrides/com.valvesoftware.Steam', '[Context]\nfilesystems=/personal;\n')
        original = path.read_bytes()
        row = dict(path=str(path), fingerprint=fps.digest(original), extra='com.valvesoftware.Steam')
        def configure(*args, **kwargs):
            path.write_bytes(original + b'# added access\n')
            return type('Result', (), dict(returncode=0, stderr=b''))()
        with patch('fps_setup.subprocess.run', side_effect=configure):
            result = self.setup.allow_flatpak(row)
        self.assertEqual(result['status'], 'changed')
        self.assertTrue(result['backup'])
        self.setup.restore(result['backup'])
        self.assertEqual(path.read_bytes(), original)

    def test_shared_steam_file_is_saved_and_backed_up_once(self):
        path = self.steam_fixture()
        path.write_bytes(STEAM.replace(b'custom --args %command%', b''))
        rows = self.scan()
        with patch.object(self.setup, 'write', wraps=self.setup.write) as write:
            results = self.apply(rows)
        self.assertEqual(write.call_count, 2)  # common config + shared Steam file
        steam = [r for r in results if r['label'].startswith('Game')]
        self.assertTrue(all(r['status'] == 'changed' for r in steam))
        self.assertEqual(steam[0]['backup'], steam[1]['backup'])
        self.setup.restore(steam[0]['backup'])
        self.assertEqual(path.read_bytes(), STEAM.replace(b'custom --args %command%', b''))

    def test_yaml_indentation_scalars_and_scalar_lists(self):
        for data in (b'system:\n  env:\n    KEEP: yes\n   stray: value\n',
                     b'system: {}\n  mangohud: false\n',
                     b'Parameters: {mangohud: false}\n', b'Name: "unterminated\n'):
            with self.assertRaises(fps.SetupError):
                self.setup.update(dict(kind='bottles'), data)
        bottles = (b'Name: "Sample"\nInherited_Environment_Variables:\n- HOME\n- PATH\n'
                   b'Installed_Dependencies: []\nParameters:\n    dxvk: true\n'
                   b'External_Programs:\n    1234-abcd:\n        name: Game\n        path: C:\\Game\\game.exe\n')
        updated = self.setup.update(dict(kind='bottles'), bottles)
        fps.validate_yaml(updated)
        self.assertIn(b'path: C:\\Game\\game.exe', updated)

    def test_advanced_yaml_is_refused(self):
        for data in (b'system: &shared\n  env: {}\n', b'system:\n  <<: *alias\n',
                     b'system:\n  mangohud: false\n  mangohud: true\n', b'system:\n\tenv: {}\n'):
            with self.assertRaises(fps.SetupError):
                self.setup.update(dict(kind='lutris'), data)

    def test_playonlinux_preserves_custom_prerun_script(self):
        row = dict(kind='pol')
        script = self.setup.update(row, b'')
        self.assertIn(b'BEFORE_WINE=', script)
        self.assertEqual(script, self.setup.update(row, script))
        with self.assertRaisesRegex(fps.SetupError, 'conserv'):
            self.setup.update(row, b'echo custom\n')

    def test_symlinks_and_world_writable_configs_are_refused(self):
        original = self.put('outside.txt', 'original')
        linked = self.home / '.config/MangoHud'
        linked.parent.mkdir()
        linked.symlink_to(self.home, target_is_directory=True)
        with self.assertRaises(fps.SetupError):
            self.setup.write(linked / 'outside.txt', b'original', b'changed')
        self.assertEqual(original.read_bytes(), b'original')
        original.chmod(0o666)
        with self.assertRaises(fps.SetupError):
            self.setup.write(original, b'original', b'changed')

    def test_restore_never_clobbers_subsequent_user_edits(self):
        path = self.put('file.json', 'original')
        token = self.setup.write(path, b'original', b'changed')
        path.write_bytes(b'personal edit')
        with self.assertRaisesRegex(fps.SetupError, 'changé'):
            self.setup.restore(token)
        self.assertEqual(path.read_bytes(), b'personal edit')
        with self.assertRaises(fps.SetupError):
            self.setup.restore('../file.json')

    def test_flatpak_allowlist_and_permissions_are_narrow(self):
        result = subprocess_result = type('Result', (), dict(returncode=0, stdout=b'com.valvesoftware.Steam\norg.other.App\n', stderr=b''))()
        with patch('fps_setup.subprocess.run', return_value=result):
            rows = self.setup.scan()
        flatpak = [r for r in rows if r['kind'] == 'flatpak']
        self.assertEqual(len(flatpak), 1)
        with patch('fps_setup.subprocess.run', return_value=subprocess_result) as run:
            self.setup.allow_flatpak(flatpak[0])
        args = run.call_args.args[0]
        self.assertIn('--filesystem=' + str(self.setup.mango.parent) + ':ro', args)
        self.assertIn('--filesystem=' + str(self.setup.folder), args)
        self.assertFalse(any('filesystem=home' in arg or 'device=' in arg for arg in args))


if __name__ == '__main__':
    unittest.main()
