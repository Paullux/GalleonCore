// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GameArtworkResolver.h"
#include "GameMode.h"
#include "WallpaperCatalog.h"
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
QString alternateSequel(QString title) {
    // Convert only an isolated final sequel number, never letters inside words
    // or release years. Keep persistent artwork identifiers unchanged.
    const QStringList romans{QStringLiteral("I"), QStringLiteral("II"), QStringLiteral("III"), QStringLiteral("IV"),
        QStringLiteral("V"), QStringLiteral("VI"), QStringLiteral("VII"), QStringLiteral("VIII"), QStringLiteral("IX"),
        QStringLiteral("X"), QStringLiteral("XI"), QStringLiteral("XII"), QStringLiteral("XIII"), QStringLiteral("XIV"),
        QStringLiteral("XV"), QStringLiteral("XVI"), QStringLiteral("XVII"), QStringLiteral("XVIII"), QStringLiteral("XIX"), QStringLiteral("XX")};
    const auto match = QRegularExpression(QStringLiteral("\\b([0-9]{1,2}|[IVXivx]{1,5})[™®]?\\s*$")).match(title);
    if (!match.hasMatch() || match.capturedStart(1) == 0) return {};
    const auto token = match.captured(1);
    const int roman = int(romans.indexOf(token.toUpper()));
    if (roman >= 0) title.replace(match.capturedStart(1), token.size(), QString::number(roman + 1));
    else {
        bool ok = false;
        const int number = token.toInt(&ok);
        if (!ok || number < 1 || number > romans.size() || token != QString::number(number)) return {};
        title.replace(match.capturedStart(1), token.size(), romans[number - 1]);
    }
    return title;
}
bool trustedArtworkUrl(const QUrl &url) {
    if (url.scheme() != QStringLiteral("https") || url.port(-1) != -1 || !url.userInfo().isEmpty()) return false;
    const QString host = url.host().toLower();
    return host == QStringLiteral("s3.amazonaws.com")
        || host == QStringLiteral("steamgriddb.com")
        || host.endsWith(QStringLiteral(".steamgriddb.com"));
}
QUrl apiUrl(const QString &path) {
    return {QStringLiteral("https://www.steamgriddb.com/api/v2/") + path};
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
QString GameArtworkResolver::gameKey(const QString &title, int steamAppId) { return identifier(title, steamAppId); }
QString GameArtworkResolver::selectedArtwork(const QString &title, int steamAppId) {
    const QString path = WallpaperCatalog::userRoot() + QStringLiteral("/mine/gaming-") + identifier(title, steamAppId) + QStringLiteral(".png");
    if (!WallpaperCatalog::isAcceptable(path)) return {};
    auto url = QUrl::fromLocalFile(path);
    url.setQuery(QStringLiteral("v=") + QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch()));
    return url.toString();
}
bool GameArtworkResolver::saveSelection(const QString &title, int steamAppId, const QString &localImage) {
    const QUrl url(localImage);
    if (!url.isLocalFile() || !WallpaperCatalog::isAcceptable(url.toLocalFile())) return false;
    QFile input(url.toLocalFile());
    if (!input.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = input.read(32LL * 1024 * 1024 + 1);
    if (data.size() > 32LL * 1024 * 1024) return false;
    const QString folder = WallpaperCatalog::userRoot() + QStringLiteral("/mine");
    if (!QDir().mkpath(folder)) return false;
    QSaveFile output(folder + QStringLiteral("/gaming-") + identifier(title, steamAppId) + QStringLiteral(".png"));
    return output.open(QIODevice::WriteOnly) && output.write(data) == data.size() && output.commit();
}
void GameArtworkResolver::clearCacheState() {
    ++m_generation;
    m_busy = false;
    m_choices.clear(); m_retryAt.clear(); m_status.clear();
    for (auto *reply : m_network.findChildren<QNetworkReply *>()) reply->abort();
}
void GameArtworkResolver::loadChoices(const QString &id) {
    if (m_choices.contains(id)) return;
    if (m_choices.size() >= 64) m_choices.clear();
    QFile file(cachePath(id) + QStringLiteral(".json"));
    QList<QUrl> urls;
    if (file.open(QIODevice::ReadOnly)) {
        const auto bytes = file.read(65537);
        if (bytes.size() <= 65536) {
            for (const auto &entry : QJsonDocument::fromJson(bytes).array()) {
                const QUrl url(entry.toString());
                if (trustedArtworkUrl(url) && url.toString().size() <= 2048 && !urls.contains(url)) urls.append(url);
                if (urls.size() == 40) break;
            }
        }
    }
    m_choices.insert(id, urls);
}
void GameArtworkResolver::rememberChoices(const QString &id, const QList<QUrl> &urls) {
    if (m_choices.size() >= 64 && !m_choices.contains(id)) m_choices.clear();
    m_choices[id] = urls;
    QJsonArray array;
    for (const auto &url : urls) array.append(url.toString());
    const QString path = cachePath(id) + QStringLiteral(".json");
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return;
    QSaveFile file(path);
    const auto bytes = QJsonDocument(array).toJson(QJsonDocument::Compact);
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()) file.commit();
}
int GameArtworkResolver::choiceCount(const QString &title, int steamAppId) {
    const QString id = identifier(title, steamAppId);
    loadChoices(id);
    return int(m_choices.value(id).size());
}
QString GameArtworkResolver::preview(const QString &title, int steamAppId, const QString &apiKey, int index) {
    const QString id = identifier(title, steamAppId);
    loadChoices(id);
    const auto choices = m_choices.value(id);
    if (choices.isEmpty()) { requestChoices(title, steamAppId, apiKey); return {}; }
    const auto at = ((qint64(index) % choices.size()) + choices.size()) % choices.size();
    const QUrl &url = choices[int(at)];
    const QString key = id + '-' + QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex());
    const QString path = cachePath(key);
    if (WallpaperCatalog::isAcceptable(path)) return QUrl::fromLocalFile(path).toString();
    if (at == 0 && WallpaperCatalog::isAcceptable(cachePath(id))) return QUrl::fromLocalFile(cachePath(id)).toString();
    if (!apiKey.isEmpty() && !m_busy && m_retryAt.value(key) <= QDateTime::currentMSecsSinceEpoch()) {
        m_busy = true; m_pending = key;
        m_status = QStringLiteral("Loading SteamGridDB artwork");
        download(key, url);
    }
    return {};
}
int GameArtworkResolver::searchGameId(const QJsonObject &response, const QString &title) {
    if (const int exact = exactGameId(response, title); exact > 0) return exact;
    if (!response.value(QStringLiteral("success")).toBool()) return 0;
    const QString sought = title.trimmed();
    if (canonical(sought).size() < 3) return 0;
    // Accept an optional English "The" in a SteamGridDB result.
    // For longer editions, match a whole title followed by punctuation/space:
    // "0 A.D." -> "0 A.D. Empires Ascendant", never "Doom" -> "Doomsday".
    // Preserve API ranking, but do not use unrelated autocomplete results.
    for (const auto &entry : response.value(QStringLiteral("data")).toArray()) {
        const auto game = entry.toObject();
        const int id = game.value(QStringLiteral("id")).toInt();
        if (id <= 0) continue;
        const QString candidate = game.value(QStringLiteral("name")).toString().trimmed();
        const QString withoutThe = candidate.startsWith(QStringLiteral("The "), Qt::CaseInsensitive)
            ? candidate.mid(4) : candidate;
        if (withoutThe != candidate && canonical(withoutThe) == canonical(sought)) return id;
        if (!withoutThe.startsWith(sought, Qt::CaseInsensitive) || withoutThe.size() <= sought.size()) continue;
        if (!withoutThe.at(sought.size()).isLetterOrNumber()) return id;
    }
    return 0;
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
    const QString alternate = alternateSequel(title);
    if (!alternate.isEmpty()) for (const auto &value : response.value(QStringLiteral("data")).toArray()) {
        const auto game = value.toObject();
        const int id = game.value(QStringLiteral("id")).toInt();
        if (id > 0 && canonical(game.value(QStringLiteral("name")).toString()) == canonical(alternate)) return id;
    }
    return 0; // Never silently use an unrelated autocomplete result.
}
QStringList GameArtworkResolver::searchTitles(const QString &title) {
    const auto name = GameMode::readableTitle(title);
    if (name.size() < 3 || name.size() > 120) return {};
    QStringList names{name};
    // French launcher titles can have an article absent from SteamGridDB.
    // Keep the original for display/caching and search the stripped title only
    // after the exact spelling fails.
    QString withoutArticle = name;
    withoutArticle.remove(QRegularExpression(QStringLiteral("^(?:le|la|les)\\s+"),
        QRegularExpression::CaseInsensitiveOption));
    if (withoutArticle != name && canonical(withoutArticle).size() >= 3)
        names.append(withoutArticle);
    const QStringList bases = names;
    for (const auto &base : bases) {
        const auto alternate = alternateSequel(base);
        if (!alternate.isEmpty() && !names.contains(alternate)) names.append(alternate);
    }
    return names;
}
QList<QUrl> GameArtworkResolver::wideImages(const QJsonObject &response) {
    if (!response.value(QStringLiteral("success")).toBool()) return {};
    QList<QUrl> urls;
    qsizetype urlBytes = 0;
    for (const auto &value : response.value(QStringLiteral("data")).toArray()) {
        const auto image = value.toObject();
        const int width = image.value(QStringLiteral("width")).toInt();
        const int height = image.value(QStringLiteral("height")).toInt();
        if (width < 400 || height < 180 || qint64(width) * height > 32000000
            || double(width) / height < 1.4) continue;
        const QUrl url(image.value(QStringLiteral("url")).toString());
        const auto encodedSize = url.toEncoded().size();
        if (trustedArtworkUrl(url) && encodedSize <= 2048 && !urls.contains(url) && urlBytes + encodedSize < 60000) {
            urls.append(url); urlBytes += encodedSize;
        }
        if (urls.size() == 40) break;
    }
    return urls;
}
QUrl GameArtworkResolver::firstWideImage(const QJsonObject &response) {
    return wideImages(response).value(0);
}
QString GameArtworkResolver::artwork(const QString &title, int steamAppId, const QString &apiKey) {
    if (title.trimmed().isEmpty() && steamAppId <= 0) return {};
    auto selected = selectedArtwork(title, steamAppId);
    if (!selected.isEmpty()) return selected;
    const QString id = identifier(title, steamAppId);
    const QString path = cachePath(id);
    const QFileInfo file(path);
    if (file.isFile() && file.size() > 0 && file.size() <= 32LL * 1024 * 1024) {
        const QImageReader reader(path);
        const QSize size = reader.size();
        if (size.width() > 0 && size.height() > 0 && qint64(size.width()) * size.height() <= 32000000)
            return QUrl::fromLocalFile(path).toString();
    }
    requestChoices(title, steamAppId, apiKey);
    return {};
}
void GameArtworkResolver::requestChoices(const QString &title, int steamAppId, const QString &apiKey) {
    const QString id = identifier(title, steamAppId);
    // A replacement personal key should immediately retry earlier 401/403 errors.
    const QString fingerprint = QString::fromLatin1(
        QCryptographicHash::hash(apiKey.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (m_keyFingerprint != fingerprint) {
        m_keyFingerprint = fingerprint;
        m_retryAt.clear();
    }
    // In-flight requests and failures never interfere with game detection.
    if (apiKey.isEmpty() || m_busy || m_retryAt.value(id) > QDateTime::currentMSecsSinceEpoch()) return;
    m_busy = true;
    m_pending = id;
    m_status = QStringLiteral("Looking for SteamGridDB artwork");
    if (steamAppId > 0) {
        apiGet(apiUrl(QStringLiteral("games/steam/") + QString::number(steamAppId)), apiKey,
            [this, id, title, apiKey](const QJsonObject &response, int httpStatus) {
                const int game = gameId(response);
                if (game > 0) fetchGrid(id, game, apiKey,
                    [this, id, title, apiKey] { search(id, title, apiKey); });
                // A missing Steam mapping can be searched by title, but do not
                // retry denied, rate-limited or offline API requests.
                else if (httpStatus == 200 || httpStatus == 404) search(id, title, apiKey);
                else finish(id, false);
            });
    } else {
        search(id, title, apiKey);
    }
}
void GameArtworkResolver::apiGet(const QUrl &url, const QString &key, std::function<void(QJsonObject, int)> done) {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(10000);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization", QByteArray("Bearer ") + key.toUtf8());
    auto *reply = m_network.get(request);
    connect(reply, &QIODevice::readyRead, reply, [reply] {
        // Bound Qt\'s receive buffer while data arrives, not only after the request ends.
        if (reply->bytesAvailable() > 256LL * 1024) reply->abort();
    });
    const auto generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, done = std::move(done)]() mutable {
        if (generation != m_generation) { reply->deleteLater(); return; }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->read(256LL * 1024 + 1);
        QJsonObject result;
        if (reply->error() == QNetworkReply::NoError && status == 200 && payload.size() <= 256LL * 1024)
            result = QJsonDocument::fromJson(payload).object();
        else if (status == 401 || status == 403)
            m_status = QStringLiteral("SteamGridDB rejected the API key or access");
        else if (status == 429)
            m_status = QStringLiteral("SteamGridDB rate limit reached");
        else
            m_status = QStringLiteral("SteamGridDB offline or unavailable");
        reply->deleteLater();
        done(result, status);
    });
}
void GameArtworkResolver::search(const QString &id, const QString &title, const QString &apiKey, int attempt) {
    const auto names = searchTitles(title);
    if (attempt < 0 || attempt >= names.size()) { finish(id, false); return; }
    const QString name = names[attempt];
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(name));
    apiGet(apiUrl(QStringLiteral("search/autocomplete/") + encoded), apiKey,
        [this, id, title, apiKey, attempt, name](const QJsonObject &response, int status) {
            // Validate against the variant actually sent, not the original title.
            const int game = searchGameId(response, name);
            if (game > 0) fetchGrid(id, game, apiKey,
                [this, id, title, apiKey, attempt] { search(id, title, apiKey, attempt + 1); });
            else if (status == 200 && response.value(QStringLiteral("success")).toBool()
                && attempt + 1 < searchTitles(title).size()) search(id, title, apiKey, attempt + 1);
            else finish(id, false);
        });
}
void GameArtworkResolver::fetchGrid(const QString &id, int gameId, const QString &apiKey,
                                    std::function<void()> noArtwork, bool broad) {
    const QString filter = broad ? QStringLiteral("?types=static")
        : QStringLiteral("?dimensions=920x430,460x215&types=static");
    apiGet(apiUrl(QStringLiteral("grids/game/") + QString::number(gameId) + filter), apiKey,
        [this, id, gameId, apiKey, broad, noArtwork = std::move(noArtwork)](
            const QJsonObject &response, int status) mutable {
            const auto urls = wideImages(response);
            if (!urls.isEmpty()) {
                rememberChoices(id, urls);
                download(id, urls.first());
            } else if (!broad && status == 200 && response.value(QStringLiteral("success")).toBool()) {
                // A game can have wide images in dimensions other than Steam's
                // two standard grids. Try all static grids before the next title.
                fetchGrid(id, gameId, apiKey, std::move(noArtwork), true);
            } else if ((status == 404 || (status == 200 && response.value(QStringLiteral("success")).toBool()))
                       && noArtwork) {
                noArtwork();
            } else {
                finish(id, false);
            }
        });
}
void GameArtworkResolver::download(const QString &id, const QUrl &url) {
    if (!trustedArtworkUrl(url)) { finish(id, false); return; }
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setTransferTimeout(12000);
    auto *reply = m_network.get(request);
    connect(reply, &QIODevice::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 6LL * 1024 * 1024) reply->abort();
    });
    const auto generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, generation] {
        if (generation != m_generation) { reply->deleteLater(); return; }
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->read(6LL * 1024 * 1024 + 1);
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError || http != 200 || bytes.size() > 6LL * 1024 * 1024) {
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
        if (m_retryAt.size() >= 64 && !m_retryAt.contains(id)) m_retryAt.clear();
        m_retryAt[id] = QDateTime::currentMSecsSinceEpoch() + 10LL * 60 * 1000;
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
