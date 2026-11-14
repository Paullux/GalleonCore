# Using GalleonCore with OpenRGB

Two programs that write the lighting of the same device at the same time do not get along. GalleonCore's live lighting of the
keyboard (experimental, off by default) and the rings of the dials can both be taken over by OpenRGB. This page says what was
seen, how to stop OpenRGB from taking the Galleon 100 SD, and what is not known.

## What was seen

Windows 11, one Galleon 100 SD (keyboard `1b1c:2b0c`, module `1b1c:2b18`), tests of the owner on 14 November 2026:

- **OpenRGB started after Bosun takes the rings of the dials.** Official OpenRGB lists them as a "Dynamic Light" device (Corsair
  LampArray, `1b1c:2b18`, interface `MI_03`).
- **With OpenRGB running, the live lighting of the keyboard showed nothing**, although `galleond` said that it held the keyboard,
  every request was answered with status 0 and the keyboard stayed in mode 2 (the trace of the lighting, see below). With
  OpenRGB closed, the colour showed after the 3 seconds that `galleond` listens for another program before it takes the keys.
- Windows Dynamic Lighting was switched off, and Windows listed no compatible device: it was not the cause.

**Not established:** by which path OpenRGB keeps the keys from showing the colour (it produced no answer that `galleond` could
see, so `galleond` could not tell that it was there), and whether OpenRGB writes continuously or only when it starts.

## Stopping OpenRGB from taking the Galleon 100 SD

In OpenRGB: **Settings** (Paramètres), **Supported Devices** (Périphériques supportés), type `Lamp` in the filter, switch off
**`HID LampArray Device`**, then **Apply the changes** (Appliquer les changements) and scan the devices again or restart OpenRGB.

- This is the detector of the HID *LampArray* devices: the rings of the dials are one (`MI_03` of the module), and the keyboard
  exposes its own LampArray interface too (`MI_04`, usage page `0x59`, see [protocol/README.md](protocol/README.md)).
  Switching it off stops OpenRGB from taking **every** device that it finds that way, not only the Galleon's.
- The owner switched it off and then tested. **What this does for the keyboard's keys has not been checked yet.**
- If you do not need OpenRGB, closing it is the safest way.

## The trace of the live lighting

`galleond` writes a text file of the exchanges with the keyboard when `GALLEON_LIGHTING_LOG=1` is in its environment, in
`%LOCALAPPDATA%\GalleonCore\logs\lighting-<date>-<time>-<pid>.log` (`~/.local/share/GalleonCore/logs/` on Linux): the time, the
direction, the length and the first 16 bytes of each report, and the changes of state. No serial number, no path, no setting,
256 KiB at most. It is for finding out why the lighting does not show on a given system; nothing more is sent because of it.
