// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TrayMenu.h"

#include <QCoreApplication>

namespace bosun {

namespace {

QString words(const char *text)
{
    return QCoreApplication::translate("TrayMenu", text);
}

} // namespace

QList<TrayEntry> trayEntries(bool windowShown, bool daemonRunning, const QStringList &profiles, const QString &activeProfile)
{
    QList<TrayEntry> entries;
    entries.append({.action = TrayEntry::Action::ToggleWindow, .label = windowShown ? words("Hide") : words("Show")});
    // With a single profile there is nothing to choose: the menu stays as short as it was.
    if (profiles.size() > 1) {
        for (const QString &name : profiles)
            entries.append({.action = TrayEntry::Action::SetProfile, .label = words("Profile: %1").arg(name), .profile = name, .checked = name == activeProfile});
    }
    if (daemonRunning) {
        entries.append({.action = TrayEntry::Action::QuitAndStop, .label = words("Quit and stop galleond")});
        entries.append({.action = TrayEntry::Action::Quit, .label = words("Quit (galleond keeps running)")});
    } else {
        entries.append({.action = TrayEntry::Action::StartDaemon, .label = words("Start galleond")});
        entries.append({.action = TrayEntry::Action::Quit, .label = words("Quit")});
    }
    return entries;
}

} // namespace bosun
