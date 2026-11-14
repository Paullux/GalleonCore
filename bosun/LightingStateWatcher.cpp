// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingStateWatcher.h"

namespace bosun {

LightingStateWatcher::LightingStateWatcher(const QString &path, qint64 notBefore, QObject *parent)
    : QObject(parent), m_path(path), m_notBefore(notBefore), m_watcher(path)
{
    connect(&m_watcher, &galleond::ReplacedFileWatcher::maybeChanged, this, &LightingStateWatcher::refresh);
    refresh();
}

void LightingStateWatcher::refresh()
{
    const auto state = galleond::readLightingState(m_path);
    if (!state || !galleond::isFreshLightingState(*state, m_notBefore) || state == m_last)
        return;
    m_last = state;
    Q_EMIT stateChanged(state->state, state->detail);
}

} // namespace bosun
