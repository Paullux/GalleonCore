// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "RingSettings.h"

#include <galleon/Protocol.h>

#include <array>
#include <cstdint>

namespace galleond {

// The green rain of the hidden effect on the ring of a dial: the same picture as on the screen, the keys and the keyboard (an almost white head, a trail
// that goes from green to black, black elsewhere, in a loop), on the four segments of a ring. This is the module's own copy, as the rule of separation
// of the two parts of galleond asks (the module side does not call the keyboard's library): a test checks that its colours are those of the keyboard.
// Pure and stateless: the picture of a ring is a function of the dial and of the number of the picture of the rain (the step of the whole effect, the same
// that numbers the pictures of the screen). It knows nothing of key presses.
//
// Which segment is where around the ring is not known yet (see protocol::dialRingSegmentReport): "falling" here means from the first segment to the last.
struct RingRgb
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend bool operator==(const RingRgb &, const RingRgb &) = default;
};

inline constexpr RingRgb MatrixRingHead{216, 255, 224}; // #d8ffe0, as widgets/MatrixRain.qml
inline constexpr RingRgb MatrixRingTrail{0, 212, 58};   // #00d43a
inline constexpr int MatrixRingTrailSegments = 3;       // the trail behind the head (a ring has four segments)
inline constexpr int MatrixRingRestSegments = 2;        // the steps of waiting before the head begins again
inline constexpr int MatrixRingCycle = galleon::protocol::DialRingLeds + MatrixRingTrailSegments + MatrixRingRestSegments;

// The colours of the four segments of the ring of `dial` at the picture number `frame` (0, 1, 2, ... one per picture of the rain). Each ring has its own
// pace and its own place in the cycle, so that the two rings do not move together. Dark for a dial or a frame out of range.
[[nodiscard]] std::array<RingRgb, galleon::protocol::DialRingLeds> matrixRingSegments(int dial, int frame);

// The colour of one segment as the rings are set (RingSettings.h): each channel scaled by the level, but at least by RingShownMinLevel, even with the rings
// put out or at level 0: the green rain is started on purpose and relights them (floorRingColour). A dark segment stays dark, and a full level leaves the
// colour as it is. Pure.
[[nodiscard]] RingRgb matrixRingShown(const RingRgb &colour, const RingSettings &settings);

} // namespace galleond
