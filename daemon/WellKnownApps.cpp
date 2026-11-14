// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WellKnownApps.h"

#include <QRegularExpression>

#include <algorithm>
#include <array>

namespace galleond {

namespace {

// Built on first use: a global of a type that can throw would have no place to be caught.
const QRegularExpression &idPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9_-]{1,32}$"));
    return pattern;
}

const std::array<WellKnownApp, 7> &table()
{
    static const std::array<WellKnownApp, 7> apps{{
        // Linux IDs from docs/THEME_FORMAT.md's own example (deb, Flatpak, Snap).
        // Windows: the Discord installer's Start Menu shortcut targets Discord.exe
        // under %LocalAppData%\Discord\app-<version>\, with that version info.
        {.id = QStringLiteral("discord"),
         .linuxDesktopIds = {QStringLiteral("discord.desktop"), QStringLiteral("com.discordapp.Discord.desktop"),
                             QStringLiteral("discord_discord.desktop")},
         .windowsExeNames = {QStringLiteral("Discord.exe")},
         .windowsVersionInfos = {{QStringLiteral("Discord"), QStringLiteral("Discord Inc.")}}},
        // The six that follow: Linux desktop file IDs as a theme exported on Linux names them (the "son" theme of the
        // owner, 14 November 2026); Windows: the target of each application's Start Menu shortcut, read on the owner's PC
        // the same day. Several carry no version information at all: the exe name alone is all there is to go on.
        // Audacity 3 and Audacity 4 are two different installs with different version information.
        {.id = QStringLiteral("audacity"),
         .linuxDesktopIds = {QStringLiteral("audacity.desktop")},
         .windowsExeNames = {QStringLiteral("Audacity.exe"), QStringLiteral("Audacity4.exe")},
         .windowsVersionInfos = {{QStringLiteral("Audacity"), QStringLiteral("Audacity Team")},
                                 {QStringLiteral("Audacity 4.0"), QStringLiteral("MuseCY Holdings Ltd")}}},
        {.id = QStringLiteral("ardour"),
         .linuxDesktopIds = {QStringLiteral("ardour.desktop")},
         .windowsExeNames = {QStringLiteral("Ardour.exe")},
         .windowsVersionInfos = {}},
        {.id = QStringLiteral("kdenlive"),
         .linuxDesktopIds = {QStringLiteral("org.kde.kdenlive.desktop")},
         .windowsExeNames = {QStringLiteral("kdenlive.exe")},
         .windowsVersionInfos = {}},
        {.id = QStringLiteral("lmms"),
         .linuxDesktopIds = {QStringLiteral("lmms.desktop")},
         .windowsExeNames = {QStringLiteral("lmms.exe")},
         .windowsVersionInfos = {{QStringLiteral("LMMS"), QStringLiteral("LMMS Developers")}}},
        // The executable carries the major version (MuseScore4.exe): another one needs a new line here.
        {.id = QStringLiteral("musescore"),
         .linuxDesktopIds = {QStringLiteral("mscore.desktop")},
         .windowsExeNames = {QStringLiteral("MuseScore4.exe")},
         .windowsVersionInfos = {}},
        {.id = QStringLiteral("tuxguitar"),
         .linuxDesktopIds = {QStringLiteral("ar.com.tuxguitar.TuxGuitar.desktop")},
         .windowsExeNames = {QStringLiteral("tuxguitar.exe")},
         .windowsVersionInfos = {}},
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
    // Nothing to compare against: no version info recorded, or none in the executable (the exe name alone).
    if (app.windowsVersionInfos.isEmpty() || (productName.isEmpty() && companyName.isEmpty()))
        return true;
    // One recorded version has to fit as a whole: a product of one version with the publisher of another is no version.
    return std::ranges::any_of(app.windowsVersionInfos, [&](const WindowsVersionInfo &known) {
        const bool productFits = known.productName.isEmpty() || productName.isEmpty() || productName.compare(known.productName, Qt::CaseInsensitive) == 0;
        const bool companyFits = known.companyName.isEmpty() || companyName.isEmpty() || companyName.compare(known.companyName, Qt::CaseInsensitive) == 0;
        return productFits && companyFits;
    });
}

} // namespace galleond
