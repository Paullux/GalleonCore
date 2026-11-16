// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/Appearance.h"

#include <array>
#include <span>

// The two small animations of the sky of the mockup of Aaryush1 (lab/mockups/sky-palette.html, issue #111): a SHIMMER by day (each key brightens a
// little, out of step with its neighbours, slowly) and STARS by night (nine stars that drift along the rows of letters, and a few keys that twinkle).
// Pure and stateless: the colours of the picture are a function of the positions of the LEDs and of the time, so the same time gives the same picture,
// nothing is kept between two pictures, and a test can ask for any moment. Nothing here is random in the sense of the machine: the "chance" of the mockup
// is a fixed hash of the star, the lap and the key, so the sky is the same every time.
//
// These effects look at no key press and know of none: the stars drift by themselves and twinkle by chance. (The reactive glow of the mockup, which
// depends on the keys pressed, is a separate layer with its own privacy review.)
namespace galleon::keyboard {

// The time is brought to a step of this many seconds before anything is computed, so that the sky changes at most 8 times a second instead of at every
// frame: a gentle animation does not need more, and it keeps the stream to the keyboard light.
inline constexpr double SkyEffectStep = 0.125;

inline constexpr int SkyStarCount = 9; // as in the mockup
inline constexpr double SkyStarFadeSeconds = 2.5; // the time that a star takes to appear, and to vanish, at the two ends of its walk

// The light that the stars give to each LED at `seconds`, from 0 to 1 (rows 1 to 5 only). It is what animateSky uses, given on its own so that it can be
// tested and drawn. Each star walks its row from the first key to the last, lights the two keys that it is between whatever the distance between them (it
// crosses the big keys, such as the space bar, rather than vanishing), fades in over the first seconds of its walk and fades out over the last ones (so it
// does not appear or vanish with a jump), and then begins a new walk on another row.
[[nodiscard]] std::array<double, LiveLedCount> starLights(std::span<const LedPosition> positions, double seconds);

// Animates `colours` (one colour for each LED of the frame, as the base of the sky left them) at `seconds`: by day the shimmer, by night the stars and
// the twinkling, in the proportion `nightness` (0 in the day, 1 in the night, as skyNightness gives it). Only the LEDs that have a position are
// touched, and not those of the light strip (as in the mockup). `positions` are those of the LEDs that light something.
void animateSky(std::span<const LedPosition> positions, std::span<Rgb, LiveLedCount> colours, double seconds, double nightness);

} // namespace galleon::keyboard
