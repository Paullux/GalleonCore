<!-- SPDX-FileCopyrightText: 2026 Paul Woisard -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Windows Game Bar-first detection experiment (not ready for production)

This branch is **an integration seam and fallback test**, not a functioning Xbox Game Bar widget.
The Windows Setup and ZIP built from this branch still use the existing process-scanning
detector unless an external source writes the experimental report.

## Rationale

The official Xbox Game Bar SDK provides `XboxGameBarAppTargetTracker` with
`GetTarget()`, `TargetChanged`, `IsGame` and `DisplayName`. Its constructor
**requires a `XboxGameBarWidget` context**. Consequently galleond cannot
directly call this API from an ordinary Qt Win32 executable. An independently
installed UWP/XAML Game Bar widget and a supported local transport would be
required for live reports; neither is packaged by this branch.

SDK: https://learn.microsoft.com/en-us/xbox/game-bar/api/xgb-apptargettracker

## Experimental report contract

A potential widget companion would publish a small JSON object to the file
`widgets.json.gamebar-target.json`, adjacent to the usual `widgets.json` config
file. It contains:

```json
{"at": 0, "isGame": true, "displayName": "Example Game"}
```

`at` is milliseconds since the Unix epoch (the example zero is deliberately
invalid). Every report must be refreshed at least every five seconds while the
user is focusing the game. There are size, type, age, and title bounds.

On Windows, a fresh `isGame=true` report with a nonempty `displayName`
causes GalleonCore to resolve the **actual foreground window PID** and executable
using Win32. The report **never controls the PID**, capture process, command
line or executable path. PresentMon still filters by the real executable; only
the game's displayed title and SteamGridDB search title are overridden.
If the report is missing/stale/invalid, the previous Steam/Epic/GOG/EA/
manual-profile discovery is preserved. The current limitation is that this
reports the foreground app; switching away from the game falls back to the
previous detector. Game Bar's active target may not map perfectly to every
game's rendering process. No guarantee of coverage for unrecognized games.

## Windows artifact diagnostic test (without the Game Bar widget)

1. Install the branch's `windows-desktop-x86_64` Setup artifact. This is
   **not** a full Xbox Game Bar integration yet.
2. Locate the actual `widgets.json` file used by Bosun/galleond.
3. Launch a supported game and, in PowerShell from the checked-out branch, run:

```powershell
.\tools\gamebar\simulate-target.ps1 -WidgetsJsonPath "C:\path\to\widgets.json" -DisplayName "Ghostrunner" -Seconds 60
```

4. The script gives five seconds to focus the game, then repeatedly writes an
   **artificial** Game Bar report for one minute. During the simulation, the
   captured game's title should come from this report, with its real PID and
   existing FPS/SteamGridDB processing unchanged. Once the script ends, the
   report expires after five seconds and the existing detector resumes.
5. Confirm fallback again without the script, and check that the Setup installs
   and uninstalls as before. If PresentMon is missing or no frames are captured,
   the Gaming screen may remain inactive even with a valid report.

Do **not** use this diagnostic file as a security boundary: it is intentionally
local and experimental. The eventual UWP widget must address deployment,
consent, background lifetime, application-target-to-PID correlation, and the
correct transport before switching on Game Bar automatically. No Game Bar
service, tracking permission, or new installer dependency is added here.
