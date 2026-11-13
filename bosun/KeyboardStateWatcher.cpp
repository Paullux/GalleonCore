// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyboardStateWatcher.h"

namespace bosun {

KeyboardStateWatcher::KeyboardStateWatcher(const QString &path, qint64 notBefore, QObject *parent)
    : QObject(parent), m_path(path), m_notBefore(notBefore), m_watcher(path)
{
    connect(&m_watcher, &galleond::ReplacedFileWatcher::maybeChanged, this, &KeyboardStateWatcher::refresh);
    refresh(); // a value galleond wrote just before this window started, if it is fresh
}

std::optional<int> KeyboardStateWatcher::brightness() const
{
    if (!m_last)
        return std::nullopt;
    return m_last->brightness;
}

void KeyboardStateWatcher::refresh()
{
    const auto state = galleond::readKeyboardState(m_path);
    if (!state || !galleond::isFreshKeyboardState(*state, m_notBefore) || state == m_last)
        return;
    m_last = state;
    Q_EMIT brightnessChanged(state->brightness);
}

} // namespace bosun
