// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/CalibrationFlow.h"

#include "galleon/keyboard/LiveFrame.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace galleon::keyboard {

CalibrationFlow::CalibrationFlow(const KeyMap &known) : m_map(known)
{
    advance(); // the first LED without an answer
}

void CalibrationFlow::advance()
{
    while (m_cursor < LiveLedCount && m_map.assignmentFor(static_cast<std::uint16_t>(m_cursor)).has_value())
        ++m_cursor;
}

std::optional<std::uint16_t> CalibrationFlow::currentLed() const
{
    return m_cursor < LiveLedCount ? std::optional<std::uint16_t>(static_cast<std::uint16_t>(m_cursor)) : std::nullopt;
}

AnswerOutcome CalibrationFlow::record(const LedAssignment &assignment)
{
    m_conflict.reset();
    if (finished())
        return AnswerOutcome::Refused;
    const auto error = m_map.assign(assignment);
    if (error == AssignError::KeyAlreadyUsed) {
        m_conflict = m_map.ledForKey(assignment.key);
        return AnswerOutcome::KeyAlreadyUsed;
    }
    if (error)
        return AnswerOutcome::Refused;
    m_asked.push_back(assignment.led);
    ++m_cursor;
    advance();
    return AnswerOutcome::Recorded;
}

AnswerOutcome CalibrationFlow::answerKey(HidUsage key)
{
    const auto led = currentLed();
    if (!led)
        return AnswerOutcome::Refused;
    LedAssignment assignment;
    assignment.led = *led;
    assignment.kind = LedKind::Key;
    assignment.key = key;
    return record(assignment);
}

AnswerOutcome CalibrationFlow::answerStrip(std::uint8_t position)
{
    const auto led = currentLed();
    if (!led)
        return AnswerOutcome::Refused;
    LedAssignment assignment;
    assignment.led = *led;
    assignment.kind = LedKind::Strip;
    assignment.stripPosition = position;
    return record(assignment);
}

AnswerOutcome CalibrationFlow::answerUnlit()
{
    const auto led = currentLed();
    if (!led)
        return AnswerOutcome::Refused;
    LedAssignment assignment;
    assignment.led = *led;
    assignment.kind = LedKind::Unlit;
    return record(assignment);
}

void CalibrationFlow::skip()
{
    m_conflict.reset();
    if (finished())
        return;
    m_asked.push_back(static_cast<std::uint16_t>(m_cursor));
    ++m_cursor;
    advance();
}

AnswerOutcome CalibrationFlow::back()
{
    m_conflict.reset();
    if (m_asked.empty())
        return AnswerOutcome::NothingToUndo;
    const std::uint16_t led = m_asked.back();
    m_asked.pop_back();
    static_cast<void>(m_map.clear(led)); // nothing to forget when that LED had been skipped
    m_cursor = led;
    return AnswerOutcome::Recorded;
}

std::vector<std::uint16_t> CalibrationFlow::skippedLeds() const
{
    std::vector<std::uint16_t> skipped;
    for (std::size_t led = 0; led < m_cursor; ++led) {
        if (!m_map.assignmentFor(static_cast<std::uint16_t>(led)).has_value())
            skipped.push_back(static_cast<std::uint16_t>(led));
    }
    return skipped;
}

} // namespace galleon::keyboard
