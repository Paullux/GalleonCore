<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Profiles

A profile is a **complete pages file** (screen, screensaver, themes, pages, dials): the same format as `pages.json`, so everything that a pages
file can do, a profile can do. One profile is active at a time; `galleond` shows it on the module. The person chooses: nothing reads the active
window, the applications that run or any key that is typed.

*State (28 November 2026):* the daemon side is done and tested (this page). **Bosun does not know about profiles yet**: its editor still reads and writes
`pages.json`, which is the profile `default`. The profile selector of Bosun is the next step.

## Where the files are

In the configuration folder of `galleoncore` (`~/.config/galleoncore` on Linux, the same folder of the user's configuration on Windows):

| File | What it is |
| --- | --- |
| `pages.json` | The profile `default`. It is the file that was always there: nothing is moved, and a person who never makes a profile sees no change. If it does not exist, `default` is the built-in pages, as at the first start. |
| `profiles/<name>.json` | Another profile. |
| `profile-active.json` | The profile that was active, remembered across restarts: `{"version": 1, "profile": "gaming"}`. Written by `galleond`. A file that is damaged, too large, with other keys or another version, or that names a profile with no file, is not used: `galleond` then starts on `default`. |

Icons and screensaver videos named in a profile are looked for next to `pages.json` (the same folder for every profile), not in `profiles/`.
`galleond --pages <file>` keeps to that one file and turns the profiles off.

## Names

`default`, or 1 to 32 characters among `a` to `z`, `0` to `9` and `-`, the first one not a `-`. No capital letter, no dot, no space, no
path separator: a valid name can only be one file of the `profiles` folder. A name is checked **before any path is built from it**, wherever it
comes from (a key of a pages file, D-Bus, the local endpoint of Windows). At most 16 profiles are listed, `default` included. The folder
`profiles/.deleted` is kept for deleted profiles (Bosun's next step) and is never listed.

## Changing profile

- **A key (or a dial press) of a pages file:** `{ "type": "profile", "profile": "gaming" }` goes to a profile; `{ "type": "profile", "step": 1 }`
  goes that many places on in the list, round (`-1`: the one before; from -16 to 16, never 0). Exactly one of `profile` and `step`. A dial turn cannot
  use it. A theme package (`.gctheme`) cannot contain it: a downloaded theme does not choose what the module shows as a whole.
- **D-Bus (Linux), `org.galleoncore.Deck`:** `SetProfile(name) -> bool`, `StepProfile(steps) -> bool`, `Profiles() -> list`, `ActiveProfile() -> name`.
- **Local endpoint (Windows):** `{"action": "profile", "name": "gaming"}` and `{"action": "profile-step", "steps": 1}`. The endpoint gives no
  answer: the active profile is in `profile-active.json`.

A change is refused, and nothing changes, when the name is not valid, the profile has no file, or its file has errors (the errors are logged, as
for `pages.json`). When it is done, the file is read and applied **even if its content is the same as the one in use**, the keys go back to its
home page, and the choice is remembered. A profile that is edited while it is in use is followed, as `pages.json` always was; a profile that was
left is not.

## Privacy and safety

The profile is the person's choice. Nothing is read from the system to decide it, nothing is sent anywhere, and no event is returned to a
caller: the answers are `true` or `false`, the list and the active name.
