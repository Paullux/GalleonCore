// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/Appearance.h"
#include "galleon/keyboard/SunTimes.h"

#include <array>
#include <cstdint>

// The colours of a sky that follows the hour of the day, drawn on the six rows of the keyboard (night at the bottom of the day, dawn, day, dusk).
// Pure: the hour is an argument, nothing is read or sent here.
//
// The palette, its keyframes and the way the colours are blended come from the interactive mockup of Aaryush1
// (https://github.com/Aaryush1/GalleonCore/blob/key-lighting/lab/mockups/sky-palette.html, issue #111): six colours for each of night, dawn, day
// and dusk, a keyframe table over 24 hours, a linear blend of each channel between two keyframes. The shimmer and the stars of the mockup are not
// part of this first version. The colours are those of a screen: the real LEDs look brighter and more saturated, and none of it is tuned on the
// keyboard yet.
namespace galleon::keyboard {

inline constexpr int SkyRowCount = 6;

// The sky of a day of the sun: its real sunrise and sunset when the sun rises and sets, the schedule of the mockup when it does not (a polar day or
// night) .
[[nodiscard]] SkyDay skyDayFrom(const SunDay &sun);

// The keyframes are anchored on the sunrise and on the sunset with the gaps of the mockup: night until 1 h 15 before the sunrise, dawn at the
// sunrise, day 1 h 15 after it, day until 1 h 30 before the sunset, dusk at the sunset, night 1 h 15 after it. With the default `SkyDay` that is
// exactly the table of the mockup. When the day is too short or the night too short for the gaps (high latitudes), the gaps shrink in proportion so
// that the keyframes stay in order.
//
// One colour for each of the six rows (row 0 is the top row of the keys, the function row), at `hours` on a 24 h clock (any real number: it wraps).
[[nodiscard]] std::array<Rgb, SkyRowCount> skyRowColours(double hours, const SkyDay &day = {});

// 1 in the night, 0 in the day, between at dusk and at dawn (the mockup's `nightness`): how much the stars would show.
[[nodiscard]] double skyNightness(double hours, const SkyDay &day = {});

// Where a LED is in the sky: a row of the keys (0 to 5) or the light strip above them.
struct SkyPlace
{
    bool strip = false; // the light strip: it takes the colour of the top row, dimmed (skyStripColour)
    int row = 0;        // 0 to 5, meaningful for the keys
};

// From the position of a LED (millimetres, y downwards as in data/devices/galleon-100-sd-led-positions.json): the strip is above the top row; the
// rows are the lines of keys, 20.5 mm apart from the top row (y = 43 mm). A LED between two lines (the lower part of the ISO Enter) goes to the
// upper one.
[[nodiscard]] SkyPlace skyPlaceOf(double y);

// The colour of the light strip: the colour of the top row mixed 40 % of the way to black, so 60 % of it (`mix(rows[0], [0,0,0], 0.4)` in the mockup).
[[nodiscard]] Rgb skyStripColour(const Rgb &topRow);

} // namespace galleon::keyboard
