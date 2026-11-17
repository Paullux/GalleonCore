// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/Appearance.h"

#include <array>
#include <span>

// The green rain of the hidden effect, on the LEDs of the keyboard: the same picture as the one that galleond draws on the screen and on the 12 keys of the
// Stream Deck (widgets/MatrixRain.qml): a column of keys has a "head" that falls, almost white, with a trail above it that goes from green to black; the
// keys that no head or trail touches are dark; and it goes on in a loop, each column at its own speed and at its own place in the cycle.
// Pure and stateless: the picture is a function of the positions of the LEDs and of the time, so any moment can be tested. It knows nothing of key presses.
namespace galleon::keyboard {

// One step of the rain lasts this long, as on the module (about 11 pictures a second).
inline constexpr double MatrixTickSeconds = 0.09;

// The colours of the LEDs that have a position, at `seconds` since the rain began (every other LED is dark). The columns are the columns of keys, the
// rows the lines of keys, with the light strip as one more line above them.
[[nodiscard]] std::array<Rgb, LiveLedCount> matrixRainColours(std::span<const LedPosition> positions, double seconds);

} // namespace galleon::keyboard
