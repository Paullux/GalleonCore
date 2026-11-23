<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Lighting single keys from another program (Linux, D-Bus)

galleond can light single keys for other programs, so that an assistant, a calendar or a script can say "mail" or "meeting" on a key without ever touching the keyboard. galleond stays the only program that writes to it.

**Off by default.** The person allows it in Bosun (Keyboard, live lighting: "Other programs may light keys"). Nothing here works while the live lighting is off, and nothing here switches it on.

## What a program can and cannot do

- It **sends** a colour for one key, for a time it chooses (1 second to 24 hours, mandatory). That is all.
- It **never receives** anything about the keyboard: not which keys are pressed, not what is lit, not a list of its own signals. There is no event, no property and no signal on this interface (AGENTS.md, "Security and privacy by design").
- Its signals are its own (they belong to its D-Bus connection): it cannot take away another program's, and they are taken away when it leaves the bus, when the person switches the choice off, or when the live lighting stops.
- They are drawn over the person's lighting at the brightness in force: dark during "lights off until morning".
- Limits: 8 programs, 16 signals per program, 64 in all, 20 requests per second together. A refused request returns a sentence saying why.

## Interface

Session bus, service `org.galleoncore.Deck`, object `/org/galleoncore/KeySignals`, interface `org.galleoncore.KeySignals`:

| Method | Arguments | Returns |
| --- | --- | --- |
| `Set` | `key` s, `red` i, `green` i, `blue` i (0 to 255), `effect` s (`steady` or `pulse`), `seconds` d (1 to 86400) | `""` when accepted, else why not |
| `Clear` | `key` s | true if this program had a signal on that key |
| `ClearAll` | | true if this program had any signal |

`key` is the name of the physical key as on a QWERTY keyboard ("F3", "Left Control", "Q" is the key that says A on an AZERTY keyboard). A key whose LED has not been measured yet (`data/devices/galleon-100-sd-keymap.json`) is refused.

```bash
busctl --user call org.galleoncore.Deck /org/galleoncore/KeySignals org.galleoncore.KeySignals Set siiissd F3 255 120 0 pulse 600
```

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
