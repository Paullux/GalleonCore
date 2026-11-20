// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QImage>
#include <QMutex>
#include <QString>

namespace galleond {

// The icons of the desktop's theme, one thread at a time.
//
// On a KDE desktop, QIcon::fromTheme() and QIcon::pixmap() go through KIconLoader (installed by the Plasma platform theme), which does not
// stand two threads at once. The settings window asks for icons from more than one thread: the GUI thread, and the image loader thread of
// Qt Quick, for the tiles of the keys (image://tiles) and for the icons of the key editor (image://themeicon). Two of them in KIconLoader
// together crashed the window (a segmentation fault in KIconLoader::loadScaledIcon, with two threads in it in the core dumps of the owner).
// KIconLoader is one resource of the whole process, so the lock is too: every place that loads an icon of the theme goes through this
// function, never through QIcon directly. The lock is held only while the icon is loaded and loading waits for no other thread.
//
// The icon `name` at `size` x `size` pixels, or `fallback` when the theme has no `name` (when `fallback` is not empty); a null image when
// neither is in the theme.
[[nodiscard]] QImage themeIconImage(const QString &name, int size, const QString &fallback = {});

// The lock itself, for the test that holds it to see that the loading waits.
class ThemeIconLock
{
public:
    ThemeIconLock();
    ~ThemeIconLock();
    ThemeIconLock(const ThemeIconLock &) = delete;
    ThemeIconLock &operator=(const ThemeIconLock &) = delete;
};

} // namespace galleond
