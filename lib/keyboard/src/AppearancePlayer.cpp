// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/AppearancePlayer.h"

#include <optional>
#include <utility>
#include <vector>

namespace galleon::keyboard {

void AppearancePlayer::setPositions(std::vector<LedPosition> positions)
{
    m_positions = std::move(positions);
    m_resend = true;
}

bool AppearancePlayer::setAppearance(const Appearance &appearance, double seconds)
{
    if (!isValid(appearance))
        return false;
    m_appearance = appearance;
    m_origin = seconds;
    m_resend = true;
    return true;
}

void AppearancePlayer::resend()
{
    m_resend = true;
}

std::optional<LiveScene> AppearancePlayer::sceneAt(double seconds)
{
    if (!ready() || !m_appearance)
        return std::nullopt;
    auto scene = renderAppearance(*m_appearance, m_positions, seconds - m_origin);
    if (!scene)
        return std::nullopt;
    if (!m_resend && m_last && *m_last == *scene)
        return std::nullopt; // nothing new to show
    m_resend = false;
    m_last = scene;
    return scene;
}

} // namespace galleon::keyboard
