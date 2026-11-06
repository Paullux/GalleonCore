# Corsair Web Hub on Linux: what works, what does not

Observed on 2026-11-06 with the Galleon 100 SD (keyboard `1b1c:2b0c`, firmware 2.3.88), Web Hub v1.2.42, Kubuntu 26.04 (KDE Plasma 6, Wayland), Google Chrome. Nothing here is a protocol fact: it is about which browsers can reach the keyboard. For the traffic itself see [webhub-lighting-analysis.md](webhub-lighting-analysis.md) and [README.md](README.md).

## What works

- **A Chromium-family browser** (Chrome, Chromium, Edge, Brave) on a package install (not Flatpak or Snap), with read and write access to the keyboard's `hidraw` nodes (a udev rule). Web Hub shows "GALLEON 100 SD", its profile, lighting effects, key assignments and the Game Mode settings. The browser's own chooser lists "CORSAIR GALLEON 100 SD Mechanical Gaming Keyboard" when "Add a device" is pressed.
- Running it as an application window (`chromium --app=https://www.corsair.com/web-hub`) works the same: no address bar, same WebHID.

## What does not work

- **Qt WebEngine (Qt 6.10.2).** `navigator.hid` exists, but "Add a device" does nothing: there is no device chooser behind it, and none can be added without Chromium's private browser-side API. A page that needs WebHID cannot be embedded this way.
- **Tauri on Linux** uses WebKitGTK, which has no WebHID at all (not tested here: this follows from WebKit not implementing the API).
- **Firefox and Safari** do not implement WebHID.

## Why it matters for GalleonCore

- Web Hub is the only way to configure the keyboard's own lighting and keys on Linux today. GalleonCore only links to it and never writes to the keyboard's memory.
- What Web Hub saves goes to the keyboard's onboard storage (see the warnings in `webhub-lighting-analysis.md`). Never replay those writes.
- Keeping Bosun free of any HID access for the keyboard (see `AGENTS.md`) means the browser, not Bosun, must be the one that opens it.

## Embedding a browser window (experiment, not published)

A window of the real Chromium can be taken into another X11 window, which makes Web Hub appear inside an application. It needs X11 (Xwayland is enough), a separate browser profile folder so that a browser already running on Wayland does not answer in its place, and it is fragile: the browser moves its window back to its own position after each move, so the placement has to be absorbed by an intermediate window. It was tried on a local branch only. Wayland offers no way to do the same.
