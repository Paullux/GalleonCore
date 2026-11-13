# Images and videos

Rules for every image, video and animation stored in this repository: logos, wallpapers for the keyboard screen and keys, example clips.

## What may be added

- Original work, by you or by a contributor who agrees to its license.
- Work under a free license compatible with the project, with its source.
- Images or videos generated with AI tools, under the conditions below.

The default license for images and videos is **CC-BY-SA-4.0**. Declare it in `REUSE.toml` (or in a `.license` file next to the asset), together with the author. The project logo, `site/logo.svg`, and the icon of the settings application, `bosun/icons/galleon-bosun.svg` (a ship's wheel around the 12 keys, drawn by hand for the project), are exceptions: they are GPL-3.0-or-later, like the website and the code, with the SPDX header inside the file.

## What may not be added

- **Existing characters**, even redrawn or "inspired by": if someone looking at the picture names a film, series, game or manga character, it does not belong here. This covers distinctive costumes, helmets, creatures and emblems.
- **Trademarks and logos** of films, series, games, studios or brands, including in file names.
- **Real people**: no likeness of actors, celebrities or anyone else without their consent.
- **Extracts of films, series, trailers or games**, however short.
- Sexual content, and anything depicting a person who could be a minor in a suggestive way.

An instructional screenshot of a third-party settings interface may show its
product names and interface marks solely to identify the control being
documented. Keep its provenance and purpose next to the file; this exception
does not apply to wallpapers, screensavers, branding or promotional art.

Documentary screenshots of Galleon Bosun may likewise show icons of applications
and games discovered on the author's desktop. They illustrate the selector and
are not standalone art assets or endorsements. Keep provenance with the captures;
third-party icon and trademark rights remain with their respective owners.

Styles are free to use: manga, cyberpunk, space western, cinematic lighting. Characters, logos and footage are not.

## Where they go

Wallpapers go in [`public/wallpaper/`](public/wallpaper/), one folder per style, with a `PROVENANCE.md` listing each file. That folder is CC-BY-SA-4.0 as a whole.

Screensaver videos go in [`public/Screensavers/`](public/Screensavers/), with a `PROVENANCE.md` (tool, date, exact prompts). Same rules, same licence. A video is checked frame by frame for logos, brands and existing characters before it is added.

## Drawings generated from the project's own data

`bosun/icons/galleon-100-sd-overlay.svg` (the dark layer, with real holes), `bosun/icons/galleon-100-sd-map.svg` (the same over a white light, to look at) and `bosun/qml/KeyboardMapData.js` (what Bosun draws) are **generated** by `tools/keyboard-map/build_map.py` from `docs/protocol/lamps-*.csv` and `data/devices/galleon-100-sd.json`, which are this project's own measurements. They are GPL-3.0-or-later like the script, with the SPDX header written into each file; a test fails when they are out of date. They are **not** taken from, traced over or stylised from a photograph or a render of the product, and carry no logo or brand name: the key legends are those of the standard French layout, drawn by the script. Do not add a picture of the keyboard to the repository, original or "restyled": it would be a derivative of the maker's pictures and would carry their marks. Change the script and its data instead.

## AI-generated assets

- Do not name an existing work, character or artist in the prompt.
- Record the provenance next to the license: the tool, the date, and the prompt.
- Review the result with the rules above: a generated image can still resemble an existing character.

## Screensaver videos

The animated screensaver plays one vertical video over the screen and the keys (see the README). The same rules apply to any clip added to the repository: no existing characters, trademarks, real people or footage, provenance recorded (tool, date, prompt), CC-BY-SA-4.0. A clip generated or animated with an AI service is added only if that service's terms allow publishing it under this license. Your own clip for your own keyboard stays on your computer.

## Your own wallpapers

The settings application lets you import any image or video for your own keyboard. Those files stay on your computer, in `~/.local/share/galleoncore/`, and never go into this repository. What you put on your own keyboard is your responsibility.
