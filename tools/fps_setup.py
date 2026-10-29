#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded, offline launcher configuration. No launcher or game scripts are executed.

Formats: Heroic src/backend/{config,game_config}.ts; Lutris lutris/config.py
and sysoptions.py; Bottles backend/models/config.py; PlayOnLinux
python/configurewindow/ConfigureWindowNotebook.py (pre_shortcut).
Steam localconfig uses Valve text KeyValues; edits retain all untouched bytes.
Only a conservative block-mapping subset of YAML is edited, never evaluated.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import shutil
import shlex
import subprocess
import sys
import tempfile
import uuid

MAX_BYTES = 8 * 1024 * 1024
MAX_ROWS = 1024
MAX_VDF_VALUE_CHARS = 1024 * 1024
MAX_VDF_KEY_CHARS = 4096
FLATPAKS = {"com.valvesoftware.Steam": "Steam", "com.heroicgameslauncher.hgl": "Heroic",
            "net.lutris.Lutris": "Lutris", "com.usebottles.bottles": "Bottles"}


class SetupError(Exception):
    pass


class Preserved(SetupError):
    pass


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read(path, missing=False, limit=MAX_BYTES):
    path = Path(path)
    if not path.exists() and missing:
        return b""
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or path.is_symlink() or info.st_size > limit:
        raise SetupError("Fichier non régulier, lien symbolique ou taille excessive")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise SetupError("Fichier trop volumineux")
    return data


def object_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise SetupError("Clé JSON dupliquée")
        result[key] = value
    return result


def json_object(data):
    try:
        def invalid_constant(value):
            raise SetupError("Constante JSON invalide : " + value)
        result = json.loads(data, object_pairs_hook=object_pairs, parse_constant=invalid_constant)
    except RecursionError as error:
        raise SetupError("JSON trop profond") from error
    pending = [(result, 0)]
    count = 0
    while pending:
        value, depth = pending.pop()
        count += 1
        if depth > 32 or count > 65536:
            raise SetupError("JSON trop complexe")
        if isinstance(value, dict):
            pending.extend((item, depth + 1) for item in value.values())
        elif isinstance(value, list):
            pending.extend((item, depth + 1) for item in value)
    if not isinstance(result, dict):
        raise SetupError("Objet JSON attendu")
    return result


def atomic(path, data, mode=0o600):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=".galleon-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            os.fchmod(stream.fileno(), mode)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def safe_target(path, home):
    """Configuration targets must remain inside the user's own home, without links."""
    path = Path(path).absolute()
    if not path.is_relative_to(home):
        raise SetupError("Configuration hors du dossier personnel : action manuelle nécessaire")
    current = home
    for part in path.relative_to(home).parts:
        current = current / part
        if current.is_symlink():
            raise SetupError("Lien symbolique dans le chemin de configuration")
        if current.exists():
            info = current.stat()
            if info.st_uid != os.getuid() or info.st_mode & 0o002:
                raise SetupError("Configuration non personnelle ou modifiable par tous les utilisateurs")
    return path


# Text KeyValues lexer with source offsets, used to insert only LaunchOptions.
def vdf(data):
    text = data.decode("utf-8")
    tokens = []
    pos = 0
    while pos < len(text):
        if text[pos].isspace():
            pos += 1
        elif text.startswith("//", pos):
            end = text.find("\n", pos)
            pos = len(text) if end < 0 else end + 1
        elif text[pos] in "{}":
            tokens.append((text[pos], text[pos], pos, pos + 1)); pos += 1
        elif text[pos] == '"':
            start = pos; pos += 1; chars = []
            while pos < len(text) and text[pos] != '"':
                char = text[pos]; pos += 1
                if char == "\\":
                    if pos == len(text):
                        raise SetupError("VDF tronqué")
                    char = {"n": "\n", "t": "\t"}.get(text[pos], text[pos]); pos += 1
                chars.append(char)
                if len(chars) > MAX_VDF_VALUE_CHARS:
                    raise SetupError("Valeur VDF trop longue")
            if pos == len(text):
                raise SetupError("VDF tronqué")
            pos += 1; tokens.append(("s", "".join(chars), start, pos))
        else:
            raise SetupError("Format VDF non pris en charge")
        if len(tokens) > 200000:
            raise SetupError("VDF trop complexe")
    cursor = 0

    def body(depth=0):
        nonlocal cursor
        if depth > 16:
            raise SetupError("VDF trop profond")
        nodes = {}
        while cursor < len(tokens) and tokens[cursor][0] != "}":
            key = tokens[cursor]; cursor += 1
            if key[0] != "s" or len(key[1]) > MAX_VDF_KEY_CHARS or cursor >= len(tokens) or key[1].lower() in nodes:
                raise SetupError("VDF ambigu")
            token = tokens[cursor]; cursor += 1
            if token[0] == "{":
                children = body(depth + 1)
                if cursor >= len(tokens):
                    raise SetupError("VDF tronqué")
                close = tokens[cursor]; cursor += 1
                nodes[key[1].lower()] = (children, token[2], close[2])
            elif token[0] == "s":
                nodes[key[1].lower()] = (token[1], token[2], token[3])
            else:
                raise SetupError("VDF invalide")
        return nodes
    tree = body()
    if cursor != len(tokens):
        raise SetupError("VDF invalide")
    return text, tree


def child(tree, key):
    node = tree.get(key.lower())
    return node[0] if node and isinstance(node[0], dict) else {}


def quote(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def same_launch_options(existing, candidates):
    try:
        tokens = shlex.split(existing)
        return any(tokens == shlex.split(candidate) for candidate in candidates)
    except ValueError:
        return False


def patch_steam(data, appid, launch, alternatives=()):
    text, tree = vdf(data)
    steam = child(child(child(tree, "UserLocalConfigStore"), "Software"), "Valve")
    steam = child(steam, "Steam")
    apps = steam.get("apps")
    if not apps or not isinstance(apps[0], dict):
        raise SetupError("Section Steam apps absente : ouvrir Steam une fois, puis le fermer")
    game = apps[0].get(appid)
    if game:
        if not isinstance(game[0], dict):
            raise SetupError("Configuration de jeu Steam invalide")
        option = game[0].get("launchoptions")
        if option:
            if not isinstance(option[0], str):
                raise SetupError("Options Steam invalides")
            if same_launch_options(option[0], (launch, *alternatives)):
                return data
            if option[0].strip():
                raise Preserved("Options de lancement personnelles conservées")
            return (text[:option[1]] + quote(launch) + text[option[2]:]).encode()
        pos = game[2]; insert = '\n\t\t\t\t\t"LaunchOptions"\t\t' + quote(launch) + '\n\t\t\t\t'
    else:
        pos = apps[2]
        insert = '\n\t\t\t\t' + quote(appid) + '\n\t\t\t\t{\n\t\t\t\t\t"LaunchOptions"\t\t' + quote(launch) + '\n\t\t\t\t}\n\t\t\t'
    return (text[:pos] + insert + text[pos:]).encode()


def binary_shortcuts(data):
    """Binary KeyValues offsets; unknown types are refused, never guessed."""
    cursor = 0
    fields = 0

    def string():
        nonlocal cursor
        end = data.find(b'\0', cursor)
        if end < 0 or end - cursor > 4096:
            raise SetupError('Raccourci Steam tronqué')
        start = cursor; cursor = end + 1
        return data[start:end].decode('utf-8')

    def body(depth):
        nonlocal cursor, fields
        if depth > 8:
            raise SetupError('Raccourcis Steam trop profonds')
        nodes = {}
        while cursor < len(data):
            kind = data[cursor]; cursor += 1
            if kind == 8:
                return nodes, cursor - 1
            key = string().lower()
            fields += 1
            if fields > 32768 or key in nodes:
                raise SetupError('Raccourcis Steam ambigus')
            if kind == 0:
                nested, end = body(depth + 1)
                nodes[key] = (nested, 0, end)
            elif kind == 1:
                start = cursor; value = string()
                nodes[key] = (value, start, cursor)
            elif kind in (2, 3, 4, 6, 7, 10):
                size = 8 if kind in (7, 10) else 4
                if cursor + size > len(data):
                    raise SetupError('Raccourci Steam tronqué')
                nodes[key] = (None, cursor, cursor + size)
                cursor += size
            else:
                raise SetupError('Type de raccourci Steam inconnu')
        raise SetupError('Raccourcis Steam tronqués')
    tree, _ = body(0)
    if cursor != len(data):
        raise SetupError('Raccourcis Steam invalides')
    return child(tree, 'shortcuts')


def patch_shortcut(data, index, launch, alternatives=()):
    game = binary_shortcuts(data).get(index)
    if not game or not isinstance(game[0], dict):
        raise SetupError('Raccourci Steam disparu')
    option = game[0].get('launchoptions')
    value = launch.encode() + b'\0'
    if option:
        if not isinstance(option[0], str):
            raise SetupError('Options du raccourci invalides')
        if same_launch_options(option[0], (launch, *alternatives)):
            return data
        if option[0].strip():
            raise Preserved('Options de lancement personnelles conservées')
        return data[:option[1]] + value + data[option[2]:]
    pos = game[2]
    return data[:pos] + b'\x01LaunchOptions\0' + value + data[pos:]


def yaml_string(value):
    """Decode one scalar, including YAML's hex escapes, without evaluating YAML."""
    value = value.strip()
    if value.startswith('"'):
        result = []; index = 1
        escapes = {'0': '\0', 'a': '\a', 'b': '\b', 't': '\t', 'n': '\n',
                   'v': '\v', 'f': '\f', 'r': '\r', 'e': '\x1b', ' ': ' ',
                   '"': '"', '/': '/', '\\': '\\', 'N': '\x85', '_': '\xa0', 'L': '\u2028', 'P': '\u2029'}
        while index < len(value):
            char = value[index]; index += 1
            if char == '"':
                trailing = value[index:].strip()
                if trailing and not trailing.startswith('#'):
                    raise SetupError('Chaîne YAML invalide')
                return ''.join(result)
            if char == '\\':
                if index >= len(value):
                    raise SetupError('Chaîne YAML tronquée')
                escape = value[index]; index += 1
                if escape in ('x', 'u', 'U'):
                    digits = {'x': 2, 'u': 4, 'U': 8}[escape]
                    token = value[index:index + digits]
                    if len(token) != digits or not re.fullmatch('[0-9a-fA-F]+', token):
                        raise SetupError('Échappement YAML invalide')
                    number = int(token, 16)
                    if number > 0x10ffff or 0xd800 <= number <= 0xdfff:
                        raise SetupError('Point Unicode YAML invalide')
                    char = chr(number); index += digits
                elif escape in escapes:
                    char = escapes[escape]
                else:
                    raise SetupError('Échappement YAML non pris en charge')
            result.append(char)
        raise SetupError('Chaîne YAML tronquée')
    if value.startswith("'"):
        match = re.fullmatch(r"'((?:[^']|'')*)'\s*(?:#.*)?", value)
        if not match:
            raise SetupError('Chaîne YAML invalide')
        return match[1].replace("''", "'")
    if not value or value[0] in '&*!|>{[':
        raise SetupError('Scalaire YAML non pris en charge')
    return value.split(' #', 1)[0].rstrip()


def lutris_executable_exists(data):
    """False only for a recognizable, absolute executable that is now missing.

    Inspect the top-level game map only, not embedded installer scripts. Unknown
    runner formats remain available for manual review; no shell expansion runs.
    """
    inside = False; fields = {}; indentation = None
    for line in data.decode().splitlines():
        if line.startswith('game:') and line.strip() == 'game:':
            if inside or fields:
                return None
            inside = True; continue
        if not inside or not line.strip() or line.lstrip().startswith('#'):
            continue
        level = len(line) - len(line.lstrip(' '))
        if level == 0:
            break
        if indentation is None:
            indentation = level
        if level != indentation:
            continue
        match = re.fullmatch(r'\s*(exe|prefix):\s*(.+)', line)
        if match:
            if match[1] in fields:
                return None
            fields[match[1]] = yaml_string(match[2])
    executable = fields.get('exe', '').replace('\\', '/')
    if not executable or any(char in executable for char in ('$', '\0', '\n', '\r')):
        return None
    if re.match(r'^[cC]:/', executable):
        prefix = fields.get('prefix', '')
        if not Path(prefix).is_absolute():
            return None
        executable = str(Path(prefix) / 'drive_c' / executable[3:])
    elif re.match(r'^[zZ]:/', executable):
        executable = '/' + executable[3:]
    if not Path(executable).is_absolute():
        return None
    return Path(executable).is_file()


def validate_yaml(data):
    """Validate the block maps/scalar lists we can preserve safely.

    No aliases, directives, tags, multi-line scalars, flow collections beyond
    empty maps/lists, quoted keys or list-of-map constructs are accepted. These
    remain manual actions. Nested indentation and duplicate keys are checked.
    """
    frames = [dict(indent=0, kind='map', keys=set(), pending=False)]
    lines = data.decode().splitlines()
    if len(lines) > 16000:
        raise SetupError('YAML trop complexe')

    def scalar(value):
        if len(value) > 8192 or any(ord(char) < 32 for char in value):
            raise SetupError('Scalaire YAML invalide')
        if value.startswith(('"', "'")):
            yaml_string(value)
        elif value not in ('{}', '[]') and (not value or value[0] in '&*!|>{['
                or re.search(r':(?:\s|$)|\s[&*!|>]', value)):
            raise SetupError('YAML avancé : configuration manuelle nécessaire')

    for line in lines:
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if '\t' in line or len(line) > 8192:
            raise SetupError('YAML non pris en charge')
        indent = len(line) - len(line.lstrip(' '))
        content = line[indent:]
        is_list = content.startswith('- ')
        # Comments outside quoted values are ignored for validation only.
        match = re.fullmatch(r'([A-Za-z_0-9][\w.-]*):(?:\s+(.*))?', content) if not is_list else None
        if not is_list and not match:
            raise SetupError('YAML non reconnu')
        # A map after an indentless sequence closes that sequence.
        if frames[-1]['kind'] == 'seq' and not is_list and indent <= frames[-1]['indent']:
            frames.pop()
        while indent < frames[-1]['indent']:
            frames.pop()
        frame = frames[-1]
        if indent > frame['indent'] or (is_list and frame['kind'] == 'map' and frame['pending']):
            if not frame['pending']:
                raise SetupError('Indentation YAML ambiguë')
            frame['pending'] = False
            frames.append(dict(indent=indent, kind='seq' if is_list else 'map', keys=set(), pending=False))
            if len(frames) > 32:
                raise SetupError('YAML trop profond')
            frame = frames[-1]
        if indent != frame['indent'] or (is_list != (frame['kind'] == 'seq')):
            raise SetupError('Indentation YAML invalide')
        if is_list:
            scalar(content[2:].strip())
            continue
        key, value = match.groups()
        if key in frame['keys']:
            raise SetupError('Clé YAML dupliquée')
        frame['keys'].add(key)
        value = (value or '').strip()
        if value.startswith('#'):
            value = ''
        elif value and not value.startswith(('"', "'")):
            value = value.split(' #', 1)[0].rstrip()
        frame['pending'] = not value
        if value:
            scalar(value)


def yaml_environment_conflict(data, section):
    """Detect a per-game inline config that would bypass the common config file."""
    stack = []
    for line in data.decode().splitlines():
        match = re.match(r'^( *)([A-Za-z_0-9][\w.-]*):(?:\s+(.*?))?\s*$', line)
        if not match:
            continue
        padding, name, value = match.groups()
        level = len(padding)
        while stack and stack[-1][0] >= level:
            stack.pop()
        full = '/'.join([item[1] for item in stack] + [name])
        if full == section + '/MANGOHUD_CONFIG' and value:
            # Even read_cfg can be combined with per-game logging overrides.
            # Leave a personal inline config for manual review instead of
            # claiming that writing MANGOHUD_CONFIGFILE guarantees CSV logging.
            raise Preserved('MANGOHUD_CONFIG personnel conservé : vérifiez le dossier CSV manuellement')
        if value in (None, '', '{}'):
            stack.append((level, name))


def yaml_patch(data, section, key, value):
    """Patch a simple block mapping, retaining unrelated YAML byte for byte.

    Refuse aliases, merges, tags, flow collections (except empty maps), block
    scalars, tabs and duplicate mappings. The caller reports a manual action.
    """
    validate_yaml(data)
    text = data.decode("utf-8")
    lines = text.splitlines(keepends=True)
    if len(lines) > 16000 or any(len(line) > 8192 for line in lines):
        raise SetupError("YAML trop complexe")
    stack = []; seen = set(); found_section = None; target = None
    end = len(lines); indent = 2; section_indent = 0
    for index, line in enumerate(lines):
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            continue
        if '\t' in line or stripped in ('---', '...'):
            raise SetupError("YAML avancé : configuration manuelle nécessaire")
        # Ignore list/scalar contents only when editing a different simple map;
        # ambiguous YAML constructs are rejected throughout the document.
        if re.search(r'(^|\s)[&*!|>]|<<\s*:', stripped):
            raise SetupError("YAML avancé : configuration manuelle nécessaire")
        match = re.match(r'^( *)([A-Za-z_0-9][\w.-]*):(?:\s+(.*?))?\s*$', line.rstrip('\r\n'))
        if not match:
            # Bottles stores External_Programs under UUID keys and lists; don't
            # interpret them. Only allow plainly recognizable mapping/list lines.
            if not re.match(r'^\s*(?:-[ \w"\']|[\w"\'][^\n]*:)', line):
                raise SetupError("YAML non reconnu")
            continue
        padding, name, scalar = match.groups(); level = len(padding)
        while stack and stack[-1][0] >= level:
            stack.pop()
        full = '/'.join([entry[1] for entry in stack] + [name])
        if full in seen:
            raise SetupError("Clé YAML dupliquée")
        seen.add(full)
        if found_section is not None and index > found_section and level <= section_indent and end == len(lines):
            end = index
        if full == section:
            if scalar not in (None, '', '{}'):
                raise SetupError("Section YAML non prise en charge")
            found_section = index; section_indent = level; indent = level + 2
        if full.startswith(section + '/') and full.count('/') == section.count('/') + 1:
            indent = level
        if full == section + '/' + key:
            target = index; indent = level
        if scalar in (None, '', '{}'):
            stack.append((level, name))
    encoded = str(value).lower() if isinstance(value, bool) else json.dumps(value, ensure_ascii=False)
    replacement = ' ' * indent + key + ': ' + encoded + '\n'
    if target is not None:
        old = lines[target].split(':', 1)[1].strip()
        if old == encoded:
            return data
        # Other MangoHud variables remain untouched; only selected fields change.
        lines[target] = replacement
    elif found_section is not None:
        lines[found_section] = lines[found_section].replace('{}', '').rstrip() + '\n'
        lines.insert(end, replacement)
    else:
        if '/' in section:
            raise SetupError("Section YAML imbriquée absente")
        if lines and not lines[-1].endswith('\n'):
            lines[-1] += '\n'
        lines += [section + ':\n', replacement]
    return ''.join(lines).encode()


def delegated_shortcut(executable, options, home):
    """Don't wrap another launcher or a Windows link as a native executable."""
    target = executable.strip().strip('"').replace('\\', '/')
    if not target:
        return 'Raccourci sans exécutable reconnu : configuration manuelle nécessaire.'
    path = Path(target)
    launchers = {'lutris': 'Lutris', 'heroic': 'Heroic', 'bottles': 'Bottles',
                 'playonlinux': 'PlayOnLinux', 'crossover': 'CrossOver', 'cxrun': 'CrossOver', 'steam': 'Steam'}
    launcher = launchers.get(path.name.lower())
    if path.is_relative_to(home / '.cxoffice'):
        launcher = 'CrossOver'
    elif options.strip().strip('"').lower().startswith('lutris:'):
        launcher = 'Lutris'
    if launcher:
        return 'Raccourci vers ' + launcher + ' : configurer dans ce lanceur ; options Steam conservées.'
    if path.suffix.lower() in ('.lnk', '.desktop'):
        return 'Raccourci indirect : configurer le lanceur d’origine ; options Steam conservées.'
    return ''


def installed_flatpaks():
    try:
        result = subprocess.run(['flatpak', 'list', '--app', '--columns=application'],
                                capture_output=True, timeout=5, check=False)
        if result.returncode == 0:
            return set(result.stdout.decode().splitlines()) & set(FLATPAKS)
    except (OSError, ValueError, subprocess.TimeoutExpired):
        pass
    return set()


def native_installed(name, home, data):
    if shutil.which(name):
        return True
    if name == 'crossover':
        for path in (Path('/opt/cxoffice/bin/crossover'), home / 'cxoffice/bin/crossover'):
            if path.is_file() and os.access(path, os.X_OK):
                return True
    # Native AppImages may only have a desktop launcher, not a PATH entry.
    # Read Exec as arguments, never execute it or treat a Flatpak shortcut as native.
    for folder in (data / 'applications', Path('/usr/share/applications')):
        matches = [path for path in folder.glob('*.desktop') if name in path.name.lower()][:32]
        for path in matches:
            try:
                section = ''
                for line in read(path).decode().splitlines():
                    if line.startswith('['):
                        section = line.strip()
                    if section != '[Desktop Entry]' or not line.startswith('Exec='):
                        continue
                    args = shlex.split(line[5:])
                    if args and Path(args[0]).name == 'env':
                        args = args[1:]
                        while args and '=' in args[0] and not args[0].startswith('-'):
                            args.pop(0)
                    if not args or name not in Path(args[0]).name.lower():
                        continue
                    executable = shutil.which(args[0])
                    if executable and Path(executable).is_file() and os.access(executable, os.X_OK):
                        return True
            except (OSError, ValueError, SetupError):
                continue
    return False


def patch_crossover(data, config_file):
    """CodeWeavers' quoted INI EnvironmentVariables; retain unrelated lines.

    https://support.codeweavers.com/en_US/advanced-crossover-linux-configuration
    Format also checked against the installed CrossOver cxconfig.py reader.
    Vulkan/DXVK activation only; do not replace LD_PRELOAD for OpenGL.
    """
    if any(char in str(config_file) for char in '\n\r\0'):
        raise SetupError('Chemin de configuration CrossOver invalide')
    lines = data.decode().splitlines(keepends=True)
    desired = {'MANGOHUD': '1', 'MANGOHUD_CONFIGFILE': str(config_file)}
    section = ''; start = None; end = len(lines); fields = {}; sections = set()
    seen = set()
    quoted = re.compile(r'^\s*"((?:[^"\\]|\\.)*)"\s*=\s*"((?:[^"\\]|\\.)*)"\s*(?:;.*)?$')
    plain = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$')
    for index, line in enumerate(lines):
        value = line.strip()
        if not value or value.startswith((';', '#')):
            continue
        header = re.fullmatch(r'\[([^\[\]]+)\]\s*(?:;.*)?', value)
        if header:
            if start is not None and end == len(lines):
                end = index
            section = header[1].casefold()
            if section in sections:
                raise SetupError('Section CrossOver dupliquée : action manuelle nécessaire')
            sections.add(section)
            if section == 'environmentvariables':
                start = index
            continue
        match = quoted.fullmatch(line.rstrip('\r\n')) or plain.fullmatch(line.rstrip('\r\n'))
        if not section or not match:
            raise SetupError('Format CrossOver non reconnu')
        key = match[1].casefold()
        identity = (section, key)
        if identity in seen:
            raise SetupError('Variable CrossOver dupliquée : action manuelle nécessaire')
        seen.add(identity)
        if section == 'environmentvariables':
            if not re.fullmatch('[a-z_][a-z0-9_]*', key):
                raise SetupError('Nom de variable CrossOver non reconnu')
            if key == 'mangohud_config':
                raise Preserved('MANGOHUD_CONFIG personnel conservé : action manuelle nécessaire')
            fields[key] = (index, match[2])
    if 'bottle' not in sections:
        raise SetupError('Description de bouteille CrossOver absente')
    additions = []
    for key, value in desired.items():
        encoded = value.replace('\\', '\\\\').replace('"', '\\"')
        replacement = '"' + key + '" = "' + encoded + '"\n'
        existing = fields.get(key.casefold())
        if existing:
            if existing[1] != encoded:
                lines[existing[0]] = replacement
        else:
            additions.append(replacement)
    if additions:
        if lines and not lines[-1].endswith('\n'):
            lines[-1] += '\n'
        if start is None:
            lines += ['[EnvironmentVariables]\n'] + additions
        else:
            lines[end:end] = additions
    return ''.join(lines).encode()


class Setup:
    def __init__(self, home=None, config=None, data=None):
        self.home = Path(home or Path.home()).resolve()
        self.config = Path(config or os.environ.get('XDG_CONFIG_HOME', self.home / '.config'))
        self.data = Path(data or os.environ.get('XDG_DATA_HOME', self.home / '.local/share'))
        self.folder = self.data / 'galleoncore/fps'
        self.mango = self.config / 'MangoHud/MangoHud.conf'
        self.backups = self.data / 'galleoncore/fps-setup-backups'
        self.rows = []
        self.row_ids = set()

    def options(self):
        return 'autostart_log=1,log_interval=500,output_folder=' + str(self.folder) + ',background_alpha=0,alpha=0'

    def add(self, group, label, kind, path, extra='', enabled=True, note=''):
        if len(self.rows) >= MAX_ROWS:
            return
        path = Path(path)
        rowid = digest((kind + '\0' + str(path) + '\0' + extra).encode())
        if rowid in self.row_ids:
            return
        self.row_ids.add(rowid)
        row = dict(id=rowid, group=group, label=label[:240], kind=kind, path=str(path),
                   extra=extra, enabled=enabled, note=note)
        try:
            original = read(path, missing=True)
            row['fingerprint'] = digest(original)
            safe_target(path, self.home)
        except (OSError, SetupError) as error:
            row.update(enabled=False, note=str(error))
        self.rows.append(row)

    def steam(self, root, label, flatpak=False):
        if not root.is_dir():
            return
        # Resolve Steam's conventional directory aliases once, then validate all
        # actual write targets. External game libraries are read-only metadata.
        root = root.resolve()
        libraries = [root]
        try:
            _, tree = vdf(read(root / 'steamapps/libraryfolders.vdf'))
            for node in child(tree, 'libraryfolders').values():
                if isinstance(node[0], dict):
                    path = node[0].get('path')
                    if path and Path(path[0]).is_absolute():
                        libraries.append(Path(path[0]))
        except (OSError, ValueError, TypeError, SetupError):
            pass
        games = {}
        for library in list(dict.fromkeys(libraries))[:16]:
            for manifest in sorted((library / 'steamapps').glob('appmanifest_*.acf'))[:500]:
                try:
                    _, tree = vdf(read(manifest)); app = child(tree, 'AppState')
                    appid = app['appid'][0]; name = app['name'][0]
                    if not re.fullmatch(r'[1-9][0-9]{0,9}', appid):
                        continue
                    if re.search(r'proton|steam linux runtime|steamworks|redistributable|soldier|sniper', name, re.I):
                        continue
                    games[appid] = name
                except (OSError, ValueError, TypeError, KeyError, SetupError):
                    continue
        for user in sorted((root / 'userdata').glob('*'))[:32]:
            if not user.name.isdecimal() or int(user.name) == 0:
                continue
            target = user / 'config/localconfig.vdf'
            for appid, name in sorted(games.items(), key=lambda item: item[1].casefold()):
                self.add(label, name + ' · compte ' + user.name, 'steam', target, appid,
                         note='MANGOHUD=1 %command%' if flatpak else 'mangohud %command%')
            shortcut_path = user / 'config/shortcuts.vdf'
            try:
                shortcuts = binary_shortcuts(read(shortcut_path))
                for index, game in list(shortcuts.items())[:512]:
                    if not isinstance(game[0], dict):
                        continue
                    name = game[0].get('appname')
                    if name and isinstance(name[0], str):
                        exe = game[0].get('exe')
                        options = game[0].get('launchoptions')
                        route = delegated_shortcut(exe[0] if exe and isinstance(exe[0], str) else '',
                                                   options[0] if options and isinstance(options[0], str) else '', self.home)
                        if route:
                            continue
                        self.add(label, name[0] + ' · hors boutique · compte ' + user.name,
                                 'steam-shortcut', shortcut_path, index,
                                 note='MANGOHUD=1 %command%' if flatpak else 'mangohud %command%')
            except (OSError, ValueError, TypeError, SetupError):
                pass


    def heroic(self, root, label):
        if not root.is_dir():
            return
        self.add(label, 'Paramètres généraux · futurs jeux', 'heroic-global', root / 'config.json')
        games = {}
        identities = {}
        imported = set()
        for source in (root / 'legendaryConfig/legendary/installed.json', root.parent / 'legendary/installed.json',
                       root / 'nile_config/installed.json'):
            try:
                obj = json_object(read(source))
                for appid, game in list(obj.items())[:2048]:
                    if isinstance(game, dict):
                        game_id = str(game.get('app_name', appid))
                        games[game_id] = str(game.get('title', appid))
                        directory = game.get('install_path')
                        executable = game.get('executable')
                        if isinstance(directory, str) and isinstance(executable, str) and directory and executable:
                            candidate = Path(directory) / executable.replace('\\', '/')
                            if candidate.is_absolute():
                                identities[game_id] = str(candidate.resolve())
            except (OSError, ValueError, TypeError, SetupError):
                pass
        try:
            installed = json_object(read(root / 'gog_store/installed.json')).get('installed', [])
            if not isinstance(installed, list):
                raise SetupError('Métadonnées GOG non reconnues')
            for game in installed[:2048]:
                appid = str(game.get('appName', game.get('app_name', '')))
                if appid:
                    games[appid] = str(game.get('title', appid))
        except (OSError, ValueError, TypeError, AttributeError, SetupError):
            pass
        for source in (root / 'gog_store/library.json', root / 'sideload_apps/library.json'):
            try:
                obj = json_object(read(source))
                for game in obj.get('games', [])[:2048]:
                    if game.get('is_installed'):
                        appid = str(game.get('app_name', game.get('appName', '')))
                        if appid:
                            games[appid] = str(game.get('title', appid))
                            executable = game.get('install', {}).get('executable')
                            if isinstance(executable, str) and Path(executable).is_absolute():
                                identities[appid] = str(Path(executable).resolve())
                                if source.parent.name == 'sideload_apps':
                                    imported.add(appid)
            except (OSError, ValueError, TypeError, AttributeError, SetupError):
                pass
        prefixes = {}
        def prefix_for(appid):
            if appid not in prefixes:
                if not re.fullmatch(r'[\w.-]{1,200}', appid) or appid in ('.', '..'):
                    return ('invalid', appid)
                try:
                    data = read(root / 'GamesConfig' / (appid + '.json'), missing=True)
                    profile = json_object(data).get(appid, {}) if data else {}
                    if not isinstance(profile, dict):
                        raise SetupError('Profil Heroic invalide')
                    prefix = profile.get('winePrefix')
                    if prefix is not None and not isinstance(prefix, str):
                        raise SetupError('Préfixe Heroic invalide')
                    prefixes[appid] = str(Path(prefix).resolve()) if prefix and Path(prefix).is_absolute() else prefix
                except (OSError, ValueError, TypeError, SetupError):
                    prefixes[appid] = ('invalid', appid)
            return prefixes[appid]
        preferred = {}
        for appid in sorted(imported):
            preferred[(games[appid].casefold(), identities[appid], prefix_for(appid))] = appid
        for appid, title in sorted(games.items(), key=lambda item: item[1].casefold()):
            winner = preferred.get((title.casefold(), identities.get(appid), prefix_for(appid)))
            if winner and appid != winner:
                continue
            if re.fullmatch(r'[\w.-]{1,200}', appid) and appid not in ('.', '..'):
                self.add(label, title, 'heroic', root / 'GamesConfig' / (appid + '.json'), appid)

    def scan(self):
        self.rows = []
        self.row_ids = set()
        flatpaks = installed_flatpaks()
        native = {name: native_installed(name, self.home, self.data)
                  for name in ('steam', 'heroic', 'lutris', 'bottles', 'playonlinux', 'crossover')}
        self.add('Commun', 'Configuration MangoHud et dossier CSV', 'common', self.mango)
        roots = [self.home / '.steam/steam', self.home / '.steam/root', self.data / 'Steam']
        seen = set()
        if native['steam']:
            for root in roots:
                canonical = root.resolve()
                if canonical not in seen:
                    seen.add(canonical); self.steam(root, 'Steam · natif')
        apps = self.home / '.var/app'
        if 'com.valvesoftware.Steam' in flatpaks:
            root = apps / 'com.valvesoftware.Steam/data/Steam'
            if root.resolve() not in seen:
                self.steam(root, 'Steam · Flatpak', True)
        if native['heroic']:
            self.heroic(self.config / 'heroic', 'Heroic · natif')
        if 'com.heroicgameslauncher.hgl' in flatpaks:
            self.heroic(apps / 'com.heroicgameslauncher.hgl/config/heroic', 'Heroic · Flatpak')
        for root, label, installed in (
                (self.config / 'lutris/games', 'Lutris · natif', native['lutris']),
                (self.data / 'lutris/games', 'Lutris · natif', native['lutris']),
                (apps / 'net.lutris.Lutris/config/lutris/games', 'Lutris · Flatpak', 'net.lutris.Lutris' in flatpaks)):
            if not installed:
                continue
            for path in sorted(set(root.glob('*.yml')) | set(root.glob('*.yaml')))[:256]:
                try:
                    if lutris_executable_exists(read(path)) is False:
                        continue
                except (OSError, ValueError, SetupError):
                    pass  # Keep unknown configurations for an explicit manual result.
                self.add(label, path.stem, 'lutris', path)
        for root, label, installed in (
                (self.data / 'bottles/bottles', 'Bottles · natif', native['bottles']),
                (apps / 'com.usebottles.bottles/data/bottles/bottles', 'Bottles · Flatpak', 'com.usebottles.bottles' in flatpaks)):
            if not installed:
                continue
            for path in sorted(root.glob('*/bottle.yml'))[:256]:
                self.add(label, path.parent.name + ' · toute la bouteille', 'bottles', path,
                         note='MangoHud s’applique à tous les programmes de cette bouteille')
        if native['playonlinux']:
            pol = self.home / '.PlayOnLinux'
            for path in sorted((pol / 'shortcuts').glob('*'))[:256]:
                if path.is_file() and not path.is_symlink():
                    self.add('PlayOnLinux 4', path.name, 'pol', pol / 'configurations/pre_shortcut' / path.name)
        if native['crossover']:
            for path in sorted((self.home / '.cxoffice').glob('*/cxbottle.conf'))[:256]:
                self.add('CrossOver · natif', path.parent.name + ' · toute la bouteille', 'crossover', path,
                         note='Vulkan/DXVK uniquement ; tous les programmes de cette bouteille. OpenGL : configuration manuelle.')
        for appid in sorted(flatpaks):
            self.add('Permissions Flatpak', FLATPAKS[appid], 'flatpak', self.data / 'flatpak/overrides' / appid,
                     appid, note='Lecture MangoHud.conf et écriture CSV uniquement ; extension MangoHud requise')
        return self.rows

    def running(self, group):
        needles = {'Steam': ('steam', 'steamwebhelper'), 'Heroic': ('heroic',),
                   'Lutris': ('lutris',), 'Bottles': ('bottles',), 'PlayOnLinux': ('playonlinux',), 'CrossOver': ('crossover', 'cxrun', 'wine', 'wine64', 'wineserver')}
        prefix = next((key for key in needles if group.startswith(key)), None)
        if not prefix:
            return False
        for proc in Path('/proc').glob('[0-9]*'):
            try:
                if proc.stat().st_uid != os.getuid():
                    continue
                name = (proc / 'comm').read_text().strip().lower()
                args = (proc / 'cmdline').read_bytes()[:8192].split(b'\0')[:3]
                words = [Path(arg.decode(errors='replace')).name.lower() for arg in args]
                if name in needles[prefix] or any(word in needles[prefix] for word in words):
                    return True
            except OSError:
                continue
        return False

    def update(self, row, original):
        kind = row['kind']
        if kind == 'common':
            if any(char in str(self.folder) for char in ',\n\r\0'):
                raise SetupError('Chemin CSV incompatible avec les options MangoHud')
            text = original.decode()
            lines = [line for line in text.splitlines() if not re.match(
                r'^\s*(autostart_log|log_interval|output_folder|background_alpha|alpha)\s*=', line)]
            return ('\n'.join(lines + ['autostart_log=1', 'log_interval=500',
                     'output_folder=' + str(self.folder), 'background_alpha=0', 'alpha=0']) + '\n').encode()
        if kind == 'crossover':
            return patch_crossover(original, self.mango)
        if kind in ('steam', 'steam-shortcut'):
            existing_galleon = 'MANGOHUD_CONFIG=' + shlex.quote(self.options()) + ' mangohud %command%'
            patch = patch_steam if kind == 'steam' else patch_shortcut
            return patch(original, row['extra'], row['note'], (existing_galleon,))
        if kind.startswith('heroic'):
            obj = json_object(original) if original else {}
            if kind == 'heroic-global':
                settings = obj.setdefault('defaultSettings', {})
            else:
                if obj.get('version', 'v0') not in ('v0', 'v0.1'):
                    raise SetupError('Version Heroic inconnue')
                settings = obj.setdefault(row['extra'], {})
                if not original:
                    obj['version'] = 'v0.1'; obj['explicit'] = False
            if not isinstance(settings, dict):
                raise SetupError('Réglages Heroic invalides')
            if kind == 'heroic' and 'enviromentOptions' not in settings:
                global_path = Path(row['path']).parent.parent / 'config.json'
                if global_path.exists():
                    globals_obj = json_object(read(global_path))
                    inherited = globals_obj.get('defaultSettings', {}).get('enviromentOptions', [])
                    settings['enviromentOptions'] = inherited
            env = settings.setdefault('enviromentOptions', [])
            if not isinstance(env, list) or len(env) > 256:
                raise SetupError('Variables Heroic non reconnues')
            keys = set()
            for entry in env:
                if not isinstance(entry, dict) or not isinstance(entry.get('key'), str) or entry['key'] in keys:
                    raise SetupError('Variables Heroic ambiguës')
                keys.add(entry['key'])
                if entry['key'] == 'MANGOHUD_CONFIG' and entry.get('value') != self.options():
                    raise Preserved('MANGOHUD_CONFIG personnel conservé : action manuelle nécessaire')
            settings['showMangohud'] = True
            desired = {'MANGOHUD_CONFIG': self.options()}
            for key, value in desired.items():
                if key not in keys:
                    env.append(dict(key=key, value=value))
            return (json.dumps(obj, ensure_ascii=False, indent=2) + '\n').encode()
        if kind in ('lutris', 'bottles'):
            section = 'system' if kind == 'lutris' else 'Parameters'
            yaml_environment_conflict(original, 'system/env' if kind == 'lutris' else 'Environment_Variables')
            updated = yaml_patch(original, section, 'mangohud', True)
            env = 'system/env' if kind == 'lutris' else 'Environment_Variables'
            if kind == 'lutris':
                # Ensure the nested mapping exists before adding a variable.
                try:
                    updated = yaml_patch(updated, env, 'MANGOHUD_CONFIGFILE', str(self.mango))
                except SetupError as error:
                    if str(error) != 'Section YAML imbriquée absente':
                        raise
                    updated = yaml_patch(updated, 'system', 'env', '__GALLEON_EMPTY_MAP__')
                    updated = updated.replace(b'"__GALLEON_EMPTY_MAP__"', b'{}')
                    updated = yaml_patch(updated, env, 'MANGOHUD_CONFIGFILE', str(self.mango))
            else:
                updated = yaml_patch(updated, env, 'MANGOHUD_CONFIGFILE', str(self.mango))
            return updated
        if kind == 'pol':
            # Pre-shortcut scripts are never executed or parsed as shell code.
            if original.strip():
                expected = self.pol_script()
                if original == expected:
                    return original
                raise Preserved('Commande de pré-lancement personnelle conservée : action manuelle nécessaire')
            return self.pol_script()
        raise SetupError('Action inconnue')

    def pol_script(self):
        return ('# GalleonCore MangoHud\nexport MANGOHUD_CONFIG=' + shlex.quote(self.options())
                + '\nexport BEFORE_WINE="mangohud${BEFORE_WINE:+ $BEFORE_WINE}"\n').encode()

    def backup(self, path, original, updated, group):
        safe_target(self.backups / 'placeholder', self.home)
        self.backups.mkdir(parents=True, exist_ok=True, mode=0o700)
        token = uuid.uuid4().hex
        exists = path.exists()
        mode = stat.S_IMODE(path.stat().st_mode) if exists else 0o600
        backup = dict(path=str(path), original=original.hex(), updated=digest(updated),
                      existed=exists, mode=mode, group=group)
        atomic(self.backups / (token + '.json'), json.dumps(backup).encode())
        return token

    def write(self, path, original, updated, group=""):
        path = safe_target(path, self.home)
        if read(path, missing=True) != original:
            raise SetupError('Fichier modifié depuis la détection : actualisez la liste')
        if updated == original:
            return ''
        token = self.backup(path, original, updated, group)
        mode = stat.S_IMODE(path.stat().st_mode) if path.exists() else 0o600
        # Check again after backup, immediately before replacing the file.
        if read(path, missing=True) != original:
            raise SetupError('Fichier modifié pendant la préparation : aucune écriture effectuée')
        atomic(path, updated, mode)
        return token

    def common_ready(self):
        if not self.folder.is_dir():
            return False
        try:
            options = {}
            for line in read(self.mango).decode().splitlines():
                match = re.match(r'^\s*(autostart_log|log_interval|output_folder)\s*=\s*(.*?)\s*$', line)
                if match:
                    options[match[1]] = match[2]
            return options == dict(autostart_log='1', log_interval='500', output_folder=str(self.folder))
        except (OSError, ValueError, SetupError):
            return False

    def apply(self, selected):
        current = {row['id']: row for row in self.scan()}
        results = []
        files = {}
        # Group writes by file: hundreds of Steam games may share an 8 MiB
        # localconfig. Patch in memory, then back up and replace the file once.
        for wanted in selected[:MAX_ROWS]:
            row = current.get(wanted.get('id'))
            if row is None:
                results.append(dict(id=wanted.get('id'), label=wanted.get('label', '')[:240],
                                    status='error', detail='Élément disparu : actualisez la liste'))
            else:
                files.setdefault(row['path'], []).append((row, wanted))
        for path, entries in files.items():
            file_results = []
            changed = []
            try:
                original = read(path, missing=True)
                updated = original
                for row, wanted in entries:
                    result = dict(id=row['id'], label=row['label'], status='error', detail='')
                    file_results.append(result)
                    try:
                        if not row['enabled']:
                            raise SetupError(row['note'])
                        if self.running(row['group']):
                            raise SetupError('Fermez complètement ce lanceur puis réessayez')
                        if row['fingerprint'] != wanted.get('fingerprint') or digest(original) != row['fingerprint']:
                            raise SetupError('Fichier modifié depuis la détection : actualisez la liste')
                        if row['kind'] != 'common' and not self.common_ready():
                            raise SetupError('Sélectionnez d’abord la configuration commune MangoHud')
                        if row['kind'] == 'flatpak':
                            result.update(self.allow_flatpak(row))
                            continue
                        if row['kind'] == 'common':
                            safe_target(self.folder / 'placeholder', self.home)
                            self.folder.mkdir(parents=True, exist_ok=True)
                        candidate = self.update(row, updated)
                        if candidate != updated:
                            changed.append(result)
                        updated = candidate
                        result.update(status='unchanged', detail='Déjà configuré', common=row['kind'] == 'common')
                    except Preserved as error:
                        result.update(status='preserved', detail=str(error))
                    except (OSError, ValueError, TypeError, KeyError, SetupError) as error:
                        result['detail'] = str(error)
                if changed:
                    if self.running(entries[0][0]['group']):
                        raise SetupError('Le lanceur a démarré pendant la préparation : aucune écriture effectuée')
                    token = self.write(path, original, updated, entries[0][0]['group'])
                    for result in changed:
                        result.update(status='changed', detail='Configuré', backup=token)
            except (OSError, ValueError, TypeError, KeyError, SetupError) as error:
                if not file_results:
                    file_results = [dict(id=row['id'], label=row['label'], status='error', detail=str(error))
                                    for row, _ in entries]
                for result in changed:
                    result.update(status='error', detail=str(error))
                    result.pop('common', None)
            results.extend(file_results)
        return results

    def allow_flatpak(self, row):
        # Use the supported CLI; snapshot overrides first for conflict-safe undo.
        path = safe_target(row['path'], self.home)
        original = read(path, missing=True)
        if digest(original) != row['fingerprint']:
            raise SetupError('Permissions modifiées depuis la détection : actualisez la liste')
        token = self.backup(path, original, original, '')
        error = ''
        try:
            result = subprocess.run(['flatpak', 'override', '--user',
                '--filesystem=' + str(self.mango.parent) + ':ro', '--filesystem=' + str(self.folder),
                '--env=MANGOHUD_CONFIGFILE=' + str(self.mango), row['extra']],
                capture_output=True, timeout=8, check=False)
            if result.returncode:
                error = 'Échec des permissions Flatpak : ' + result.stderr.decode(errors='replace')[:300]
        except (OSError, subprocess.TimeoutExpired):
            error = 'Permissions Flatpak non confirmées : vérifiez les accès avant de réessayer'
        current = read(path, missing=True)
        if current == original:
            (self.backups / (token + '.json')).unlink()
            token = ''
        else:
            entry = self.backups / (token + '.json')
            record = json_object(read(entry, limit=2 * MAX_BYTES + 4096))
            record['updated'] = digest(current)
            atomic(entry, json.dumps(record).encode())
        if error:
            return dict(status='error', detail=error, backup=token)
        return dict(status='changed' if token else 'unchanged',
                    detail='Accès autorisés ; relancez le lanceur. Extension MangoHud du runtime requise.', backup=token)

    def restore(self, token):
        if not re.fullmatch('[0-9a-f]{32}', token):
            raise SetupError('Sauvegarde invalide')
        entry = self.backups / (token + '.json')
        safe_target(entry, self.home)
        obj = json_object(read(entry, limit=2 * MAX_BYTES + 4096))
        path = safe_target(obj['path'], self.home)
        if self.running(obj.get('group', '')):
            raise SetupError('Fermez complètement ce lanceur avant de restaurer')
        if digest(read(path)) != obj['updated']:
            raise SetupError('Le fichier a changé depuis l’application : restauration refusée')
        original = bytes.fromhex(obj['original'])
        if obj['existed']:
            atomic(path, original, obj['mode'])
        else:
            path.unlink()
        entry.unlink()
        return dict(status='restored', detail='Configuration précédente restaurée')


def main():
    setup = Setup()
    request = json.loads(sys.stdin.buffer.read(1024 * 1024))
    action = request.get('action')
    # Serialize setup sessions without blocking the GUI or a second Bosun.
    lock = None
    if action in ('apply', 'restore'):
        import fcntl
        lock_path = safe_target(setup.data / 'galleoncore/fps-setup.lock', setup.home)
        lock_path.parent.mkdir(parents=True, exist_ok=True)
        lock = lock_path.open('a')
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise SetupError('Une autre configuration FPS est déjà en cours') from error
    if action == 'scan':
        result = dict(rows=setup.scan())
    elif action == 'apply':
        result = dict(results=setup.apply(request.get('selected', [])))
    elif action == 'restore':
        result = setup.restore(request.get('token', ''))
    else:
        raise SetupError('Action inconnue')
    print(json.dumps(result, ensure_ascii=False))
    if lock is not None:
        lock.close()


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, TypeError, KeyError, SetupError, subprocess.TimeoutExpired) as error:
        print(json.dumps(dict(error=str(error)), ensure_ascii=False))
        sys.exit(1)
