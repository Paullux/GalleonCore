<!--
SPDX-FileCopyrightText: 2026 Paul Woisard
SPDX-License-Identifier: GPL-3.0-or-later
-->

# OpenRGB plugin (kept for documentation)

**This code is kept as documentation. It is not the way GalleonCore drives
the lighting today.** The dial rings (and, step by step, the keyboard LEDs)
are now driven directly by `galleond`, with GalleonCore's own code. See the
decision of 12 November 2026 in [`AGENTS.md`](../AGENTS.md) and
[`ROADMAP.md`](../ROADMAP.md).

What stays true of this folder:

- It is a separate, optional integration. It is **not part of the main
  build** (see the comment at the top of `CMakeLists.txt`), **not shipped in
  the DEB, RPM or Arch packages**. It is neither built nor tested by the CI or
  by `tools/check.py` (its tests need an OpenRGB source tree); only
  `reuse lint` covers it. It may therefore have drifted from OpenRGB's
  current plugin API.
- It was the first working proof that the two LampArray interfaces of the
  Galleon 100 SD can be read and written on Linux (through libusb, because
  `usbhid` does not bind them). The measurements it relies on are in
  [`docs/protocol/README.md`](../docs/protocol/README.md).
- Writes are off unless OpenRGB is started with `GALLEONCORE_WRITE=1`.
- **Do not run it together with `galleond` on the same rings.** Both write
  the dial rings and nothing arbitrates between them (an open point in
  `ROADMAP.md`, section 3). On Windows, Dynamic Lighting and OpenRGB's
  "Dynamic Light" device compete for the same rings too.

## What each file did, and what does the job in GalleonCore now

The two sides do **not** speak the same protocol, so the table gives the
role played, not an equivalent line by line:

- the plugin writes the standard HID **LampArray** reports (usage page
  `0x59`), with libusb control transfers, on the lamp array interface of each
  USB part;
- `galleond` writes the module's own feature reports through hidapi (the
  dial rings, `03 24 <led> <red> <green> <blue>`) and the keyboard's live
  per-key frame (`12 00` + 146 RGB triplets, Bragi interface). The LampArray
  write path of `galleond` is not implemented.

| Plugin file | What it did | Where `galleond` does this now |
| --- | --- | --- |
| `LampArray.h/.cpp` | Pure parsing and building of LampArray reports: no I/O, every input checked | `lib/src/Protocol.cpp` (`dialRingReports`, `dialRingSegmentReport`: ring reports of the module) and `lib/keyboard/src/LiveFrame.cpp`, `BragiFrame.cpp`, `BragiReply.cpp` (keyboard frame and replies) |
| `UsbLampArray.h/.cpp` | Opens the interface, reads the attributes and lamps, writes the colours, paced | Rings: `lib/src/StreamDeck.cpp` (`setDialRing`, `setDialRingSegment`) over `lib/src/HidTransport.cpp`. Keyboard: `lib/keyboard/src/LiveLink.cpp` with `LivePolicy.cpp` (pacing, freeze rule), `LiveRelease.cpp` and `LiveYield.cpp` (giving the lamps back) |
| `LampArrayController.h/.cpp` | Described the lamps (positions, names, modes) to OpenRGB | `daemon/LedPositions.cpp` (where each keyboard LED is, in mm). The rings have four segments, `DialRingLeds` in `lib/include/galleon/Protocol.h` |
| `GalleonPlugin.h/.cpp` | Loaded by OpenRGB, registered one controller per USB part, took the lighting and gave it back at unload | Rings: `daemon/PagesController.cpp` (`applyRings`: theme colour on the left ring, white on the right), `daemon/RingSettingsApplier.cpp` with `daemon/RingSettings.cpp` (`dial-rings.json`: lit or off, level), `daemon/MatrixRainController.cpp` with `daemon/MatrixRing.cpp` (the rain on the rings). Keyboard: `daemon/LightingController.cpp`, `LightingWorker.h`, `LightingAppearanceApplier.cpp` (one worker thread owns the keyboard link) |
| `tests/test_lamparray.cpp` | Tests of the parsing from the real descriptors | The tests of the files above in `tests/` (Qt Test), run by `ctest` |
| `galleon-plugin.json` | OpenRGB plugin metadata | none (nothing to register: `galleond` is the owner of the device) |

The two USB parts stay separate in `galleond`: the module side (rings,
screen, keys) never calls the keyboard side, and the other way round, even
though both run in the same process.

## Measurements that came from this work

- Ring order and direction on the module, measured with
  `galleond --ring-walk`: same for both dials, counter-clockwise, segment 0 =
  top left, 1 = bottom left, 2 = bottom right, 3 = top right.
- The LampArray attributes of both parts and the 108 keyboard lamps:
  `docs/protocol/README.md`, `docs/protocol/lamps-2b0c.csv`,
  `docs/protocol/lamps-2b18.csv`; probes in `tools/linux/lamparray-probe.c`
  and `tools/linux/lamparray-test-lamp.c`.
