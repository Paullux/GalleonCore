<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# The keys glow under your fingers (Linux)

When this is on, the key you just pressed lights up in cyan and fades out in a second and a half, over whatever the keyboard shows. It is the first
feature of GalleonCore that has to read the key presses of the keyboard, so it is built under the rules of AGENTS.md ("Security and privacy by design").
Everything below is a promise the code keeps and a test checks.

## Turning it on

**Off by default.** In Bosun (Keyboard tab, live lighting) the line "Keys glow under your fingers" shows the state, always, and one click changes it. Once it
is on, "Colour of the keys you press" offers the same colours as the keyboard (a few swatches, or `#rrggbb`), cyan until you choose another. Bosun writes
`~/.config/galleoncore/key-glow.json` (`{"version":1,"allow":true,"colour":"#00c8ff"}`); galleond only reads it, once a second, so unticking the box stops
everything within the second and a new colour shows within the second. The file is read as untrusted like any other: exactly these three keys, a true or false,
a colour of the form `#rrggbb`, a few hundred bytes at most; anything else means off and cyan, and nothing is taken from a part of a damaged file. Switching the
glow off keeps the colour for next time, and choosing a colour never switches the glow on. No program on the bus can change it, and nothing here switches the live lighting on: the glow only runs while the live lighting is on.
To try it without the window, write that file by hand. A missing, oversized or damaged file means off.

Linux only for now. On Windows Bosun shows nothing about it, because galleond has no source of key presses there yet (Raw Input is planned); the choice is
never offered where it would do nothing.

## What galleond reads, and what happens to it

- It reads the hidraw node of the keyboard's key interface (`1b1c:2b0c`, the one whose descriptor is a keyboard), **read-only** and **without taking it away
  from the system**: the system keeps receiving every key. `tools/udev/70-galleon.rules` already lets your session open it, so there is no new permission and no
  member of the `input` group is needed. The layout of the reports is the one the keyboard's own descriptor declares (`docs/protocol/README.md`, "Keyboard
  input reports"); the keyboard was measured sending the report of ID 1 only.
- It reads **only while the glow is on and the live lighting is on.** Between the two, nothing is opened or read.
- Each report is turned at once into "this LED went down" (the keys whose light was measured), and the bytes are cleared. What is kept is the LED and the
  time of the last touch of at most 64 keys, in memory, for the length of the fade (`ReactiveGlow.h`): **no key, no order, no count, no history, no file and
  no log.** A key held while another is pressed is said again with each report, and glows again: remembering what was down before would be a record of the keys.
- It is **internal**: not on D-Bus, not on the local endpoint, no method asks which key was pressed or whether the glow is on (a test lists the names of the
  lighting controller's methods and signals and finds none about it). Programs that light keys (`docs/KEY_SIGNALS.md`) never learn about it, and their signals
  are drawn over the glow, so what they asked to show stays visible.
- **The lighting trace is paused while it runs.** `GALLEON_LIGHTING_LOG=1` writes a line for each report sent to the keyboard, and a frame that lights a key at
  a given moment would reveal that it was pressed. While the glow is watching, the trace writes nothing (it says so in one line before, and again after, a
  couple of seconds past the end, once the last glowing frame is gone). The line of the log that says how many LEDs are lit is cut too, for the same reason.
- The glow is drawn at the brightness in force: at night ("lights off until morning") it is black like everything else, and it never lights a key that the lighting does not.

## What it does not do

The fade is fixed (1.5 s) in this first version; the colour is yours. It is not on Windows yet. It does not follow the neighbouring keys. It reads nothing from the
other keyboards of the computer (only the node of this one).

## For whoever reviews a change here

The review that AGENTS.md asks for after reactive effects (logs and LED frames) was done for this version: the only log lines that could have followed the typing
were the trace of the lighting and the count of lit LEDs, and both are off while the glow runs. Any change to event capture, permissions, logs or network
communication must keep that, with a test, and name its review in the pull request.
