<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Profiles

A profile is a **complete pages file** (screen, screensaver, themes, pages, dials): the same format as `pages.json`, so everything that a pages
file can do, a profile can do. One profile is active at a time; `galleond` shows it on the module. The person chooses: nothing reads the active
window, the applications that run or any key that is typed.

*State (28 November 2026):* the daemon side, the logic of Bosun's backend and Bosun's window are done and tested: a **profile menu at the top of the
window** (next to the language) and **one entry per profile in the menu of the notification area**. **Not done yet:** the editor of the "profile" key
(a key that goes to a profile, to the next or to the previous one is written in the pages file by hand for now).

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

## What Bosun's backend does (`BosunBackend`)

- With the profiles on (not in a preview run, and not when `--pages <file>` was given), the editor, the themes, the screensaver and the installation or
  export of a theme work on the file of the **active profile**; the configuration folder stays the fixed place of `pages.json`, `profiles/`, the curves,
  the icons and the videos. The window follows `profile-active.json`: a key of the module that changes the profile changes what the window edits.
- `setProfile(name)` remembers the choice and edits that profile from then on; the window then asks `galleond` to show it (`DaemonStatus::setProfile`, D-Bus
  on Linux, a local command on Windows). With no `galleond`, the remembered choice is read at its next start.
- `createProfile(name, from)` makes a copy (the new profile is not made active), `renameProfile` and `deleteProfile` refuse the **active** profile (the window
  switches to another one first, so that `galleond` never has its file taken away), and `default` can be copied but neither renamed nor deleted. A deleted
  profile is **moved** to `profiles/.deleted/<name>-<time>.json`, never erased, and never replaces a file that is already there.
- A copy is written whole or not at all, and is the person's own (not writable by others, as a pages file must be to be trusted).

## In Bosun's window

- **The menu** ("Profile: gaming ▾", left of the language menu, only when profiles are on): the list of the profiles with a tick on the one in use, then
  *New profile (a copy of this one)…*, *Rename this profile…* and *Delete this profile…* (the last two are not offered for `default`). The three open a small
  question with a name to type (or a confirmation to delete) and the reason of a refusal in the language of the person.
- Choosing a profile makes every tab edit it and asks `galleond` to show it. **Renaming or deleting the profile in use first goes back to `default`** (and asks
  `galleond` to show it), so that `galleond` never has its file taken away; a refused change puts the window back on the profile it was on.
- **The menu of the notification area** has one entry per profile ("Profile: gaming"), the one in use ticked, when there are at least two profiles.
- The tab bar keeps room for the menu; without profiles (a preview, or `--pages <file>`) the window is as it was.

## Privacy and safety

The profile is the person's choice. Nothing is read from the system to decide it, nothing is sent anywhere, and no event is returned to a
caller: the answers are `true` or `false`, the list and the active name.
