// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/KeyIdentity.h"
#include "galleon/keyboard/KeyMap.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// The order of questions of the key calibration: one LED is lit alone, the person says what it was, then the next LED. Pure:
// nothing is lit, read or saved here; a caller lights the LED that currentLed() names, shows the question and passes the answer.
//
// What the flow keeps is the map being built and the path back (the LEDs asked, so that "back" can undo the last answer). It
// never keeps which key was pressed to answer, in which order or when, beyond the map itself (AGENTS.md, "Security and privacy by
// design"): the map says which LED is which key, nothing about use.
namespace galleon::keyboard {

enum class AnswerOutcome : std::uint8_t {
    Recorded,      // the answer is in the map and the flow moved on
    KeyAlreadyUsed,// another LED already lights that key: nothing changed, the same LED is asked again (see conflictingLed)
    Refused,       // the answer is not valid (an unknown key, a strip position out of range) or the flow is finished: nothing changed
    NothingToUndo  // back() at the first question
};

class CalibrationFlow
{
public:
    // `known` are the answers already given (an earlier calibration): they are kept and their LEDs are not asked again.
    explicit CalibrationFlow(const KeyMap &known = {});

    // The LED to light now, or nullopt once every LED has been passed.
    [[nodiscard]] std::optional<std::uint16_t> currentLed() const;
    [[nodiscard]] bool finished() const { return !currentLed().has_value(); }

    // The answers. Each moves on to the next LED that has no answer, except a refused one and KeyAlreadyUsed.
    [[nodiscard]] AnswerOutcome answerKey(HidUsage key);
    [[nodiscard]] AnswerOutcome answerStrip(std::uint8_t position); // a section of the light strip, 1 = leftmost
    [[nodiscard]] AnswerOutcome answerUnlit();                      // nothing could be seen
    // Leaves the LED without an answer (not sure, or not now) and moves on.
    void skip();
    // Undoes the last answer or skip: the LED before is asked again, with its answer forgotten.
    [[nodiscard]] AnswerOutcome back();

    // After KeyAlreadyUsed: the LED that already has the key the person pressed.
    [[nodiscard]] std::optional<std::uint16_t> conflictingLed() const { return m_conflict; }

    [[nodiscard]] const KeyMap &map() const { return m_map; }
    // LEDs that were passed without an answer (skipped), by increasing index; the LEDs after the cursor are not listed.
    [[nodiscard]] std::vector<std::uint16_t> skippedLeds() const;
    // How many of the LedCount LEDs have been passed (answered, skipped or already known).
    [[nodiscard]] std::size_t passed() const { return m_cursor; }

private:
    [[nodiscard]] AnswerOutcome record(const LedAssignment &assignment);
    void advance();

    KeyMap m_map;
    std::size_t m_cursor = 0;
    std::vector<std::uint16_t> m_asked; // the LEDs that were asked, in order, for back()
    std::optional<std::uint16_t> m_conflict;
};

} // namespace galleon::keyboard
