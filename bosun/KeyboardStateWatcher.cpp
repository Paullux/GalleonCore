// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyboardStateWatcher.h"

#include <QDir>
#include <QFileInfo>

namespace bosun {

KeyboardStateWatcher::KeyboardStateWatcher(QString path, qint64 notBefore, QObject *parent)
    : QObject(parent), m_path(std::move(path)), m_notBefore(notBefore)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { watch(); refresh(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { watch(); refresh(); });
    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, [this] { watch(); refresh(); });
    watch();
    refresh(); // a value galleond wrote just before this window started, if it is fresh
}

void KeyboardStateWatcher::watch()
{
    const QString folder = QFileInfo(m_path).absolutePath();
    if (!m_watcher.directories().contains(folder) && !m_watcher.addPath(folder)) {
        m_retry.start(); // the folder is not there yet: try again later
        return;
    }
    m_retry.stop();
    // The write replaces the file by a rename: the old watch is gone with the old file.
    if (QFileInfo::exists(m_path) && !m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
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
