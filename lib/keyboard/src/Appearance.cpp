// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/Appearance.h"

#include "galleon/keyboard/LiveFrame.h"
#include "galleon/keyboard/MatrixRain.h"
#include "galleon/keyboard/SkyEffects.h"
#include "galleon/keyboard/SkyPalette.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace galleon::keyboard {

namespace {

constexpr double RainbowSpread = 0.85; // the hue goes round 85 % of the circle from the left end to the right end, as the preview always did
constexpr double Saturation = 0.9;
constexpr double Lightness = 0.55;

[[nodiscard]] std::uint8_t toByte(double unit)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0, 1.0) * 255.0));
}

// The standard conversion from HSL (all three in 0 to 1) to RGB.
[[nodiscard]] Rgb fromHsl(double hue, double saturation, double lightness)
{
    const double chroma = (1.0 - std::fabs(2.0 * lightness - 1.0)) * saturation;
    const double sector = hue * 6.0;
    const double second = chroma * (1.0 - std::fabs(std::fmod(sector, 2.0) - 1.0));
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    switch (static_cast<int>(sector) % 6) {
    case 0: red = chroma; green = second; break;
    case 1: red = second; green = chroma; break;
    case 2: green = chroma; blue = second; break;
    case 3: green = second; blue = chroma; break;
    case 4: red = second; blue = chroma; break;
    default: red = chroma; blue = second; break;
    }
    const double match = lightness - chroma / 2.0;
    return {toByte(red + match), toByte(green + match), toByte(blue + match)};
}

[[nodiscard]] std::uint8_t scaled(std::uint8_t channel, int level)
{
    return static_cast<std::uint8_t>((static_cast<int>(channel) * level + LevelFull / 2) / LevelFull); // rounded to the nearest
}

// The colour the base gives to one LED.
[[nodiscard]] Rgb baseColour(const Appearance &appearance, const LedPosition &position, double place, double turn, const std::array<Rgb, SkyRowCount> &sky)
{
    switch (appearance.base) {
    case BaseType::Rainbow:
        return rainbowColour(place, turn);
    case BaseType::Sky: {
        const SkyPlace where = skyPlaceOf(position.y);
        return where.strip ? skyStripColour(sky.front()) : sky.at(static_cast<std::size_t>(where.row));
    }
    case BaseType::Uniform:
    case BaseType::Matrix: // the rain is drawn as a whole, below
        break;
    }
    return appearance.colour;
}

} // namespace

Rgb rainbowColour(double place, double turn)
{
    double hue = std::fmod(place * RainbowSpread + turn, 1.0);
    if (hue < 0.0)
        hue += 1.0;
    return fromHsl(hue, Saturation, Lightness);
}

bool isValid(const Appearance &appearance)
{
    if (appearance.level < 0 || appearance.level > LevelFull || appearance.rainbowSpeed < 0 || appearance.rainbowSpeed > MaxRainbowSpeed)
        return false;
    if (appearance.retouches.size() > LiveLedCount)
        return false;
    if (appearance.skyPlace && !isValidPlace(appearance.skyPlace->latitude, appearance.skyPlace->longitude))
        return false;
    std::array<bool, LiveLedCount> seen{};
    for (const Retouch &retouch : appearance.retouches) {
        if (retouch.led >= LiveLedCount || seen.at(retouch.led))
            return false;
        seen.at(retouch.led) = true;
    }
    return true;
}

std::optional<LiveScene> renderAppearance(const Appearance &appearance, std::span<const LedPosition> positions, double seconds, double dayHours,
                                           const SkyDay &skyDay)
{
    if (!isValid(appearance) || positions.empty())
        return std::nullopt;
    std::array<Rgb, LiveLedCount> colours{};
    std::array<bool, LiveLedCount> lit{};
    double low = positions.front().x;
    double high = positions.front().x;
    for (const LedPosition &position : positions) {
        if (position.led >= LiveLedCount)
            return std::nullopt;
        low = std::min(low, position.x);
        high = std::max(high, position.x);
    }
    const double span = high - low;
    const double turn = std::fmod(seconds * appearance.rainbowSpeed / 1000.0, 1.0);
    const auto sky = skyRowColours(dayHours, skyDay);
    for (const LedPosition &position : positions) {
        const double place = span > 0.0 ? (position.x - low) / span : 0.0;
        colours.at(position.led) = baseColour(appearance, position, place, turn, sky);
        lit.at(position.led) = true;
    }
    if (appearance.base == BaseType::Matrix) {
        const auto rain = matrixRainColours(positions, seconds); // the whole picture of the rain: every LED that has a place, the dark ones too
        for (const LedPosition &position : positions)
            colours.at(position.led) = rain.at(position.led);
    }
    if (appearance.base == BaseType::Sky && appearance.skyAnimated)
        animateSky(positions, colours, seconds, skyNightness(dayHours, skyDay)); // before the retouches: they keep their own colour
    for (const Retouch &retouch : appearance.retouches) {
        colours.at(retouch.led) = retouch.colour;
        lit.at(retouch.led) = true; // a retouch lights a LED even where there is no position for it
    }
    std::vector<LedColor> leds;
    leds.reserve(LiveLedCount);
    for (std::size_t led = 0; led < LiveLedCount; ++led) {
        if (!lit.at(led))
            continue;
        const Rgb &colour = colours.at(led);
        leds.push_back({static_cast<std::uint16_t>(led), scaled(colour.red, appearance.level), scaled(colour.green, appearance.level),
                        scaled(colour.blue, appearance.level)});
    }
    return perLedScene(leds);
}

} // namespace galleon::keyboard
