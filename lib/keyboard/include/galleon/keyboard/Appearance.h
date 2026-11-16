// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/LiveScene.h"
#include "galleon/keyboard/SunTimes.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

// What the live lighting looks like, as data: a BASE (one colour, or a rainbow), RETOUCHES on single LEDs, and a LEVEL. Pure: the colours come
// from this description, the positions of the LEDs and the time, all given as arguments; nothing is sent, read or saved here, and nothing
// here knows about key presses (a later effect that does will be a separate layer on top, see ROADMAP.md, "Sécurité et vie privée").
//
// Order of rendering: the base (it may move with time), then the retouches over it (they keep their colour), then the level (a factor on
// every colour, 0 to 1000 as the brightness of the keyboard). "One colour per key" is a base of black with retouches.
namespace galleon::keyboard {

struct Rgb
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend bool operator==(const Rgb &, const Rgb &) = default;
};

enum class BaseType : std::uint8_t { Uniform, Rainbow, Sky }; // Sky: the colours of the hour of the day (SkyPalette.h), six rows

struct Retouch
{
    std::uint16_t led = 0;
    Rgb colour;

    friend bool operator==(const Retouch &, const Retouch &) = default;
};

inline constexpr int LevelFull = 1000; // as the brightness of the keyboard: 0 is dark, 1000 is the colours as they are
inline constexpr int MaxRainbowSpeed = 1000; // thousandths of a turn of the rainbow per second: 0 is fixed, 1000 is one turn a second

struct Appearance
{
    BaseType base = BaseType::Uniform;
    Rgb colour;               // Uniform: the colour of every LED that lights something
    int rainbowSpeed = 0;     // Rainbow: 0 = fixed, up to MaxRainbowSpeed
    std::vector<Retouch> retouches; // at most LiveLedCount, one per LED
    int level = LevelFull;    // 0 to LevelFull
    // Sky only: follow the real sunrise and sunset instead of the fixed hours of the mockup (off by default), and the place typed in the settings of the
    // sky, used when the weather widget has none. A place is personal data: it is never logged.
    bool followSun = false;
    std::optional<GeoPlace> skyPlace;

    friend bool operator==(const Appearance &, const Appearance &) = default;
};

// Where a LED is on the keyboard, in millimetres (data/devices/galleon-100-sd-led-positions.json). Only the LEDs that light something have one.
struct LedPosition
{
    std::uint16_t led = 0;
    double x = 0;
    double y = 0;
};

// True when every value is in range: retouches on LEDs of the frame, no LED twice, at most LiveLedCount of them, a level and a speed within their bounds.
[[nodiscard]] bool isValid(const Appearance &appearance);

// The colours of every LED that has a position at `seconds` (the time only matters to a moving rainbow) and at `dayHours` (the hour of the day, 0 to
// 24, which only the sky depends on), as a scene of one colour per LED.
// nullopt for an appearance that is not valid or for no position at all: nothing is sent then.
[[nodiscard]] std::optional<LiveScene> renderAppearance(const Appearance &appearance, std::span<const LedPosition> positions, double seconds,
                                                        double dayHours = 12.0);

// One colour of the rainbow, by the place along the keyboard (0 at the left end, 1 at the right end) and the turn it has made (0 to 1, from the
// time): the same function whoever draws it, the keyboard and the preview of the settings window.
[[nodiscard]] Rgb rainbowColour(double place, double turn);

} // namespace galleon::keyboard
