// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

.pragma library

// The folders the shared widgets (widgets/ of the project) are looked for in, in this order: those of `extra` (a list, may be empty:
// the tests, a source tree), then the installed ones: a package puts them in /usr/share/galleoncore/widgets, a local install in
// /usr/local/share/galleoncore/widgets.
function candidates(extra) {
    const folders = [];
    for (const folder of extra) {
        if (folder !== "")
            folders.push(folder);
    }
    folders.push("/usr/share/galleoncore/widgets");
    folders.push("/usr/local/share/galleoncore/widgets");
    return folders;
}

// The address of the widget `name` in `folder`.
function widgetUrl(folder, name) {
    return "file://" + folder + "/" + name;
}

// The screen of the module is 720 x 384 (15:8). The widget is drawn for those pixels, so it is shown at that size and scaled, never
// stretched: `fit` gives the largest 15:8 rectangle that fits in an area of `width` x `height`, and the scale to give the widget.
const ScreenWidth = 720;
const ScreenHeight = 384;

function fit(width, height) {
    if (width <= 0 || height <= 0)
        return {width: 0, height: 0, scale: 0};
    const scale = Math.min(width / ScreenWidth, height / ScreenHeight);
    return {width: ScreenWidth * scale, height: ScreenHeight * scale, scale: scale};
}
