// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
namespace galleond {
struct GameInstallation { QString directory, title, executable; };
// Local metadata only. Paths and titles are data, never commands to execute.
class GameInstallations {
public:
    static QList<GameInstallation> discover();
    static QJsonObject scanMetadata(); // Isolated --launcher-info child only.
    static int revision();
    static GameInstallation epicManifest(const QByteArray &json);
    static QList<GameInstallation> heroicLibrary(const QByteArray &json);
    static QList<GameInstallation> legendaryManifest(const QByteArray &json);
    static QList<GameInstallation> steamShortcuts(const QByteArray &bytes);
    static GameInstallation lutrisConfig(const QByteArray &yaml, const QString &fallbackTitle);
    static QList<GameInstallation> bottlesConfig(const QByteArray &yaml, const QString &bottleDirectory);
    static GameInstallation registryRecord(const QString &directory, const QString &title);
    static QString titleForProcess(const QString &process, const QList<GameInstallation> &games);
    static QString titleForPath(const QString &path, const QList<GameInstallation> &games);
};
}
