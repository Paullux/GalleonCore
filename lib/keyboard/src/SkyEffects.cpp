// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/SkyEffects.h"

#include "galleon/keyboard/SkyPalette.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace galleon::keyboard {

namespace {

// The numbers of the mockup.
constexpr double ShimmerPeriodScale = 2200.0;   // sin(now / 2200 + i * 0.5 + r), with `now` in milliseconds
constexpr double ShimmerPerKey = 0.5;           // the phase that one more key in the row adds
constexpr double ShimmerBrightening = 1.2;      // the key goes towards its colour times 1.2
constexpr Rgb StarColour{235, 238, 255};
constexpr Rgb TwinkleColour{255, 255, 255};
constexpr double TwinkleStrength = 0.85;
constexpr double StarSpeedLow = 0.25;           // keys a second
constexpr double StarSpeedSpread = 0.25;        // up to 0.5 keys a second
constexpr double StarBrightnessLow = 0.5;
constexpr double StarBrightnessSpread = 0.3;    // up to 0.8
constexpr int StarRows = 5;                     // the stars are in rows 1 to 5 (not the top row, the function row)
constexpr double KeysPerRow = 14.0;             // a star crosses about a row of keys at that speed: the lap is the width over (speed / 14) per second
constexpr double TwinkleChancePerSecond = 0.0012 * 60.0; // the mockup draws at about 60 pictures a second, with a chance of 0.0012 for each key
constexpr double TwinkleFadePerStep = 0.45;     // the mockup fades by 0.06 a picture, about 7.5 pictures in a step of 1/8 second

[[nodiscard]] std::uint8_t mixChannel(std::uint8_t from, std::uint8_t to, double fraction)
{
    return static_cast<std::uint8_t>(std::lround(from + (to - from) * std::clamp(fraction, 0.0, 1.0)));
}

[[nodiscard]] Rgb mixColour(const Rgb &from, const Rgb &to, double fraction)
{
    return {mixChannel(from.red, to.red, fraction), mixChannel(from.green, to.green, fraction), mixChannel(from.blue, to.blue, fraction)};
}

// A fixed hash of three whole numbers to a number in [0, 1): the "chance" of the mockup, the same every time (splitmix64).
[[nodiscard]] double unitHash(std::uint64_t a, std::uint64_t b, std::uint64_t c)
{
    std::uint64_t x = a * 0x9E3779B97F4A7C15ULL + b * 0xBF58476D1CE4E5B9ULL + c * 0x94D049BB133111EBULL + 0x2545F4914F6CDD1DULL;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return static_cast<double>(x >> 11) / 9007199254740992.0; // 2^53
}

struct RowLed
{
    std::uint16_t led;
    double x;
};

// The LEDs of the keys of each of the six rows, from left to right (the strip is not part of the sky effects).
using Rows = std::array<std::vector<RowLed>, SkyRowCount>;

[[nodiscard]] Rows rowsOf(std::span<const LedPosition> positions)
{
    Rows rows;
    for (const LedPosition &position : positions) {
        const SkyPlace place = skyPlaceOf(position.y);
        if (!place.strip)
            rows.at(static_cast<std::size_t>(place.row)).push_back({position.led, position.x});
    }
    for (auto &row : rows)
        std::ranges::sort(row, [](const RowLed &a, const RowLed &b) { return a.x < b.x; });
    return rows;
}

void shimmer(const Rows &rows, std::span<Rgb, LiveLedCount> colours, double seconds, double nightness)
{
    const double daylight = 1.0 - nightness;
    if (daylight <= 0.0)
        return;
    const double milliseconds = seconds * 1000.0;
    for (std::size_t row = 0; row < rows.size(); ++row) {
        for (std::size_t i = 0; i < rows.at(row).size(); ++i) {
            const double wave = (std::sin(milliseconds / ShimmerPeriodScale + static_cast<double>(i) * ShimmerPerKey + static_cast<double>(row)) + 1.0) / 4.0;
            Rgb &colour = colours[rows.at(row).at(i).led];
            const auto brighter = [](std::uint8_t c) { return static_cast<std::uint8_t>(std::min(255.0, std::round(c * ShimmerBrightening))); };
            colour = mixColour(colour, {brighter(colour.red), brighter(colour.green), brighter(colour.blue)}, wave * daylight);
        }
    }
}

// The light that the stars give to each LED, 0 to 1.
[[nodiscard]] std::array<double, LiveLedCount> starLight(const Rows &rows, double seconds)
{
    std::array<double, LiveLedCount> light{};
    for (std::uint64_t star = 0; star < static_cast<std::uint64_t>(SkyStarCount); ++star) {
        const double speed = StarSpeedLow + unitHash(star, 1, 0) * StarSpeedSpread; // keys a second
        const double brightness = StarBrightnessLow + unitHash(star, 2, 0) * StarBrightnessSpread;
        const double start = unitHash(star, 3, 0);                                   // where in the first walk it begins, 0 to 1
        const double travelled = start + seconds * speed / KeysPerRow;              // in walks
        const double lap = std::floor(travelled);
        const double along = travelled - lap;                                        // 0 to 1 along the row in this walk
        const std::size_t row = 1 + static_cast<std::size_t>(unitHash(star, 4, static_cast<std::uint64_t>(std::max(0.0, lap))) * StarRows) % StarRows;
        const auto &leds = rows.at(row);
        if (leds.size() < 2)
            continue;
        // The walk goes from the first key of the row to the last one, so no stretch of it is spent where there is no key. It fades in at the beginning and
        // out at the end, over a time that is the same in seconds for every star.
        const double fadeLength = std::min(0.5, SkyStarFadeSeconds * speed / KeysPerRow);
        const double envelope = std::min({1.0, along / fadeLength, (1.0 - along) / fadeLength});
        const double x = leds.front().x + along * (leds.back().x - leds.front().x);
        for (std::size_t i = 0; i + 1 < leds.size(); ++i) {
            const RowLed &a = leds.at(i);
            const RowLed &b = leds.at(i + 1);
            if (x < a.x || x > b.x)
                continue;
            const double f = b.x > a.x ? (x - a.x) / (b.x - a.x) : 0.0; // however far apart the two keys are: the star crosses the gap
            light.at(a.led) = std::max(light.at(a.led), (1.0 - f) * brightness * envelope);
            light.at(b.led) = std::max(light.at(b.led), f * brightness * envelope);
            break;
        }
    }
    return light;
}

void stars(const Rows &rows, std::span<Rgb, LiveLedCount> colours, double seconds, double nightness)
{
    if (nightness <= 0.0)
        return;
    const auto light = starLight(rows, seconds);
    const auto slot = static_cast<std::int64_t>(std::floor(seconds / SkyEffectStep));
    const double chance = TwinkleChancePerSecond * SkyEffectStep;
    for (std::size_t row = 1; row < rows.size(); ++row) { // not the top row, as in the mockup
        for (const RowLed &key : rows.at(row)) {
            Rgb &colour = colours[key.led];
            colour = mixColour(colour, StarColour, light.at(key.led) * nightness);
            double twinkle = 0.0;
            for (std::int64_t back = 0; back < 3; ++back) { // a twinkle fades over about three steps
                if (slot - back < 0)
                    break;
                if (unitHash(key.led, 5, static_cast<std::uint64_t>(slot - back)) < chance)
                    twinkle = std::max(twinkle, 1.0 - TwinkleFadePerStep * static_cast<double>(back));
            }
            if (twinkle > 0.0)
                colour = mixColour(colour, TwinkleColour, twinkle * TwinkleStrength * nightness);
        }
    }
}

} // namespace

std::array<double, LiveLedCount> starLights(std::span<const LedPosition> positions, double seconds)
{
    if (positions.empty())
        return {};
    return starLight(rowsOf(positions), seconds);
}

void animateSky(std::span<const LedPosition> positions, std::span<Rgb, LiveLedCount> colours, double seconds, double nightness)
{
    if (positions.empty())
        return;
    const double stepped = std::floor(seconds / SkyEffectStep) * SkyEffectStep;
    const double night = std::clamp(nightness, 0.0, 1.0);
    const Rows rows = rowsOf(positions);
    shimmer(rows, colours, stepped, night);
    stars(rows, colours, stepped, night);
}

} // namespace galleon::keyboard
