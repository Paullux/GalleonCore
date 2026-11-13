// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

namespace galleond {

// Says when a file that is REPLACED by a rename on every write (the atomic way: QSaveFile) may have changed.
// Watching the file itself loses the watch with the old file, so the folder is watched and the file watched
// again each time. The folder may not exist yet (the program that writes the file creates it): until it does,
// this retries every few seconds. It carries no data and trusts nothing: the receiver re-reads and validates
// the file, so a signal for nothing costs one small read. One implementation for every such file.
class ReplacedFileWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ReplacedFileWatcher(QString path, QObject *parent = nullptr);

Q_SIGNALS:
    // The folder or the file changed (or the folder just appeared): read the file again.
    void maybeChanged();

private:
    void watch();

    QString m_path;
    QFileSystemWatcher m_watcher;
    QTimer m_retry;
};

} // namespace galleond
