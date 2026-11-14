// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QStringList>
#include <QString>

namespace galleond {

// An application described by a freedesktop .desktop file.
struct DesktopApp
{
    QString file; // absolute path of the .desktop file
    QString id;   // "firefox.desktop"
    QString name; // in the user's language when the file has it
    QString icon;     // icon theme name; empty if the file has none or gives a path
    QString iconPath; // absolute path of an image, when the file gives a path (Snap packages do)
    QStringList categories; // the menu categories (Game, Utility, ...) of the file
    int steamAppId = 0;     // the Steam game that an Exec of steam://rungameid/<n> starts, else 0

    [[nodiscard]] bool isValid() const { return !file.isEmpty(); }
};

// Looks up the applications the desktop knows, to put one on a key without
// typing its command: the key only names a .desktop file, and the desktop's own
// launcher (gio) starts it, with the file's Exec, working directory, terminal
// flag and the rest.
class DesktopEntries
{
public:
    // `desktop`: a desktop file ID ("firefox.desktop", "org.kde.dolphin.desktop")
    // looked up in the applications folders of XDG_DATA_HOME and XDG_DATA_DIRS,
    // or the absolute path of a .desktop file. Not valid or not found: an
    // invalid DesktopApp.
    [[nodiscard]] static DesktopApp lookup(const QString &desktop);

    // Every application that wants to be listed (no Hidden, no NoDisplay), by
    // name, for a picker. A file counts only if it is an Application with an Exec.
    [[nodiscard]] static QList<DesktopApp> list();
#ifdef Q_OS_WIN
    // Initial button labels do not require Shell icon extraction or Steam artwork.
    [[nodiscard]] static QList<DesktopApp> list(bool includeIcons);
#endif

    // The listed applications of one menu category ("Game"): the same as list(), filtered.
    [[nodiscard]] static QList<DesktopApp> listCategory(const QString &category);

    // The names accepted in a pages file.
    [[nodiscard]] static bool isValidReference(const QString &desktop);

    // A bare desktop file ID such as "audacity.desktop": the name of an application on Linux, and the one a theme exported
    // there carries for an application that has no portable id. On Windows it names nothing that can be started: it is
    // accepted when a theme or a pages file is read (so that one application of a shared theme without an equivalent here
    // does not refuse the whole theme) and never resolved: lookup() finds nothing, launch() refuses it, and the key is
    // kept, inert, as for an application that is not installed.
    [[nodiscard]] static bool isPlainDesktopId(const QString &desktop);

    // The portable id (see WellKnownApps.h) this literal reference is one of
    // the known candidates of, or empty if it names none. Used only when
    // exporting a theme, so a shared file never carries a path and stays
    // installable on another system.
    [[nodiscard]] static QString wellKnownIdFor(const QString &desktop);

    // Starts the application through the desktop's launcher (gio launch, else
    // gtk-launch, else kioclient). Returns false if none could be started. The
    // program gets the file as ONE argument; no shell is involved.
    static bool launch(const DesktopApp &app);

private:
    [[nodiscard]] static DesktopApp read(const QString &file, const QString &id);
#ifdef Q_OS_WIN
    [[nodiscard]] static DesktopApp read(const QString &file, const QString &id, bool includeIcons);
#endif
};

} // namespace galleond
