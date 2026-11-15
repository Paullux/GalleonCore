// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/LiveFrame.h"

#include <cstdint>
#include <optional>

// What the live lighting shows, as a value, and the frame that carries it. Pure: nothing is sent here and the frame is the
// one LiveFrame.h builds (146 LEDs, the format of docs/protocol/README.md "Live per-key lighting"); a scene only decides
// the colour of each LED.
//
// Two scenes exist so far: one colour for every key, and ONE lit LED with every other LED black. The second is what the
// calibration of the key map needs: it shows the person which key a LED belongs to. Both are output only: a scene says
// what to light, it never says anything about what is pressed.
namespace galleon::keyboard {

struct LiveScene
{
    enum class Kind : std::uint8_t { Uniform, SingleLed };
    Kind kind = Kind::Uniform;
    std::uint16_t led = 0; // SingleLed only
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend bool operator==(const LiveScene &, const LiveScene &) = default;
};

// nullopt when a value is out of range: a colour channel outside 0 to 255, or a LED outside 0 to LiveLedCount - 1.
[[nodiscard]] std::optional<LiveScene> uniformScene(int red, int green, int blue);
[[nodiscard]] std::optional<LiveScene> singleLedScene(int led, int red, int green, int blue);

// nullopt only for a scene built by hand with a LED beyond the frame (the two functions above never make one): nothing is
// sent then. The frame is never built with a missing header.
[[nodiscard]] std::optional<LiveFrame> sceneFrame(const LiveScene &scene);

} // namespace galleon::keyboard
