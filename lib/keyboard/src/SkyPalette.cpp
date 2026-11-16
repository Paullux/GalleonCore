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

// The gaps of the mockup (its keyframes are at 5 h, 6 h 15, 7 h 30, 16 h 30, 18 h and 19 h 15): hours before and after the sunrise and the sunset.
constexpr double NightBeforeDawn = 1.25; // the night ends this long before the sunrise
constexpr double DayAfterDawn = 1.25;    // the day is there this long after the sunrise
constexpr double DayBeforeDusk = 1.5;    // the day lasts until this long before the sunset
constexpr double NightAfterDusk = 1.25;  // the night is there this long after the sunset

struct Keyframe
{
    double hours;
    const Palette *palette;
    double night; // 1 for a night keyframe, 0 for the others: the stars of the mockup show at night
};

// The keyframes of one day, from the end of one night to the end of the next, in order, on a line of hours that is not wrapped (the first can be
// negative, the last past 24).
[[nodiscard]] std::array<Keyframe, 7> keyframesOf(const SkyDay &day)
{
    const double sunrise = std::clamp(day.sunriseHours, 0.0, 24.0);
    const double length = std::clamp(day.dayLengthHours, 0.0, 24.0);
    const double gap = 24.0 - length; // the night, from the sunset to the next sunrise
    const double nightSide = std::min(1.0, gap / (2.0 * std::max(NightBeforeDawn, NightAfterDusk)));
    const double nightBefore = NightBeforeDawn * nightSide;
    const double nightAfter = NightAfterDusk * nightSide;
    const double daySide = std::min(1.0, length / (DayAfterDawn + DayBeforeDusk));
    const double dayStart = sunrise + DayAfterDawn * daySide;
    const double dayEnd = sunrise + length - DayBeforeDusk * daySide;
    return {{{sunrise - nightBefore, &Night, 1.0},
             {sunrise, &Dawn, 0.0},
             {dayStart, &Day, 0.0},
             {dayEnd, &Day, 0.0},
             {sunrise + length, &Dusk, 0.0},
             {sunrise + length + nightAfter, &Night, 1.0},
             {sunrise + 24.0 - nightBefore, &Night, 1.0}}};
}

// Where `hours` is in the line of the keyframes: the one before it and the share of the way to the next, after the hour was brought into
// [first keyframe, first keyframe + 24).
struct Segment
{
    std::size_t from;
    double fraction;
};

[[nodiscard]] Segment segmentAt(const std::array<Keyframe, 7> &frames, double hours)
{
    const double first = frames.front().hours;
    double h = std::fmod(hours - first, 24.0);
    if (h < 0.0)
        h += 24.0;
    h += first;
    for (std::size_t i = 0; i + 1 < frames.size(); ++i) {
        const double a = frames.at(i).hours;
        const double b = frames.at(i + 1).hours;
        if (h >= a && h <= b)
            return {i, b > a ? (h - a) / (b - a) : 0.0};
    }
    return {frames.size() - 2, 1.0};
}

// The row pitch and the height of the top row, from data/devices/galleon-100-sd-led-positions.json (rows at y = 43, 63, 84, 104, 125 and 146 mm).
constexpr double TopRowY = 43.0;
constexpr double RowPitch = 20.5;
constexpr double StripBelowY = 30.0; // the strip is at y = 16.8 mm: anything above this line is the strip
constexpr double StripFade = 0.4; // the strip is the top row mixed 40 % of the way to black (`mix(rows[0], [0,0,0], 0.4)` in the mockup)

[[nodiscard]] std::uint8_t mixChannel(std::uint8_t from, std::uint8_t to, double fraction)
{
    return static_cast<std::uint8_t>(std::lround(from + (to - from) * fraction));
}

} // namespace

SkyDay skyDayFrom(const SunDay &sun)
{
    if (sun.kind != SunDay::Kind::Normal)
        return {}; // a polar day or night: the schedule of the mockup
    return {sun.sunriseHours, sun.dayLengthHours};
}

std::array<Rgb, SkyRowCount> skyRowColours(double hours, const SkyDay &day)
{
    const auto frames = keyframesOf(day);
    const Segment at = segmentAt(frames, hours);
    const Keyframe &from = frames.at(at.from);
    const Keyframe &to = frames.at(at.from + 1);
    std::array<Rgb, SkyRowCount> rows{};
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const Rgb &a = from.palette->at(r);
        const Rgb &b = to.palette->at(r);
        rows.at(r) = {mixChannel(a.red, b.red, at.fraction), mixChannel(a.green, b.green, at.fraction), mixChannel(a.blue, b.blue, at.fraction)};
    }
    return rows;
}

double skyNightness(double hours, const SkyDay &day)
{
    const auto frames = keyframesOf(day);
    const Segment at = segmentAt(frames, hours);
    const double from = frames.at(at.from).night;
    const double to = frames.at(at.from + 1).night;
    return from + (to - from) * at.fraction;
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
