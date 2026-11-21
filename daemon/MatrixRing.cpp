// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MatrixRing.h"

#include <algorithm>
#include <cmath>

namespace galleond {

namespace {

// Pictures of the rain for one step of the head, and the first place in the cycle, of each ring.
struct RingPace
{
    int framesPerStep;
    int offset;
};

// The first ring begins with its head on the first segment (the effect shows at once); the second begins later in its cycle, with a trail.
constexpr std::array<RingPace, galleon::protocol::DialCount> Paces{{{4, 3}, {3, 7}}};

[[nodiscard]] std::uint8_t scaled(std::uint8_t channel, double factor)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(channel * factor, 0.0, 255.0)));
}

} // namespace

std::array<RingRgb, galleon::protocol::DialRingLeds> matrixRingSegments(int dial, int frame)
{
    std::array<RingRgb, galleon::protocol::DialRingLeds> colours{};
    if (dial < 0 || dial >= galleon::protocol::DialCount || frame < 0)
        return colours;
    const RingPace pace = Paces.at(static_cast<std::size_t>(dial));
    // As the keyboard: the head goes through the segments and a few more steps, then waits; it may lie before the first segment (the trail comes in first).
    const int head = ((frame / pace.framesPerStep) + pace.offset) % MatrixRingCycle - MatrixRingTrailSegments;
    for (int segment = 0; segment < galleon::protocol::DialRingLeds; ++segment) {
        const int behind = head - segment; // 0 on the head, 1 on the segment before it, ...
        if (behind < 0 || behind > MatrixRingTrailSegments)
            continue; // after the head, or past the end of the trail: dark
        if (behind == 0) {
            colours.at(static_cast<std::size_t>(segment)) = MatrixRingHead;
        } else {
            const double fade = 1.0 - static_cast<double>(behind) / (MatrixRingTrailSegments + 1); // from the head to black
            colours.at(static_cast<std::size_t>(segment)) = {scaled(MatrixRingTrail.red, fade), scaled(MatrixRingTrail.green, fade),
                                                             scaled(MatrixRingTrail.blue, fade)};
        }
    }
    return colours;
}

RingRgb matrixRingShown(const RingRgb &colour, const RingSettings &settings)
{
    const std::uint32_t packed = (static_cast<std::uint32_t>(colour.red) << 16) | (static_cast<std::uint32_t>(colour.green) << 8) | colour.blue;
    const std::uint32_t shown = packed == 0 ? 0 : floorRingColour(packed, settings); // a dark segment stays dark; the rest is at least at the minimum
    return {static_cast<std::uint8_t>(shown >> 16), static_cast<std::uint8_t>(shown >> 8), static_cast<std::uint8_t>(shown)};
}

} // namespace galleond
