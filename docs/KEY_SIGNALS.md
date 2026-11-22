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

Windows has no way in for other programs yet (the local IPC of galleond will get the same three actions).
