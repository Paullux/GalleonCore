# Code signing policy

**Draft. Not published.** See `docs/signing/README.md`. Replace every `[TO CONFIRM: ...]` before use.

GalleonCore (the `galleond` daemon, the Galleon Bosun settings application, the `galleon-keyboard-watchdog` helper and the
Windows Setup) is free software under the GPL-3.0-or-later and LGPL-3.0-or-later licences. This page says how its Windows
binaries are signed.

## Attribution

[TO CONFIRM: use the exact wording and link required by SignPath's current terms.]
Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).

## What is signed

Only binaries built from the source code of this project, in the public repository `https://github.com/Paullux/GalleonCore`,
by its automated build (`.github/workflows/windows-core.yml`): `galleond.exe`, `galleon-bosun.exe`,
`galleon-keyboard-watchdog.exe` and the Setup. Third-party libraries shipped in the package (for example Qt) are not signed by
this project's certificate unless they are rebuilt from source here. Nothing is signed that was not built by the project.

## Roles

- **Authors and committers:** Paul Woisard (repository owner). [TO CONFIRM: other committers.]
- **Reviewers** of changes made by people who are not committers: [TO CONFIRM: name or "the owner"].
- **Approvers** of each signing request (every release is approved by hand before it is signed): [TO CONFIRM: name].

Everyone with write access to the repository or to the signing service uses multi-factor authentication.

## Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the
person installing or operating it. The following requests are made by the programs, and only as described:

- **Update check** (Galleon Bosun): asks `api.github.com` for the latest release of the project, on its own at intervals by default,
  and when the user presses the button. It sends nothing about the user or the computer beyond what any web request carries (the
  address of the computer). It can be switched off in the Updates panel of Bosun. [TO CONFIRM against the code: the switch exists
  and is off for package-managed installs.]
- **Weather widget** (galleond): only after the user enters a place; it sends that place's latitude and longitude to
  `api.open-meteo.com`.
- **Game artwork** (galleond): only after the user enters a SteamGridDB key; it sends the title or Steam identifier of a game to
  `steamgriddb.com`.

The configuration files stay in the user's own folder. No usage data, no identifier and no log is sent anywhere by the project.

## Changes to the system

The programs write only in the user's own profile. The Setup can start Galleon Bosun and the daemon at sign-in through the
user's `Run` registry key; this is announced in the Setup and removed by the uninstaller. On Linux, a udev rule that gives the
keyboard and the Stream Deck module to the user is installed by the packages. The experimental live lighting is off by default,
never writes to the keyboard's memory, and is protected by a watchdog process that gives the keys back if the daemon ends.

## Uninstalling

Use "Apps" in Windows settings, or run the uninstaller in the installation folder. The user's own files (pages, themes,
wallpapers, curves) are kept unless the user deletes them.

## Reporting a problem

[TO CONFIRM: contact address or the issue tracker `https://github.com/Paullux/GalleonCore/issues`.] A signed binary that does
anything that this page does not say is a defect, and will be reported to SignPath.
