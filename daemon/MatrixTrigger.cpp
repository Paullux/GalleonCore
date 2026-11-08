// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MatrixTrigger.h"

#include <algorithm>

namespace galleond {

namespace {

bool validIndex(MatrixTrigger::Kind kind, int index)
{
    using Kind = MatrixTrigger::Kind;
    if (kind == Kind::KeyPressed || kind == Kind::KeyReleased)
        return index >= 0 && index < galleon::protocol::KeyCount;
    return index >= 0 && index < galleon::protocol::DialCount;
}

} // namespace

MatrixTrigger::Action MatrixTrigger::onEvent(Kind kind, int index, qint64 nowMs)
{
    if (!validIndex(kind, index))
        return Action::None;
    const auto dial = static_cast<std::size_t>(index);

    if (m_running) {
        if (kind == Kind::DialReleased) {
            m_down.at(dial) = false;
            if (m_ignoreRelease.at(dial)) {
                m_ignoreRelease.at(dial) = false;
                return Action::None;
            }
        }
        if (kind == Kind::DialPressed)
            m_down.at(dial) = true;
        m_running = false;
        m_ignoreRelease.fill(false);
        return Action::Stop;
    }

    switch (kind) {
    case Kind::DialPressed:
        m_down.at(dial) = true;
        if (std::ranges::all_of(m_down, [](bool down) { return down; })) {
            m_armed = true;
            m_armedAt = nowMs;
        }
        break;
    case Kind::DialReleased:
        m_down.at(dial) = false;
        m_armed = false;
        break;
    case Kind::DialRotated:
    case Kind::KeyPressed:
    case Kind::KeyReleased:
        m_armed = false; // not a clean hold
        break;
    }
    return Action::None;
}

MatrixTrigger::Action MatrixTrigger::tick(qint64 nowMs)
{
    if (m_running || !m_armed || nowMs - m_armedAt < m_holdMs)
        return Action::None;
    m_armed = false;
    m_running = true;
    m_ignoreRelease = m_down;
    return Action::Start;
}

qint64 MatrixTrigger::msUntilStart(qint64 nowMs) const
{
    if (m_running || !m_armed)
        return -1;
    return std::max<qint64>(0, m_holdMs - (nowMs - m_armedAt));
}

void MatrixTrigger::reset()
{
    m_down.fill(false);
    m_ignoreRelease.fill(false);
    m_armed = false;
    m_running = false;
}

} // namespace galleond
