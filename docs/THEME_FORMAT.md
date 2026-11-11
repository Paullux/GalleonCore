<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# GalleonCore theme file format (`.gc.theme`), version 1

A theme is a set of keys for the Stream Deck module, the colour that goes with it, what the two dials do, and the pictures it needs. A theme file is meant to be shared, so it is checked completely before anything is installed.

Status: the file reader and validator exist and are tested (`daemon/StoredZip.*`, `daemon/ThemePackage.*`, fuzz target `fuzz_theme_package`). Installing and exporting from Bosun come next. Nothing here has been confirmed on the module yet.

## The file

A `.gc.theme` file is a **zip archive whose entries are stored, not compressed** (`zip -0`). Icons and wallpapers are already compressed, and refusing compression closes the door to decompression bombs. Any zip tool opens it; to inspect one, rename it to `.zip`.

| Entry | Required | Content |
|---|---|---|
| `theme.json` | yes | The manifest, up to 128 KiB |
| `credits.txt` | no | Plain UTF-8 text for the author's notes and credits, up to 16 KiB |
| `icons/<name>.png` | if the manifest names it | A PNG key picture, up to 256 KiB and 512 x 512 pixels. At most 24 |
| `wallpapers/<name>.jpg` | no | A JPEG screen picture, up to 2 MiB and 4096 x 4096 pixels. At most 4 |

Nothing else is accepted. Every picture must be used by a key. Names use letters, digits, `.`, `_` and `-` only. The whole archive is at most 24 MiB, with at most 64 entries.

## The manifest

```json
{
  "formatVersion": 1,
  "id": "discord",
  "name": "Discord",
  "author": "A. Person",
  "license": "CC-BY-SA-4.0",
  "description": "Voice and chat keys for Discord",
  "color": "#5865F2",
  "keys": [
    { "key": 0, "label": "Mute", "icon": "mute.png", "action": { "type": "key", "key": "kp1" } },
    { "key": 1, "label": "Discord", "icon": "discord",
      "action": { "type": "app", "desktop": "discord.desktop",
                  "alternatives": ["com.discordapp.Discord.desktop", "discord_discord.desktop"] } }
  ],
  "dials": { "0": { "turn": { "type": "media", "command": "volume", "step": 2 } } }
}
```

- `formatVersion`: must be `1`.
- `id`: 1 to 32 characters among `a-z`, `0-9`, `_`, `-`. It becomes the page name when installed.
- `name` (up to 24 characters), `author` (64), `license` and `color` (`#RRGGBB`) are required. `description` is optional (300). The licence is an SPDX identifier such as `CC-BY-SA-4.0`, and says what the pictures and the theme are published under.
- `keys` and `dials` use the same fields as a page of the pages file (`pages.json`); see the README. The manifest is checked by the very same parser, so a theme can never say what a page cannot.
- An `icon` with a dot (`mute.png`) is a file of `icons/`; one without (`discord`) is a name of the desktop's icon theme.

## What a theme may do

A downloaded theme must not be able to run anything by itself. Allowed actions:

- media commands, `back`, `none`, `wallpaper-mode`;
- `page` pointing to the theme's own page only;
- `key`: a key of the fixed numeric keypad list;
- `steam`: a Steam game by its number;
- `app`: an application named by its **desktop file ID** (`firefox.desktop`), never by a path.

Refused: shell commands, URLs, absolute paths, other pages, switching themes, Steam or menu auto-fill, any field the format does not list.

### One application, several installations

The same program has a different desktop file ID depending on how it was installed (deb, Flatpak, Snap). An `app` action may list `alternatives` (up to 6 other IDs). Installing the theme picks the first ID, `desktop` then each alternative in order, that exists on the machine.

## Wallpapers

Wallpapers are JPEG only. When exporting, Bosun re-encodes every picture as JPEG at quality 80 or higher and no larger than the screen needs, which keeps files small. The reader does not trust that: it checks the sizes itself.

## Rights

Only share what you made or may redistribute. Do not include logos, characters, screenshots or footage of others (see `ASSETS.md`). A theme can be *for* an application without using its logo.
