// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/KeyMap.h"

#include "galleon/keyboard/KeyIdentity.h"
#include "galleon/keyboard/LiveFrame.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace galleon::keyboard {

namespace {

[[nodiscard]] std::optional<AssignError> detailError(const LedAssignment &assignment)
{
    switch (assignment.kind) {
    case LedKind::Key:
        return keyName(assignment.key).has_value() ? std::nullopt : std::optional<AssignError>(AssignError::UnknownKey);
    case LedKind::Strip:
        if (assignment.stripPosition == 0 || assignment.stripPosition > MaxStripPosition)
            return AssignError::BadStripPosition;
        return std::nullopt;
    case LedKind::Unlit:
        return std::nullopt;
    }
    return AssignError::MissingDetail; // a value outside the enumeration: refused
}

} // namespace

std::optional<AssignError> KeyMap::assign(const LedAssignment &assignment)
{
    if (assignment.led >= LiveLedCount)
        return AssignError::LedOutOfRange;
    if (const auto error = detailError(assignment))
        return error;
    if (assignment.kind == LedKind::Key) {
        const auto other = ledForKey(assignment.key);
        if (other && *other != assignment.led)
            return AssignError::KeyAlreadyUsed;
    }
    m_leds.at(assignment.led) = assignment;
    return std::nullopt;
}

std::optional<LedAssignment> KeyMap::assignmentFor(std::uint16_t led) const
{
    return led < LiveLedCount ? m_leds.at(led) : std::nullopt;
}

std::optional<std::uint16_t> KeyMap::ledForKey(HidUsage key) const
{
    for (std::size_t i = 0; i < m_leds.size(); ++i) {
        const auto &entry = m_leds.at(i);
        if (entry && entry->kind == LedKind::Key && entry->key == key)
            return static_cast<std::uint16_t>(i);
    }
    return std::nullopt;
}

std::vector<std::uint16_t> KeyMap::unassignedLeds() const
{
    std::vector<std::uint16_t> missing;
    for (std::size_t i = 0; i < m_leds.size(); ++i) {
        if (!m_leds.at(i))
            missing.push_back(static_cast<std::uint16_t>(i));
    }
    return missing;
}

std::size_t KeyMap::assignedCount() const
{
    return LiveLedCount - unassignedLeds().size();
}

std::optional<std::uint16_t> assumedLedForKey(HidUsage key)
{
    if (key >= LiveLedCount || !keyName(key).has_value())
        return std::nullopt;
    return key;
}

} // namespace galleon::keyboard
