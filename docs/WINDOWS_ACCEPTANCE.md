<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Windows acceptance checklist (before 1.0)

Hand tests on a **real Windows machine with the real hardware** (keyboard `1b1c:2b0c` and Stream Deck module `1b1c:2b18`).
A successful Windows build, and the native CI, are **not** hardware validation (see [`WINDOWS.md`](WINDOWS.md)): this
file is what turns "it builds and the tests pass" into "it works on the desk". It must be **fully run, and its required
rows passed or explicitly accepted, before the 1.0 release.**

It covers everything added recently, most of which was only seen on Linux so far. Where a feature has **no effect on
Windows yet**, the row says so: the expected result is then the honest message, and that is what is checked.

## How to use it

- Follow the rows **in order, line by line**. Do not skip a step to save time.
- **Write only what you saw.** A result is `Pass`, `Fail`, `Not run`, `Blocked` (with the reason), or `N/A on this version`
  (the feature does not exist on Windows yet; the expected result is the message or the absence of any effect).
- Nothing is filled in "because it should work". An unfilled row stays `Not run`.
- A `Fail` gets a GitHub issue (link it in the Notes), with what was captured (below). Do not retry until it passes:
  record the failure, then the retry as a new line.
- The tester fills the **Result** and **Notes** columns, or sends them to the maintainer, who writes them here. The
  **Progress** table below is updated each time, so that everyone can follow where the tests stand.
- **Privacy.** Nothing in this checklist needs the keys that were typed, and none of the files named here contains any
  (the diagnostic trace records report sizes and the first bytes, never the typed text). Do not attach screenshots of
  anything private.

## Progress

Rows by section. Updated when results are recorded. `R` = required for 1.0; the others are `A` = advisory.

| Section | Rows | Pass | Fail | Blocked | N/A | Not run |
| --- | --- | --- | --- | --- | --- | --- |
| 0. Environment | 1 | 0 | 0 | 0 | 0 | 1 |
| 1. Install and upgrade | 5 | 0 | 0 | 0 | 0 | 5 |
| 2. Module (keys, screen, dials) | 6 | 0 | 0 | 0 | 0 | 6 |
| 3. Dial rings | 5 | 0 | 0 | 0 | 0 | 5 |
| 4. Live lighting (keyboard) | 12 | 0 | 0 | 0 | 0 | 12 |
| 5. Restore at start | 7 | 0 | 0 | 0 | 0 | 7 |
| 6. Features without effect on Windows yet | 4 | 0 | 0 | 0 | 0 | 4 |
| 7. Coexistence | 3 | 0 | 0 | 0 | 0 | 3 |
| **Total** | **43** | **0** | **0** | **0** | **0** | **43** |

**Release gate.** All `R` rows are `Pass`, or each one that is not has a recorded decision of the maintainer to accept it
for 1.0 (with the reason), and every `Fail` has an open issue or a fix. Nothing in this table is a promise about when.

## 0. Environment (fill once, before the first row)

| Item | Value |
| --- | --- |
| Tester | |
| Date | |
| Windows version and build (`winver`) | |
| Installer used (NSIS Setup / ZIP) and its version | |
| `galleond` version (file properties of `galleond.exe`, Details tab) | |
| Bosun version (Bosun's Updates panel) | |
| Keyboard firmware (read by the Keyboard tab of Bosun) | |
| Module firmware (Bosun) | |
| Corsair Web Hub installed? iCUE? OpenRGB? | |
| Windows **Dynamic Lighting** setting (Settings > Personalization > Dynamic Lighting) | On / Off |
| Antivirus / SmartScreen messages seen at install | |

| ID | Check | Req. | Result | Notes |
| --- | --- | --- | --- | --- |
| 0.1 | The table above is filled, nothing left blank | R | Not run | |

## Capturing evidence

- **Settings and state files** are in the `galleoncore` folder of your user's local application data (find the folder that
  holds `pages.json`). The ones used below are `lighting-restore.json`, `lighting-memory.json`, `lighting-state.json`,
  `dial-rings.json`, `lighting-appearance.json`. They are small and hold no typed text.
- **Diagnostic trace of the live lighting** (off unless asked for): set the environment variable `GALLEON_LIGHTING_LOG=1`
  for `galleond`, restart it, reproduce, then collect `lighting-*.log` from `%LOCALAPPDATA%\GalleonCore\logs` (256 KiB at
  most; it holds no serial number and no path). Turn it off again afterwards.
- **Processes**: Task Manager > Details: `galleond.exe`, `galleon-bosun.exe`, `galleon-keyboard-watchdog.exe`.
- Note the **time** of each action: the trace and the notes are matched by it.

## 1. Install and upgrade

| ID | Check | Expected | Req. | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| 1.1 | Run the NSIS Setup on a clean user. Note any SmartScreen / antivirus message. | Installs per user, no administrator needed. **Record exactly what Windows said** (the code signature is not done yet). | R | Not run | |
| 1.2 | After install: `galleon-keyboard-watchdog.exe` is in the install folder **beside** `galleond.exe`. | Present. (`test-setup.ps1` checks this in the CI; this is the hand check on the real install.) | R | Not run | |
| 1.3 | Upgrade over a running install, with Bosun and galleond running. | Both are asked to stop and exit by themselves (no force-kill), the files are replaced, personal files and settings are kept, the new version starts. | R | Not run | |
| 1.4 | Uninstall with both running. | Both stop, the files are removed, the personal files and settings are **kept**. | R | Not run | |
| 1.5 | Sign out and in (only if the installer registered the start at sign-in). | Bosun starts minimized in the tray (`--minimized`), galleond starts with the session. No Windows service is created. | A | Not run | |

## 2. Module: keys, screen, dials

The first four rows repeat the older "before calling the port ready" list of [`WINDOWS.md`](WINDOWS.md): they were never
validated by a person on this version.

| ID | Check | Expected | Req. | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| 2.1 | Start galleond with the module plugged in. Press the 12 keys, turn and press the 2 dials. | The pages draw, the keys act, the dials turn and press. No frozen picture. | R | Not run | |
| 2.2 | Unplug the module, wait 10 s, plug it back. | galleond finds it again and redraws everything by itself. | R | Not run | |
| 2.3 | Change a theme with the dial, choose a wallpaper (press the dial: the right ring turns magenta, then white after 4 s or a second press). | As on Linux. | R | Not run | |
| 2.4 | Leave the PC idle until a video screensaver starts, then press a key. | The video plays on the screen and the 12 keys; the first key wakes it **and does not act**; the normal screen returns. | R | Not run | |
| 2.5 | Start a game, then check that the screensaver does not start. | The game mode keeps the screensaver off. | A | Not run | |
| 2.6 | Sleep the computer and wake it. | The module comes back with its pages; no stuck keys. | R | Not run | |

## 3. Dial rings

Bosun, Dials tab. The colour rule decided by the owner: the rings follow the **level**; with the rings **out** or at **0 %**
nothing relights them in normal use; the green rain is the one exception (at least 30 %).

| ID | Check | Expected | Req. | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| 3.1 | Left ring shows the theme colour, right ring white. Move the level slider. | Both follow the level. | R | Not run | |
| 3.2 | Put the rings **out**, then choose a wallpaper with the dial. | The right ring stays **dark** (no magenta). | R | Not run | |
| 3.3 | Set the level to **0 %**, then choose a wallpaper. | Both rings stay dark. | R | Not run | |
| 3.4 | Set a low level (10 %), choose a wallpaper. | The magenta is shown at **30 %** at least (readable). | A | Not run | |
| 3.5 | Check which program owns the rings: **Dynamic Lighting On**, then **Off**; OpenRGB closed, then open. Note who wins. | Not a pass/fail: record it. Windows Dynamic Lighting and OpenRGB's `Dynamic Light` device compete for the same rings and nothing here arbitrates between them. | A | Not run | |

## 4. Live lighting (keyboard)

**Experimental, off by default.** The keys are given back to the keyboard's own profile whenever the lighting stops.
Never test with the keyboard's only copy of an important setting: nothing here writes to its flash, but a frozen keyboard
can need an unplug.

| ID | Check | Expected | Req. | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| 4.1 | Bosun > Keyboard > live lighting: switch it **on**. | The state goes `starting`, `listening`, `taking`, then `holding` after about 3 s; the keys take the colour. | R | Not run | |
| 4.2 | Choose a colour, the rainbow, the sky. | The keys follow. | R | Not run | |
| 4.3 | Switch it **off** from Bosun. | The keys go **back to the keyboard's own profile** (not left on the last colour). | R | Not run | |
| 4.4 | With it on, **end `galleond.exe` in Task Manager** (End task, not a clean stop). | Within a few seconds the **watchdog** gives the keys back (profile colours). **If the keys stay on the last colour, that is a failure**: capture the trace and say whether `galleon-keyboard-watchdog.exe` was running. | R | Not run | |
| 4.5 | With it on, **shut the PC down** (not restart), then power on. | The keys are given back at shutdown; the keyboard shows its own profile before galleond starts. Record what you saw. | R | Not run | |
| 4.6 | With it on, **sleep and wake**. | Keys given back before sleeping; after waking they are taken again by galleond (waits up to 30 s for the keyboard). State `sleeping`, then `holding`. | R | Not run | |
| 4.7 | With it on, **lock the session** (Win+L), wait 1 min, unlock. | No freeze, no flicker; the lighting is still `holding`. | R | Not run | |
| 4.8 | With it on, **unplug the keyboard for 10 s** and plug it back. | galleond does not crash; either it recovers or it goes `off` with a reason. Record which. | R | Not run | |
| 4.9 | Open the **Corsair Web Hub** while it is on. | galleond **gives way** (state `yielded`); no flicker with two writers. | R | Not run | |
| 4.10 | Leave it on for one hour, doing normal work. | No freeze, no failed write in the trace. | A | Not run | |
| 4.11 | Stop it and start it **10 times**. | Every stop gives the keys back, every start takes them. | A | Not run | |
| 4.12 | Switch it on with the **sleep notification** unavailable (if you can arrange it) or the watchdog removed. | **Refused with a reason**, the keyboard is never opened. | A | Not run | |

## 5. Restore at start (new since 0.3.0)

Bosun > Keyboard > "Put the live lighting back at start": **off by default**. The memory file `lighting-memory.json` says
`off`, `running` or `clean`; the choice is in `lighting-restore.json`. The lighting is put back only when the box is
ticked **and** galleond last ended properly with the lighting on.

| ID | Check | Expected | Req. | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| 5.1 | Fresh install: look at the box and at `galleoncore`. | The box is **unticked**; no `lighting-restore.json`. Merely looking at the tab writes nothing. | R | Not run | |
| 5.2 | Tick the box, switch the lighting on, then **restart galleond cleanly** (from the tray or Bosun's stop). | `lighting-memory.json` says `clean` after the stop, `off` right after the start, `running` about 3 s later; the keys light again by themselves. | R | Not run | |
| 5.3 | Same, then **end `galleond.exe` in Task Manager** and start it again. | The lighting is **not** put back; the memory reads `off` after the start. This is the guard that matters most. | R | Not run | |
| 5.4 | Same, with the box **unticked**. | Never put back. | R | Not run | |
| 5.5 | With the box ticked and the lighting on, **sign out, then in**. | **Record what happens** and what `lighting-memory.json` says before and after. It is not known whether Windows lets galleond end cleanly at sign-out, which decides whether the lighting comes back. | R | Not run | |
| 5.6 | Same with **shutdown and power on**. | Same: record. | R | Not run | |
| 5.7 | Start galleond with the **keyboard unplugged**, plug it in within 30 s. | The lighting is put back once it answers; after 30 s without it, it stays off with a reason in the log. | A | Not run | |

## 6. Features without effect on Windows yet

These exist on Linux. On Windows the local endpoint has no such action, so Bosun says **"not available on Windows yet"**.
**Expected here: that message, and no change of any kind.** If one of them does something, that is a surprise to record.
The row becomes a real test when the feature is brought to Windows.

| ID | Feature | What to do | Expected now | Req. | Result | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| 6.1 | Lights off until morning (module and rings; keyboard while the live lighting is on) | Bosun > Screensavers > "Lights off until morning": choose a time, press the button. | The message, and the module, the rings and the keyboard **do not change**. No `night-mode.json`. | A | Not run | |
| 6.2 | The module follows the open theme tab | Open several theme tabs in Bosun. | The module does **not** change. | A | Not run | |
| 6.3 | "Test" button of a screensaver video | Press it on a video. | The message; nothing plays on the module. | A | Not run | |
| 6.4 | Key calibration hidden unless `--mapkeys` | Open the Keyboard tab; then start `galleon-bosun.exe --mapkeys`. | Hidden without the option; shown with it. (This one works on Windows: it is Bosun only.) | A | Not run | |

## 7. Coexistence

| ID | Check | Expected | Req. | Result | Notes |
| --- | --- | --- | --- | --- | --- |
| 7.1 | Web Hub open **and idle** with the live lighting on, for 2 min. | galleond may resume after its quiet period (about 60 s) and **yield again** at the Web Hub's next command; the Web Hub preview may briefly show galleond's frame. Record. | A | Not run | |
| 7.2 | Another RGB program owns the keyboard first (Web Hub or another host in mode 2), then switch galleond's lighting on. | galleond waits for the keyboard to be free (`listening`) or refuses; it never fights. | R | Not run | |
| 7.3 | Run **galleond and Aaryush1's helper** (or another writer) together, if available. | Not a pass/fail: a second writer in the same mode is **not detected** by galleond (it only sees the mode and foreign replies), so expect flicker, and record it. The planned local API will remove the need for a second writer. | A | Not run | |

## After the tests

- Send the results (or this file filled in). The maintainer records them here, updates the **Progress** table, and links the
  issues for the failures. Anything not run stays `Not run`.
- A change of code that touches an area above sends its rows back to `Not run`.
- This file is referenced from `ROADMAP.md` (section 6) and from [`WINDOWS.md`](WINDOWS.md).
