// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WellKnownApps.h"

#include <QRegularExpression>

#include <array>

namespace galleond {

namespace {

// Built on first use: a global of a type that can throw would have no place to be caught.
const QRegularExpression &idPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9_-]{1,32}$"));
    return pattern;
}

const std::array<WellKnownApp, 1> &table()
{
    static const std::array<WellKnownApp, 1> apps{{
        // Linux IDs from docs/THEME_FORMAT.md's own example (deb, Flatpak, Snap).
        // Windows: the Discord installer's Start Menu shortcut targets Discord.exe
        // under %LocalAppData%\Discord\app-<version>\, with that version info.
        {.id = QStringLiteral("discord"),
         .linuxDesktopIds = {QStringLiteral("discord.desktop"), QStringLiteral("com.discordapp.Discord.desktop"),
                             QStringLiteral("discord_discord.desktop")},
         .windowsExeNames = {QStringLiteral("Discord.exe")},
         .windowsProductName = QStringLiteral("Discord"),
         .windowsCompanyName = QStringLiteral("Discord Inc.")},
    }};
    return apps;
}

} // namespace

bool WellKnownApps::isValidIdSyntax(const QString &id)
{
    return idPattern().match(id).hasMatch();
}

const WellKnownApp *WellKnownApps::find(const QString &id)
{
    for (const WellKnownApp &app : table()) {
        if (app.id == id)
            return &app;
    }
    return nullptr;
}

QStringList WellKnownApps::knownIds()
{
    QStringList ids;
    for (const WellKnownApp &app : table())
        ids.push_back(app.id);
    return ids;
}

bool WellKnownApps::windowsSignatureMatches(const QString &exeBaseName, const QString &productName,
                                            const QString &companyName, const WellKnownApp &app)
{
    bool nameMatches = false;
    for (const QString &expected : app.windowsExeNames) {
        if (exeBaseName.compare(expected, Qt::CaseInsensitive) == 0) {
            nameMatches = true;
            break;
        }
    }
    if (!nameMatches)
        return false;
    if (!app.windowsProductName.isEmpty() && !productName.isEmpty()
        && productName.compare(app.windowsProductName, Qt::CaseInsensitive) != 0)
        return false;
    if (!app.windowsCompanyName.isEmpty() && !companyName.isEmpty()
        && companyName.compare(app.windowsCompanyName, Qt::CaseInsensitive) != 0)
        return false;
    return true;
}

} // namespace galleond
