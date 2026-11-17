// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/MatrixRain.h"

#include "galleon/keyboard/SkyPalette.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace galleon::keyboard {

namespace {

// The colours of widgets/MatrixRain.qml: the head "#d8ffe0", the letters "#00d43a" fading to black above the head.
constexpr Rgb HeadColour{216, 255, 224};
constexpr Rgb TrailColour{0, 212, 58};

constexpr double ColumnPitchMm = 20.5;  // the keys of a row are about this far apart
constexpr int TrailRows = 4;            // the trail above the head, in rows (the keyboard has few rows: the screen's nine would fill it)
constexpr int RestRows = 3;             // the rows of waiting before a column begins again
constexpr double SlowestRowsPerSecond = 1.8;
constexpr double FastestRowsPerSecond = 4.0;

// A fixed hash of two whole numbers, as in the QML (the same mixing of the bits).
[[nodiscard]] std::uint32_t hash(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = (a + 1U) * 73856093U ^ (b + 1U) * 19349663U ^ 83492791U;
    h = (h ^ (h >> 15)) * 2246822519U;
    return h ^ (h >> 13);
}

[[nodiscard]] std::uint8_t scaled(std::uint8_t channel, double factor)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(channel * factor, 0.0, 255.0)));
}

} // namespace

std::array<Rgb, LiveLedCount> matrixRainColours(std::span<const LedPosition> positions, double seconds)
{
    std::array<Rgb, LiveLedCount> colours{};
    if (positions.empty() || seconds < 0.0)
        return colours;
    double left = positions.front().x;
    for (const LedPosition &position : positions)
        left = std::min(left, position.x);
    // Lines: the strip is line 0, the six rows of keys are lines 1 to 6.
    constexpr int Lines = SkyRowCount + 1;
    constexpr int Cycle = Lines + TrailRows + RestRows;
    const auto tick = static_cast<std::int64_t>(std::floor(seconds / MatrixTickSeconds));
    for (const LedPosition &position : positions) {
        if (position.led >= LiveLedCount)
            continue;
        const SkyPlace place = skyPlaceOf(position.y);
        const int line = place.strip ? 0 : 1 + place.row;
        const auto column = static_cast<std::uint32_t>(std::max(0.0, std::round((position.x - left) / ColumnPitchMm)));
        // Each column falls at its own speed (in lines a second) and starts at its own place in the cycle.
        const double speed = SlowestRowsPerSecond + (hash(column, 1000) % 100) / 100.0 * (FastestRowsPerSecond - SlowestRowsPerSecond);
        const int offset = static_cast<int>(hash(column, 2000) % Cycle);
        const double travelled = static_cast<double>(tick) * MatrixTickSeconds * speed + offset;
        const int head = static_cast<int>(std::floor(travelled)) % Cycle - TrailRows; // may lie above the first line or below the last
        const int behind = head - line;                                               // 0 on the head, 1 on the line above it, ...
        if (behind < 0 || behind >= TrailRows + 1)
            continue; // below the head, or past the end of the trail: dark
        if (behind == 0) {
            colours.at(position.led) = HeadColour;
        } else {
            const double fade = 1.0 - static_cast<double>(behind) / (TrailRows + 1); // from the head to black, as the gradient of the screen
            colours.at(position.led) = {scaled(TrailColour.red, fade), scaled(TrailColour.green, fade), scaled(TrailColour.blue, fade)};
        }
    }
    return colours;
}

} // namespace galleon::keyboard
