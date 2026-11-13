// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ReplacedFileWatcher.h"

#include <QFileInfo>

namespace galleond {

ReplacedFileWatcher::ReplacedFileWatcher(QString path, QObject *parent) : QObject(parent), m_path(std::move(path))
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { watch(); Q_EMIT maybeChanged(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { watch(); Q_EMIT maybeChanged(); });
    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, [this] { watch(); });
    watch();
}

void ReplacedFileWatcher::watch()
{
    const QString folder = QFileInfo(m_path).absolutePath();
    if (!m_watcher.directories().contains(folder) && !m_watcher.addPath(folder)) {
        m_retry.start(); // the folder is not there yet: try again later
        return;
    }
    const bool wasRetrying = m_retry.isActive();
    m_retry.stop();
    // The write replaces the file by a rename: the old watch is gone with the old file.
    if (QFileInfo::exists(m_path) && !m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
    if (wasRetrying)
        Q_EMIT maybeChanged(); // the folder appeared; the file may already be in it
}

} // namespace galleond
