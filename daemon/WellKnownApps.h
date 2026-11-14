// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A portable application identity: what a shared theme writes instead of an
// OS-specific desktop file ID or path, so one .gctheme installs its "app" key
// on every supported system. A theme may still write a literal desktop file ID
// (Linux) or, for its own machine, let Bosun pick one; this table only covers
// the applications small enough in number to be worth naming portably.
//
// Resolution stays local and happens once, at install time (see
// ThemeInstallPlan.cpp, DesktopEntries::lookup): nothing here is a path, and
// nothing here is ever written back into a shared file.

#include <QList>
#include <QString>
#include <QStringList>

namespace galleond {

// The ProductName and CompanyName of an executable's version resource. An empty one is not checked.
struct WindowsVersionInfo
{
    QString productName;
    QString companyName;
};

struct WellKnownApp
{
    QString id; // "discord": the shape of a theme "id" field (a-z, 0-9, _, -), never a ".desktop" name

    // Linux: desktop file IDs to try, in order, the first installed one wins
    // (the same idea as a theme's own `alternatives`, built in for themes that
    // only name the application portably).
    QStringList linuxDesktopIds;

    // Windows: the shortcut/App Paths target's own file name (what the
    // installer named it, not a user-visible label), matched case-insensitively.
    QStringList windowsExeNames;
    // Optional PE version info the target should also carry when it has version
    // info at all: a corroborating signal, immune to the user's language or a
    // renamed shortcut, on top of the exe name. One entry per version of the
    // application that carries its own (Audacity 3 and 4 do not agree), and each is
    // a pair that has to match as a whole. Left empty: the exe name match alone is
    // accepted, which is all that many applications leave to go on (no version info).
    QList<WindowsVersionInfo> windowsVersionInfos;
};

// The small built-in table of applications a theme may name portably.
class WellKnownApps
{
public:
    // The shape a portable id must have: never a ".desktop" name or a path, so
    // a reference string can never be read as both at once.
    [[nodiscard]] static bool isValidIdSyntax(const QString &id);

    // nullptr if `id` is not in the table (a syntactically valid id can still be unknown).
    [[nodiscard]] static const WellKnownApp *find(const QString &id);

    [[nodiscard]] static QStringList knownIds();

    // True if `exeBaseName` (and, when the table and the candidate both have
    // it, `productName`/`companyName`) match what `app` expects. A metadata
    // field either side does not have is simply not checked: it never turns a
    // match into a mismatch, only a mismatch stays a mismatch.
    [[nodiscard]] static bool windowsSignatureMatches(const QString &exeBaseName, const QString &productName,
                                                       const QString &companyName, const WellKnownApp &app);
};

} // namespace galleond
