// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <functional>

namespace galleond {
// Optional, asynchronous SteamGridDB artwork lookup. Never blocks the FPS/rendering loop.
// A missing or refused user-supplied key leaves the local artwork/fallback untouched.
class GameArtworkResolver final : public QObject {
    Q_OBJECT
public:
    explicit GameArtworkResolver(QObject *parent = nullptr);
    [[nodiscard]] QString artwork(const QString &title, int steamAppId, const QString &apiKey);
    [[nodiscard]] QString preview(const QString &title, int steamAppId, const QString &apiKey, int index);
    [[nodiscard]] int choiceCount(const QString &title, int steamAppId);
    void clearCacheState();
    [[nodiscard]] bool busy() const { return m_busy; }
    [[nodiscard]] static QString gameKey(const QString &title, int steamAppId);
    [[nodiscard]] static QString selectedArtwork(const QString &title, int steamAppId);
    static bool saveSelection(const QString &title, int steamAppId, const QString &localImage);
    [[nodiscard]] static QList<QUrl> wideImages(const QJsonObject &response);
    [[nodiscard]] static int searchGameId(const QJsonObject &response, const QString &title);
    [[nodiscard]] QString status() const { return m_status; }
    // Pure parsers used by offline tests; no network dependency.
    [[nodiscard]] static int exactGameId(const QJsonObject &response, const QString &title);
    [[nodiscard]] static QStringList searchTitles(const QString &title);
    [[nodiscard]] static QUrl firstWideImage(const QJsonObject &response);
Q_SIGNALS:
    void artworkReady();
private:
    void loadChoices(const QString &id);
    void rememberChoices(const QString &id, const QList<QUrl> &urls);
    void requestChoices(const QString &title, int steamAppId, const QString &apiKey);
    void apiGet(const QUrl &url, const QString &key, std::function<void(QJsonObject, int)> done);
    void search(const QString &id, const QString &title, const QString &apiKey, int attempt = 0);
    void fetchGrid(const QString &id, int gameId, const QString &apiKey);
    void download(const QString &id, const QUrl &url);
    void finish(const QString &id, bool succeeded);
    [[nodiscard]] static QString cachePath(const QString &id);
    QNetworkAccessManager m_network;
    QHash<QString, qint64> m_retryAt;
    QHash<QString, QList<QUrl>> m_choices;
    QString m_status, m_pending, m_keyFingerprint;
    bool m_busy = false;
    quint64 m_generation = 0;
};
}
