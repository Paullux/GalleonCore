// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeIcon.h"

#include <QIcon>
#include <QMutexLocker>

namespace galleond {

namespace {

// One lock for the whole process, because the icon loader it guards is one for the whole process.
QMutex &themeIconMutex()
{
    static QMutex mutex;
    return mutex;
}

} // namespace

ThemeIconLock::ThemeIconLock() { themeIconMutex().lock(); }

ThemeIconLock::~ThemeIconLock() { themeIconMutex().unlock(); }

QImage themeIconImage(const QString &name, int size, const QString &fallback)
{
    const ThemeIconLock lock;
    QIcon icon = QIcon::fromTheme(name);
    if (icon.isNull() && !fallback.isEmpty())
        icon = QIcon::fromTheme(fallback);
    if (icon.isNull())
        return {};
    return icon.pixmap(size, size).toImage();
}

} // namespace galleond
