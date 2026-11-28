// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace bosun {

// What the menu of the icon in the notification area offers.
struct TrayEntry
{
    enum class Action : unsigned char {
        ToggleWindow,   // show the window, or hide it when it is shown
        StartDaemon,    // ask systemd to start galleond
        QuitAndStop,    // close the window and stop galleond too
        Quit,           // close the window only
        SetProfile      // show the profile named by `profile` (daemon/Profiles.h)
    };
    Action action = Action::Quit;
    QString label;
    QString profile{}; // NOLINT(readability-redundant-member-init): braced initializers elsewhere leave it out. SetProfile: the name of the profile (a name that is valid, from the files of the profiles folder)
    bool checked = false; // SetProfile: this is the active profile
};

// The entries, in order, for the state of the window and of galleond. Pure.
//   - Show / Hide the window.
//   - With more than one profile: one entry per profile, the active one checked ("Profile: gaming").
//   - galleond running: "Quit and stop galleond" (the module goes back to its numeric keypad)
//     and "Quit" (only this window; galleond stays); galleond not running: "Start galleond"
//     and "Quit".
[[nodiscard]] QList<TrayEntry> trayEntries(bool windowShown, bool daemonRunning, const QStringList &profiles = {}, const QString &activeProfile = {});

} // namespace bosun
