// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/SkyPalette.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace galleon::keyboard {

namespace {

using Palette = std::array<Rgb, SkyRowCount>;

// The four palettes of the mockup of Aaryush1 (lab/mockups/sky-palette.html), row 0 first.
constexpr Palette Night{{{10, 14, 38}, {12, 16, 44}, {12, 16, 44}, {14, 18, 50}, {16, 22, 56}, {16, 22, 56}}};
constexpr Palette Dawn{{{34, 30, 92}, {70, 50, 130}, {150, 70, 120}, {230, 110, 70}, {255, 150, 70}, {255, 180, 90}}};
constexpr Palette Day{{{26, 70, 170}, {30, 80, 185}, {36, 95, 200}, {44, 110, 215}, {52, 125, 225}, {60, 135, 230}}};
constexpr Palette Dusk{{{30, 22, 90}, {60, 30, 120}, {130, 40, 130}, {210, 60, 90}, {255, 95, 45}, {255, 120, 40}}};

struct Keyframe
{
    double hours;
    const Palette *palette;
};

// The keyframes of the mockup (`KF`): night until 5 h, dawn at 6 h 15, day from 7 h 30 to 16 h 30, dusk at 18 h, night again from 19 h 15.
constexpr std::array<Keyframe, 8> Keyframes{{{0.0, &Night}, {5.0, &Night}, {6.25, &Dawn}, {7.5, &Day}, {16.5, &Day}, {18.0, &Dusk}, {19.25, &Night}, {24.0, &Night}}};

// The row pitch and the height of the top row, from data/devices/galleon-100-sd-led-positions.json (rows at y = 43, 63, 84, 104, 125 and 146 mm).
constexpr double TopRowY = 43.0;
constexpr double RowPitch = 20.5;
constexpr double StripBelowY = 30.0; // the strip is at y = 16.8 mm: anything above this line is the strip
constexpr double StripFade = 0.4; // the strip is the top row mixed 40 % of the way to black (`mix(rows[0], [0,0,0], 0.4)` in the mockup)

[[nodiscard]] double wrapped(double hours)
{
    double h = std::fmod(hours, 24.0);
    if (h < 0.0)
        h += 24.0;
    return h;
}

[[nodiscard]] std::uint8_t mixChannel(std::uint8_t from, std::uint8_t to, double fraction)
{
    return static_cast<std::uint8_t>(std::lround(from + (to - from) * fraction));
}

} // namespace

std::array<Rgb, SkyRowCount> skyRowColours(double hours)
{
    const double h = wrapped(hours);
    for (std::size_t i = 0; i + 1 < Keyframes.size(); ++i) {
        const Keyframe &from = Keyframes.at(i);
        const Keyframe &to = Keyframes.at(i + 1);
        if (h < from.hours || h > to.hours)
            continue;
        const double span = to.hours - from.hours;
        const double fraction = span > 0.0 ? (h - from.hours) / span : 0.0;
        std::array<Rgb, SkyRowCount> rows{};
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const Rgb &a = from.palette->at(r);
            const Rgb &b = to.palette->at(r);
            rows.at(r) = {mixChannel(a.red, b.red, fraction), mixChannel(a.green, b.green, fraction), mixChannel(a.blue, b.blue, fraction)};
        }
        return rows;
    }
    return Night; // not reached: the keyframes cover 0 to 24
}

double skyNightness(double hours)
{
    const double h = wrapped(hours);
    if (h >= 19.25 || h < 5.0)
        return 1.0;
    if (h >= 18.0)
        return (h - 18.0) / 1.25;
    if (h >= 5.0 && h < 6.25)
        return 1.0 - (h - 5.0) / 1.25;
    return 0.0;
}

SkyPlace skyPlaceOf(double y)
{
    if (y < StripBelowY)
        return {true, 0};
    const double line = std::round((y - TopRowY) / RowPitch);
    return {false, std::clamp(static_cast<int>(line), 0, SkyRowCount - 1)};
}

Rgb skyStripColour(const Rgb &topRow)
{
    return {mixChannel(topRow.red, 0, StripFade), mixChannel(topRow.green, 0, StripFade), mixChannel(topRow.blue, 0, StripFade)};
}

} // namespace galleon::keyboard
