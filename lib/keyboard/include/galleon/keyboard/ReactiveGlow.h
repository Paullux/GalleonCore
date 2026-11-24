// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/Appearance.h"
#include "galleon/keyboard/LiveFrame.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// The minimal reactive effect: a soft glow under a key that was just pressed, which fades out. This file is the PURE ENGINE only. It reads no key
// press: whoever detects them (a later, separate component of galleond, off by default, see AGENTS.md "Security and privacy by design" and
// ROADMAP.md "Conception envisagée de la frappe réactive") calls GlowBoard::touch with the LED of the key at the moment it goes down. Nothing is
// sent, saved or logged here, and the time is an argument.
//
// What the privacy rules make of this:
//   - What the board keeps is the LED and the time of the last touch of at most MaxGlowing keys, in memory, for at most MaxGlowFadeSeconds. Nothing
//     is written anywhere, no sequence is kept (touching a LED again replaces its time), and there is no count, no rate and no history.
//   - It has NO reading function other than shares(), which gives the glow to draw (a LED and how much of the glow is left) for the frame, inside
//     galleond. That result is never exposed to another program, never put in a journal and never given to a test capture (rule 8: a LED that
//     reflects the last key pressed is what must not leave galleond; the diagnostic trace of the lighting is cut while this effect runs).
//   - It is BOUNDED, so a stuck key or a flood of events cannot grow it: when it is full the oldest touch is dropped.
//   - A LED is a LED of the live frame here (0 to 145); turning a key into its LED is the caller's (KeyIdentity and the key map).
namespace galleon::keyboard {

inline constexpr std::size_t MaxGlowing = 64;
inline constexpr double MinGlowFadeSeconds = 0.2;
inline constexpr double MaxGlowFadeSeconds = 5.0;
inline constexpr double DefaultGlowFadeSeconds = 1.5;

// A fade is a finite number of seconds from MinGlowFadeSeconds to MaxGlowFadeSeconds. Pure.
[[nodiscard]] bool isValidGlowFade(double seconds);

// How much of the glow is left `age` seconds after the touch, for a fade of `fade` seconds: 1 at the touch, 0 from `fade` on, falling quickly at
// first then slowly (the square of what is left of the time). An age that is negative or not a number counts as just touched; a fade that is not
// valid counts as DefaultGlowFadeSeconds. Pure.
[[nodiscard]] double glowShare(double age, double fade);

// A LED and how much of its glow is left (above 0, up to 1).
struct LedShare
{
    std::uint16_t led = 0;
    double share = 0;
};

class GlowBoard
{
public:
    // A key went down on `led` at `now`. Touching a LED that glows again starts its glow anew; when MaxGlowing are glowing and this one is new, the
    // oldest is dropped. False, and nothing changed, for a LED beyond the frame (146 or more) or a time that is not a number.
    bool touch(std::uint16_t led, double now);

    // Drops the glows that are over by `now` with this fade (a fade that is not valid counts as the default, so nothing is kept longer than
    // MaxGlowFadeSeconds).
    void expire(double now, double fade);

    // Forgets everything (the effect was switched off, the lighting stopped, the keyboard was given back).
    void clear();

    [[nodiscard]] bool empty() const { return m_touches.empty(); }
    [[nodiscard]] std::size_t size() const { return m_touches.size(); }

    // The glow to draw at `now`, by increasing LED, one entry per LED, only those with something left. For the frame inside galleond only.
    [[nodiscard]] std::vector<LedShare> shares(double now, double fade) const;

private:
    struct Touch
    {
        std::uint16_t led = 0;
        double time = 0;
    };
    std::vector<Touch> m_touches; // oldest first
};

// The frame with each LED of `glow` moved towards `colour` by its share: a share of 1 shows the colour, a share of 0.5 the middle between the
// frame's colour and the colour, and so on until the frame's own colour is back. The colour is scaled by `level` (0 to LevelFull, the brightness
// of the lighting, so that the glow is as dim as the keyboard is asked to be and dark when the level is 0). A LED beyond the frame is ignored. Pure.
[[nodiscard]] LiveFrame withGlow(LiveFrame frame, std::span<const LedShare> glow, Rgb colour, int level);

} // namespace galleon::keyboard
