// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GameArtworkResolver.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace galleond {
namespace {
QString canonical(QString text) {
    text = text.toLower();
    QString result;
    for (const QChar c : text) if (c.isLetterOrNumber()) result += c;
    return result;
}
bool trustedArtworkUrl(const QUrl &url) {
    if (url.scheme() != QStringLiteral("https") || url.port(-1) != -1 || !url.userInfo().isEmpty()) return false;
    const QString host = url.host().toLower();
    return host == QStringLiteral("s3.amazonaws.com")
        || host == QStringLiteral("steamgriddb.com")
        || host.endsWith(QStringLiteral(".steamgriddb.com"));
}
QUrl apiUrl(const QString &path) {
    return QUrl(QStringLiteral("https://www.steamgriddb.com/api/v2/") + path);
}
QString identifier(const QString &title, int appid) {
    const QString key = appid > 0 ? QStringLiteral("steam:%1").arg(appid)
                                   : QStringLiteral("name:") + canonical(title);
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex());
}
int gameId(const QJsonObject &response) {
    if (!response.value(QStringLiteral("success")).toBool()) return 0;
    const int id = response.value(QStringLiteral("data")).toObject().value(QStringLiteral("id")).toInt();
    return id > 0 ? id : 0;
}
}
GameArtworkResolver::GameArtworkResolver(QObject *parent) : QObject(parent) {}
QString GameArtworkResolver::cachePath(const QString &id) {
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/galleoncore/game-artwork/") + id + QStringLiteral(".png");
}
int GameArtworkResolver::exactGameId(const QJsonObject &response, const QString &title) {
    if (!response.value(QStringLiteral("success")).toBool()) return 0;
    const QString sought = canonical(title);
    if (sought.size() < 3) return 0;
    for (const auto &value : response.value(QStringLiteral("data")).toArray()) {
        const auto game = value.toObject();
        const int id = game.value(QStringLiteral("id")).toInt();
        if (id > 0 && canonical(game.value(QStringLiteral("name")).toString()) == sought) return id;
    }
    return 0; // Never silently use an unrelated autocomplete result.
}
QUrl GameArtworkResolver::firstWideImage(const QJsonObject &response) {
    if (!response.value(QStringLiteral("success")).toBool()) return {};
    for (const auto &value : response.value(QStringLiteral("data")).toArray()) {
        const auto image = value.toObject();
        const int width = image.value(QStringLiteral("width")).toInt();
        const int height = image.value(QStringLiteral("height")).toInt();
        if (width < 400 || height < 180 || qint64(width) * height > 32000000
            || double(width) / height < 1.4) continue;
        const QUrl url(image.value(QStringLiteral("url")).toString());
        if (trustedArtworkUrl(url)) return url;
    }
    return {};
}
QString GameArtworkResolver::artwork(const QString &title, int steamAppId, const QString &apiKey) {
    if (title.trimmed().isEmpty() && steamAppId <= 0) return {};
    const QString id = identifier(title, steamAppId);
    const QString path = cachePath(id);
    const QFileInfo file(path);
    if (file.isFile() && file.size() > 0 && file.size() <= 32LL * 1024 * 1024) {
        QImageReader reader(path);
        const QSize size = reader.size();
        if (size.width() > 0 && size.height() > 0 && qint64(size.width()) * size.height() <= 32000000)
            return QUrl::fromLocalFile(path).toString();
    }
    // In-flight requests and failures never interfere with game detection.
    if (apiKey.isEmpty() || m_busy || m_retryAt.value(id) > QDateTime::currentMSecsSinceEpoch()) return {};
    m_busy = true;
    m_pending = id;
    m_status = QStringLiteral("Looking for SteamGridDB artwork");
    if (steamAppId > 0) {
        apiGet(apiUrl(QStringLiteral("games/steam/") + QString::number(steamAppId)), apiKey,
            [this, id, title, apiKey](const QJsonObject &response) {
                const int game = gameId(response);
                if (game > 0) fetchGrid(id, game, apiKey);
                else search(id, title, apiKey);
            });
    } else {
        search(id, title, apiKey);
    }
    return {};
}
void GameArtworkResolver::apiGet(const QUrl &url, const QString &key, std::function<void(QJsonObject)> done) {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(10000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + key.toUtf8());
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, done = std::move(done)]() mutable {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->read(256 * 1024 + 1);
        QJsonObject result;
        if (reply->error() == QNetworkReply::NoError && status == 200 && payload.size() <= 256 * 1024)
            result = QJsonDocument::fromJson(payload).object();
        else if (status == 401 || status == 403)
            m_status = QStringLiteral("SteamGridDB rejected the API key or access");
        else if (status == 429)
            m_status = QStringLiteral("SteamGridDB rate limit reached");
        else
            m_status = QStringLiteral("SteamGridDB offline or unavailable");
        reply->deleteLater();
        done(result);
    });
}
void GameArtworkResolver::search(const QString &id, const QString &title, const QString &apiKey) {
    QString name = title.trimmed();
    if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) name.chop(4);
    if (name.size() < 3 || name.size() > 120) { finish(id, false); return; }
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(name));
    apiGet(apiUrl(QStringLiteral("search/autocomplete/") + encoded), apiKey,
        [this, id, name, apiKey](const QJsonObject &response) {
            const int game = exactGameId(response, name);
            if (game > 0) fetchGrid(id, game, apiKey);
            else finish(id, false);
        });
}
void GameArtworkResolver::fetchGrid(const QString &id, int gameId, const QString &apiKey) {
    apiGet(apiUrl(QStringLiteral("grids/game/") + QString::number(gameId)
                  + QStringLiteral("?dimensions=920x430,460x215&types=static")), apiKey,
        [this, id](const QJsonObject &response) {
            const QUrl url = firstWideImage(response);
            if (url.isValid()) download(id, url);
            else finish(id, false);
        });
}
void GameArtworkResolver::download(const QString &id, const QUrl &url) {
    if (!trustedArtworkUrl(url)) { finish(id, false); return; }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(12000);
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id] {
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->read(6 * 1024 * 1024 + 1);
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || http != 200 || bytes.size() > 6 * 1024 * 1024) {
            finish(id, false); return;
        }
        QBuffer buffer;
        buffer.setData(bytes);
        if (!buffer.open(QIODevice::ReadOnly)) { finish(id, false); return; }
        QImageReader reader(&buffer);
        const QByteArray format = reader.format().toLower();
        const QSize size = reader.size();
        if ((format != "png" && format != "jpg" && format != "jpeg" && format != "webp")
            || size.width() < 400 || size.height() < 180
            || double(size.width()) / size.height() < 1.4
            || qint64(size.width()) * size.height() > 16000000) {
            finish(id, false); return;
        }
        const QImage image = reader.read();
        if (image.isNull()) { finish(id, false); return; }
        const QString path = cachePath(id);
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) { finish(id, false); return; }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
            file.cancelWriting(); finish(id, false); return;
        }
        finish(id, true);
    });
}
void GameArtworkResolver::finish(const QString &id, bool succeeded) {
    if (!succeeded) {
        m_retryAt[id] = QDateTime::currentMSecsSinceEpoch() + 10 * 60 * 1000;
        if (m_status == QStringLiteral("Looking for SteamGridDB artwork"))
            m_status = QStringLiteral("No matching horizontal artwork; using local fallback");
    } else {
        m_retryAt.remove(id);
        m_status = QStringLiteral("SteamGridDB artwork cached");
    }
    m_busy = false;
    m_pending.clear();
    Q_EMIT artworkReady();
}
}
