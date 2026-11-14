// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AtomicFile.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QThread>

namespace galleond {

bool retryUntilDone(int attempts, const std::function<bool()> &once, const std::function<void(int)> &wait)
{
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        if (once())
            return true;
        if (attempt < attempts)
            wait(attempt);
    }
    return false;
}

bool writeFileAtomically(const QString &path, const QByteArray &bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    // A QSaveFile cannot commit twice: a refused rename removes its temporary file, so every try writes the whole file again.
    return retryUntilDone(
        MaxAttempts,
        [&] {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly))
                return false;
            file.write(bytes);
            return file.commit();
        },
        [](int) { QThread::msleep(RetryDelayMs); });
}

} // namespace galleond
