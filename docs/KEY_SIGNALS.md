<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Lighting single keys from another program (Linux, D-Bus)

A shorter page for developers, in four languages, is on the website: [The pipe](https://galleoncore.xyz/key-signals.html) (`site/key-signals.html`). Keep it in step with this file.

galleond can light single keys for other programs, so that an assistant, a calendar or a script can say "mail" or "meeting" on a key without ever touching the keyboard. galleond stays the only program that writes to it.

**Off by default.** The person allows it in Bosun (Keyboard, live lighting: "Other programs may light keys"). Nothing here works while the live lighting is off, and nothing here switches it on.

## What a program can and cannot do

- It **sends** a colour for one key, for a time it chooses (1 second to 24 hours, mandatory). That is all.
- It **never receives** anything about the keyboard: not which keys are pressed, not what is lit, not a list of its own signals. There is no event, no property and no signal on this interface (AGENTS.md, "Security and privacy by design").
- Its signals are its own (they belong to its D-Bus connection): it cannot take away another program's, and they are taken away when it leaves the bus, when the person switches the choice off, or when the live lighting stops.
- They are drawn over the person's lighting at the brightness in force: dark during "lights off until morning". They are also drawn over the glow of the keys under the fingers (`docs/KEY_GLOW.md`), so what a program asked to show stays visible; that glow is a separate choice, in another file, and a program can neither switch it on nor learn anything about it.
- Limits: 8 programs, 16 signals per program, 64 in all, 20 requests per second together. A refused request returns a sentence saying why.

## Interface

Session bus, service `org.galleoncore.Deck`, object `/org/galleoncore/KeySignals`, interface `org.galleoncore.KeySignals`:

| Method | Arguments | Returns |
| --- | --- | --- |
| `Set` | `key` s, `red` i, `green` i, `blue` i (0 to 255), `effect` s (`steady` or `pulse`), `seconds` d (1 to 86400) | `""` when accepted, else why not |
| `Clear` | `key` s | true if this program had a signal on that key |
| `ClearAll` | | true if this program had any signal |

`key` is the name of the physical key as on a QWERTY keyboard ("F3", "Left Control", "Q" is the key that says A on an AZERTY keyboard). A key whose LED has not been measured yet (`data/devices/galleon-100-sd-keymap.json`) is refused.

**The program has to stay on the bus.** A signal belongs to the connection that asked for it and goes when that connection closes. So a command that makes one call and exits (`busctl`, `gdbus`, `dbus-send`) lights nothing that lasts: its signal ends as soon as the command does. A client for this interface must keep its connection open for as long as the signals should last, and close it (or call `Clear` / `ClearAll`) to take them away. The seconds you give are the longest they can last, not a promise that they stay if the program leaves.

To try a signal by hand there is the command-line tool below, and Bosun has a **"Try a signal" panel** (Keyboard tab, live lighting): click a key on the drawing, choose a colour, steady or pulse, and a time (10 s, 1 min, 10 min, 1 h), then "Light the key". It sends the same request as another program (on Linux through this D-Bus interface, so the signal goes when Bosun is closed; on Windows through the local endpoint as the client `bosun`). It still needs the box "Other programs may light keys" to be ticked and the live lighting to be on, and galleond's refusal, if any, is shown in its own words (on Windows nothing comes back).

## The command-line tool: `galleon-key-signal`

Installed with the other programs (`bin/`), on Linux and Windows. It lights ONE key through the interface above, as another program would. It reads no key press and no state of the keyboard.

```
galleon-key-signal --key F3 --color #ff7800 --effect pulse --seconds 600
galleon-key-signal --key F3 --red 255 --green 120 --blue 0 --seconds 60
```

| Option | Meaning |
| --- | --- |
| `--key NAME` | The key by its place on a QWERTY keyboard (`F3`, `Left Control`; `W` is the key engraved Z on an AZERTY keyboard). A key whose light was not measured is refused. |
| `--color #rrggbb` | Its colour. Or `--red`, `--green` and `--blue`, each 0 to 255, all three. Not both forms. |
| `--effect steady\|pulse` | Steady (default) or breathing (about 1.6 s a cycle). |
| `--seconds N` | How long, 1 to 86400. **Required**: a signal always has an end. |
| `--help` | The usage. |
| `--clear`, `--clear-all`, `--client NAME` | **Windows only** (below). |

**On Linux** the tool stays running while the key is lit: the signal belongs to the D-Bus connection of the tool. It takes the signal away when its time is over or when it is stopped (Ctrl-C, `kill`). So there is no `--clear`: stop the tool. Exit status: 0 when it ended as asked, 1 for a wrong command line, 2 when galleond refused (the sentence is printed: the box "Other programs may light keys" is not ticked, the live lighting is off, a limit) or could not be reached.

**On Windows** the tool sends the request to the local endpoint and ends at once; the signal lasts until its end or a clear. `--clear --key F3` takes away the signal of that key, `--clear-all` every signal, both for the same `--client` (default `galleon-key-signal`: the name the signals belong to, 1 to 64 plain characters). The endpoint gives no reply about the lighting: the tool says "sent" when the request was taken, and a refusal shows nothing.

## Windows (local endpoint)

Windows has no D-Bus: galleond's per-user local endpoint (a named pipe, `\\.\pipe\galleoncore-<24 hex digits>`, the digits being the first 24 of the UTF-8 SHA-256 of the user's SID, as for the other actions) takes the same three requests, one JSON object per line, one request per connection:

```json
{"action":"key-signal","client":"assistant","key":"F3","red":255,"green":120,"blue":0,"effect":"pulse","seconds":600}
{"action":"key-signal-clear","client":"assistant","key":"F3"}
{"action":"key-signal-clear-all","client":"assistant"}
```

`tools/windows/key-signal.ps1` sends them (not tested on a Windows machine yet: `docs/WINDOWS_ACCEPTANCE.md`, section 8). Differences from Linux, by the nature of this endpoint:

- **No reply about the result.** The endpoint answers `{"ok":true}` for a request that is well formed, whether or not a key was lit (not allowed, lighting off, unknown key, too many requests). Nothing comes back about the keyboard.
- **No way to tell programs apart.** A signal belongs to the `client` name in the request (1 to 64 plain characters), so another program of the same person that knows the name can clear it. Every signal still ends by itself, the person's choice in Bosun comes first, and it stops when the live lighting stops.
- **No cleanup when a program quits**: only the end of the signal's duration, or a clear, removes it. Choose short durations and renew them.
