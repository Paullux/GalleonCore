<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Exit checklist for GalleonCore 1.0.0

What has to be true before `v1.0.0` is tagged. The decisions are the maintainer's (27 November 2026); the state is
written as it is, and **nothing is marked "seen on the device" without the maintainer's report**. This file is the
checklist; [`ROADMAP.md`](../ROADMAP.md) (French) keeps the history and the design notes.

The 1.0.0 meaning (14 November 2026): full control of the Stream Deck module (declared done) **and** full control of
the RGB keyboard.

## 1. In the 1.0 (decided 27 November 2026)

| Item | State | Needs the keyboard? |
| --- | --- | --- |
| Keyboard effects engine (`*.keys-fx`), then the gaming mode | **Not started** | yes |
| Module screensaver synchronised on the keyboard (one box, off by default) | **Not started** | yes |
| Faithful keyboard preview in Bosun (same rendering as the real keyboard) | **Not started** (the animated preview exists and is not faithful) | yes |
| Live lighting, brightness and 24 h curve, lights off until morning, rings colours, key signals, keys glow under your fingers | Done in code and tests; device and Windows results are the rows of section 3 | already tried on the keyboard |

Each new item comes with its own pull request, tests, and the privacy review of `AGENTS.md` (an effect that reacts to
key presses is reviewed again: a frame that lights a given key reveals that it was pressed).

## 2. Not in the 1.0 (moves to 1.1)

- Adjustable duration of the glow fade (fixed 1.5 s today).
- Items B, C, D, F, G and I of the list in issue #111 (their author said they can wait).

## 3. Windows acceptance

Source of truth: [`WINDOWS_ACCEPTANCE.md`](WINDOWS_ACCEPTANCE.md) and its **Progress** table. Decision: **every `R`
(required) row is `Pass`, or is explicitly deferred to 1.1 by the maintainer, row by row, with the reason written in the
row.** Advisory (`A`) rows do not block.

State on 27 November 2026 (66 rows, 23 `Pass`, 0 `Fail`, 43 `Not run`). Required rows still `Not run`:

| Section | Required rows not run | Rows |
| --- | --- | --- |
| 0. Environment | 1 | 0.1 |
| 1. Install and upgrade | 4 | 1.1 to 1.4 |
| 2. Module (keys, screen, dials) | 5 | 2.1 to 2.4, 2.6 |
| 3. Dial rings | 3 | 3.1 to 3.3 |
| 4. Live lighting | 9 | 4.1 to 4.9 |
| 5. Restore at start | 6 | 5.1 to 5.5, 5.8 |
| 7. Coexistence | 1 | 7.2 |
| 6, 8, 9 | 0 | only advisory rows remain (6.4, 8.8, 9.6, 9.7) |
| **Total** | **29** | |

The numbers come from the table on the date above; if they differ from the table, the table is right.

## 4. Other conditions

- **Full validation** (`tools/check.py full`) of the exact tree of the release commit, and all CI checks green.
- **Website:** remove the "under construction" marks of the lighting section the day 1.0.0 is published (the elements
  with `data-under-construction`, in four languages), and re-read its "experimental, off by default" wording.
  `tools/test_site_under_construction.py` fails from version 1.0.0 until it is done.
- **Version and notes:** version in `CMakeLists.txt`, changelogs (DEB, RPM), release notes in the four languages.
- **Windows Setup stays unsigned** (SignPath declined on 26 November 2026): the release notes keep saying that
  SmartScreen warns on first launch. Not a blocker.
- **Translations:** German and Spanish are released as "prepared with AI assistance, native review welcome" if nobody
  has reviewed them (decision of 27 November 2026; review requests are issues #181 and #182). They do not block.
- **Open points to close or write down:** the `build-test` job time limit (30 minutes, left as is unless decided
  otherwise), the unknown cause of the occasional `deploy-pages` "Found 0 artifact(s)" (a diagnostic is in the
  workflow), endurance tests of `galleond`.

## 5. Order of work

1. Tidy the evidence: the maintainer runs the Windows rows above in the order of the file, the results are recorded
   with their own pull requests.
2. The three items of section 1, one pull request each, in the order the maintainer gives.
3. Release candidate: full validation, rows re-checked on the exact build, then the tag.
