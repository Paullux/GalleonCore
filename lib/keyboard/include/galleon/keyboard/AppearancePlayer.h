// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/Appearance.h"
#include "galleon/keyboard/LiveScene.h"

#include <optional>
#include <vector>

// Plays an Appearance over time: given the time, it says what scene to show, and only when it is NEW. A still appearance gives one scene and then
// nothing, a slow rainbow gives a scene only when a colour has really changed, a fast one gives a scene at every step. So whoever sends the scenes
// (the live lighting, a frame at a time) sends only what has changed. Pure: time is an argument, nothing is sent, read or kept beyond the last
// scene given. No key press is known here (a later effect that depends on keys will be a separate layer, see ROADMAP.md).
namespace galleon::keyboard {

class AppearancePlayer
{
public:
    // Where the LEDs are (data/devices/galleon-100-sd-led-positions.json). Without any, no scene is given.
    void setPositions(std::vector<LedPosition> positions);

    // Plays this appearance from `seconds` on: a moving rainbow starts at the beginning of its turn, and the next scene is given even if it looks like
    // the last one (it has been asked for). False and nothing changed for an appearance that is not valid.
    [[nodiscard]] bool setAppearance(const Appearance &appearance, double seconds);

    // The next scene has to be given even if it looks like the last: after the keys were taken back (the keyboard shows what it showed before).
    // Only the level changes (0 to LevelFull): the rainbow keeps its place in its turn. False, and nothing changed, when it is out of range or
    // there is no appearance yet.
    [[nodiscard]] bool setLevel(int level);
    void resend();

    [[nodiscard]] bool ready() const { return m_appearance.has_value() && !m_positions.empty(); }

    // The scene to show at `seconds` and at `dayHours` (the hour of the day, which only the sky depends on), or nothing when it is the one already
    // given (and no resend was asked for).
    [[nodiscard]] std::optional<LiveScene> sceneAt(double seconds, double dayHours = 12.0, const SkyDay &skyDay = {});

private:
    std::vector<LedPosition> m_positions;
    std::optional<Appearance> m_appearance;
    double m_origin = 0.0;
    std::optional<LiveScene> m_last;
    bool m_resend = false;
};

} // namespace galleon::keyboard
