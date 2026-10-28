// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GameInstallations.h"
#include "LauncherCache.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMap>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
namespace galleond {
namespace {
int &catalogueRevision() { static int value = 0; return value; }
constexpr qint64 MaxBytes = 1024LL * 1024;
QString clean(QString path) {
    path.replace('\\', '/');
    return QDir::cleanPath(path);
}
bool absoluteLocal(const QString &path) {
    // Accept Windows drive paths in portable parser tests as well as Unix paths.
    return !path.startsWith(QStringLiteral("//")) &&
        (path.startsWith('/') || (path.size() > 3 && path[0].isLetter() && path[1] == ':' && path[2] == '/'));
}
QByteArray read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(MaxBytes + 1) : QByteArray();
}
void append(QList<GameInstallation> &games, const GameInstallation &game) {
    if (!game.directory.isEmpty() && games.size() < 2048) games.append(game);
}
// Read only simple scalar mappings used for executable/title metadata. This is
// intentionally not a YAML evaluator: tags, aliases, multiline values and
// substitutions are not interpreted. Unsupported values leave a manual fallback.
QMap<QString, QString> scalarMappings(const QByteArray &yaml) {
    if (yaml.size() > MaxBytes) return {};
    QMap<QString, QString> result;
    QList<QPair<int, QString>> sections;
    const auto lines = yaml.split('\n');
    if (lines.size() > 4096) return {};
    for (const auto &bytes : lines) {
        if (bytes.size() > 4096 || bytes.contains('\t')) return {};
        const QString line = QString::fromUtf8(bytes);
        const auto colon = line.indexOf(':');
        if (colon < 0 || line.trimmed().startsWith('#')) continue;
        int indent = 0;
        while (indent < line.size() && line[indent] == ' ') ++indent;
        while (!sections.isEmpty() && sections.last().first >= indent) sections.removeLast();
        const QString key = line.mid(indent, colon - indent).trimmed();
        QString value = line.mid(colon + 1).trimmed();
        QString full;
        for (const auto &section : sections) full += section.second + '/';
        full += key;
        if (value.isEmpty()) { sections.append({indent, key}); continue; }
        if (value.startsWith('"')) {
            const auto parsed = QJsonDocument::fromJson(('[' + value + ']').toUtf8()).array();
            if (parsed.size() != 1 || !parsed[0].isString()) continue;
            value = parsed[0].toString();
        } else if (value.startsWith('\'')) {
            if (!value.endsWith('\'') || value.size() < 2) continue;
            value = value.mid(1, value.size() - 2).replace(QStringLiteral("''"), QStringLiteral("'"));
        } else {
            if (QStringLiteral("&*!|>{[").contains(value[0])) continue;
            value = value.section(QStringLiteral(" #"), 0, 0).trimmed();
        }
        result[full] = value;
    }
    return result;
}
QString winePath(QString path, const QString &prefix) {
    path.replace('\\', '/');
    if (path.size() > 3 && path[1] == ':') {
        if (path[0].toLower() == 'c' && !prefix.isEmpty()) return clean(prefix + QStringLiteral("/drive_c/") + path.mid(3));
        if (path[0].toLower() == 'z') return clean('/' + path.mid(3));
        return {};
    }
    return path;
}

}
GameInstallation GameInstallations::registryRecord(const QString &directory, const QString &title) {
    if (directory.size() > 4096 || title.trimmed().isEmpty() || title.size() > 160) return {};
    const QString path = clean(directory);
    if (!absoluteLocal(path) || path == QStringLiteral("/") || path.size() < 4
        || path.contains(QChar::Null) || title.contains(QChar::Null)) return {};
    return {path, title.trimmed(), {}};
}
GameInstallation GameInstallations::epicManifest(const QByteArray &json) {
    if (json.size() > MaxBytes) return {};
    const auto object = QJsonDocument::fromJson(json).object();
    if (object.value(QStringLiteral("bIsIncompleteInstall")).toBool()
        || object.value(QStringLiteral("LaunchExecutable")).toString().isEmpty()
        || object.value(QStringLiteral("AppName")).toString().startsWith(QStringLiteral("UE_"))) return {};
    return registryRecord(object.value(QStringLiteral("InstallLocation")).toString(),
                          object.value(QStringLiteral("DisplayName")).toString());
}
QList<GameInstallation> GameInstallations::legendaryManifest(const QByteArray &json) {
    if (json.size() > MaxBytes) return {};
    QList<GameInstallation> games;
    const auto object = QJsonDocument::fromJson(json).object();
    if (object.size() > 2048) return {};
    for (const auto &entry : object) {
        const auto game = entry.toObject();
        append(games, registryRecord(game.value(QStringLiteral("install_path")).toString(),
                                    game.value(QStringLiteral("title")).toString()));
    }
    return games;
}
QList<GameInstallation> GameInstallations::heroicLibrary(const QByteArray &json) {
    if (json.size() > MaxBytes) return {};
    const auto array = QJsonDocument::fromJson(json).object().value(QStringLiteral("games")).toArray();
    if (array.size() > 2048) return {};
    QList<GameInstallation> games;
    for (const auto &entry : array) {
        const auto item = entry.toObject();
        if (!item.value(QStringLiteral("is_installed")).toBool()) continue;
        const auto install = item.value(QStringLiteral("install")).toObject();
        const QString path = install.value(QStringLiteral("executable")).toString();
        const QString folder = install.value(QStringLiteral("install_path")).toString();
        const QString title = item.value(QStringLiteral("title")).toString();
        if (!folder.isEmpty()) append(games, registryRecord(folder, title));
        else if (absoluteLocal(path)) {
            auto game = registryRecord(clean(path).section('/', 0, -2), title);
            if (!game.directory.isEmpty()) game.executable = clean(path);
            append(games, game);
        }
    }
    return games;
}
GameInstallation GameInstallations::lutrisConfig(const QByteArray &yaml, const QString &fallbackTitle) {
    const auto fields = scalarMappings(yaml);
    const QString executable = winePath(fields.value(QStringLiteral("game/exe")), fields.value(QStringLiteral("game/prefix")));
    if (!absoluteLocal(executable)) return {};
    const QString title = fields.value(QStringLiteral("name"), fallbackTitle);
    auto game = registryRecord(clean(executable).section('/', 0, -2), title);
    if (!game.directory.isEmpty()) game.executable = clean(executable);
    return game;
}
QList<GameInstallation> GameInstallations::bottlesConfig(const QByteArray &yaml, const QString &bottleDirectory) {
    const auto fields = scalarMappings(yaml);
    QList<GameInstallation> games;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        if (!it.key().startsWith(QStringLiteral("External_Programs/")) || !it.key().endsWith(QStringLiteral("/path"))) continue;
        const QString base = it.key().left(it.key().size() - 4);
        const QString executable = winePath(it.value(), bottleDirectory);
        if (!absoluteLocal(executable)) continue;
        auto game = registryRecord(clean(executable).section('/', 0, -2), fields.value(base + QStringLiteral("name")));
        if (!game.directory.isEmpty()) game.executable = clean(executable);
        append(games, game);
    }
    return games;
}
QString GameInstallations::titleForProcess(const QString &process, const QList<GameInstallation> &games) {
    const auto normalize = [](QString name, bool executable) {
        if (executable) {
            name.replace('\\', '/');
            name = name.section('/', -1);
            if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) name.chop(4);
            // Recognized Unreal build suffixes, not arbitrary words in game titles.
            name.remove(QRegularExpression(QStringLiteral("[-_](Win32|Win64|Linux|Linux64)[-_]Shipping$"),
                                           QRegularExpression::CaseInsensitiveOption));
        }
        QString key;
        for (const auto c : name) if (c.isLetterOrNumber()) key += c.toLower();
        return key;
    };
    const QString key = normalize(process, true);
    if (key.size() < 3 || QStringList{QStringLiteral("game"), QStringLiteral("client"), QStringLiteral("main"),
        QStringLiteral("launcher"), QStringLiteral("wine")}.contains(key)) return {};
    QString title;
    for (const auto &game : games) {
        if (key != normalize(game.title, false)
            && (game.executable.isEmpty() || key != normalize(game.executable, true))) continue;
        if (!title.isEmpty() && title != game.title) return {}; // Ambiguous names require a path/profile.
        title = game.title;
    }
    return title;
}
QString GameInstallations::titleForPath(const QString &path, const QList<GameInstallation> &games) {
    const QString normalized = clean(path);
    QString title;
    qsizetype longest = 0;
    for (const auto &game : games) {
        // Match a whole directory, prefer a nested game over its parent directory.
        const auto directory = clean(game.directory);
        const auto sensitivity = directory.size() > 1 && directory[1] == ':' ? Qt::CaseInsensitive : Qt::CaseSensitive;
        if (!game.executable.isEmpty() && normalized.compare(clean(game.executable), sensitivity) != 0) continue;
        if (directory.size() > longest && normalized.startsWith(directory + '/', sensitivity)) {
            longest = directory.size(); title = game.title;
        }
    }
    return title;
}
int GameInstallations::revision() { return catalogueRevision(); }
QList<GameInstallation> GameInstallations::discover() {
    static LauncherCache cache([] {
#ifdef Q_OS_WIN
        const QString name = QStringLiteral("galleond.exe");
#else
        const QString name = QStringLiteral("galleond");
#endif
        const QDir directory(QCoreApplication::applicationDirPath());
        const QString installed = directory.filePath(name);
        return QFileInfo::exists(installed) ? installed : directory.filePath(QStringLiteral("../daemon/") + name);
    }());
    static const auto connection = QObject::connect(&cache, &LauncherCache::ready, &cache,
        [](const QString &) { ++catalogueRevision(); });
    Q_UNUSED(connection)
    static qint64 expires = 0;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now >= expires) { cache.clear(); expires = now + 30000; }
    const auto response = cache.lookup(QStringLiteral("galleon:installed-games"));
    // Keep the previous catalogue while a refresh is in flight, so an active
    // capture is not stopped every thirty seconds for a custom installation.
    static QList<GameInstallation> previous;
    if (!response.contains(QStringLiteral("games"))) return previous;
    QList<GameInstallation> games;
    for (const auto &value : response.value(QStringLiteral("games")).toArray()) {
        const auto object = value.toObject();
        auto game = registryRecord(object.value(QStringLiteral("directory")).toString(), object.value(QStringLiteral("title")).toString());
        game.executable = object.value(QStringLiteral("executable")).toString().left(4096);
        append(games, game);
    }
    previous = games;
    return games;
}
QJsonObject GameInstallations::scanMetadata() {
    QList<GameInstallation> games;
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString home = QDir::homePath();
    const QString roaming = qEnvironmentVariable("APPDATA");
    const QStringList files{config + QStringLiteral("/heroic/legendaryConfig/legendary/installed.json"),
                      config + QStringLiteral("/legendary/installed.json"),
                      home + QStringLiteral("/.var/app/com.heroicgameslauncher.hgl/config/heroic/legendaryConfig/legendary/installed.json"),
                      roaming.isEmpty() ? QString() : roaming + QStringLiteral("/heroic/legendaryConfig/legendary/installed.json")};
#ifdef Q_OS_WIN
    // Epic documents ProgramData manifests; schema also used by Legendary/Playnite.
    // https://dev.epicgames.com/documentation/en-us/unreal-engine/multiple-launcher-unreal-engine-installs
    const QString programData = qEnvironmentVariable("ProgramData");
    if (!programData.isEmpty()) {
        const QDir manifests(programData + QStringLiteral("/Epic/EpicGamesLauncher/Data/Manifests"));
        const auto names = manifests.entryList({QStringLiteral("*.item")}, QDir::Files | QDir::NoSymLinks);
        for (int i = 0; i < qMin(512, int(names.size())); ++i) append(games, epicManifest(read(manifests.filePath(names[i]))));
    }
    // Installation metadata only, both registry views and per-user/machine installs.
    // https://github.com/Gabarsolon/game-relocator (EA Install Dir / GOG PATH)
    // https://github.com/JosefNemec/PlayniteExtensions/blob/master/source/Libraries/GogLibrary/GogLibrary.cs
    for (const auto format : {QSettings::Registry32Format, QSettings::Registry64Format}) {
        for (const auto &hive : {QStringLiteral("HKEY_LOCAL_MACHINE"), QStringLiteral("HKEY_CURRENT_USER")}) {
            for (const auto &vendor : {QStringLiteral("GOG.com\\Games"), QStringLiteral("EA Games"), QStringLiteral("Electronic Arts")}) {
                QSettings registry(hive + QStringLiteral("\\SOFTWARE\\") + vendor, format);
                const auto children = registry.childGroups();
                for (int i = 0; i < qMin(1024, int(children.size())); ++i) {
                    registry.beginGroup(children[i]);
                    const QString path = registry.value(vendor.startsWith(QStringLiteral("GOG")) ? QStringLiteral("path") : QStringLiteral("Install Dir")).toString();
                    const QString title = registry.value(QStringLiteral("gameName"), children[i]).toString();
                    append(games, registryRecord(path, title));
                    registry.endGroup();
                }
            }
            QSettings uninstall(hive + QStringLiteral("\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"), format);
            const auto children = uninstall.childGroups();
            for (int i = 0; i < qMin(2048, int(children.size())); ++i) {
                uninstall.beginGroup(children[i]);
                // GOG installers register games with numeric-id_is1, not the Galaxy client.
                const QString publisher = uninstall.value(QStringLiteral("Publisher")).toString();
                const QString displayName = uninstall.value(QStringLiteral("DisplayName")).toString();
                const bool gog = publisher == QStringLiteral("GOG.com") && children[i].endsWith(QStringLiteral("_is1"));
                const bool ea = publisher.startsWith(QStringLiteral("Electronic Arts"), Qt::CaseInsensitive)
                    && displayName.compare(QStringLiteral("EA app"), Qt::CaseInsensitive) != 0
                    && displayName.compare(QStringLiteral("EA Desktop"), Qt::CaseInsensitive) != 0
                    && displayName.compare(QStringLiteral("Origin"), Qt::CaseInsensitive) != 0;
                if (gog || ea)
                    append(games, registryRecord(uninstall.value(QStringLiteral("InstallLocation")).toString(),
                                                uninstall.value(QStringLiteral("DisplayName")).toString()));
                uninstall.endGroup();
            }
        }
    }
#endif
    for (const auto &file : files) {
        for (const auto &game : legendaryManifest(read(file))) append(games, game);
    }
    const QStringList heroicFolders{config + QStringLiteral("/heroic"),
        home + QStringLiteral("/.var/app/com.heroicgameslauncher.hgl/config/heroic"),
        roaming.isEmpty() ? QString() : roaming + QStringLiteral("/heroic")};
    for (const auto &folder : heroicFolders) {
        if (folder.isEmpty()) continue;
        for (const auto &name : {QStringLiteral("/sideload_apps/library.json"), QStringLiteral("/gog_store/library.json")})
            for (const auto &game : heroicLibrary(read(folder + name))) append(games, game);
    }
#ifndef Q_OS_WIN
    // Lutris persisted game configuration; no scripts or shell expansions are run.
    // https://github.com/lutris/lutris/blob/master/docs/installers.rst
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QStringList lutrisFolders{config + QStringLiteral("/lutris/games"), data + QStringLiteral("/lutris/games"),
        home + QStringLiteral("/.var/app/net.lutris.Lutris/config/lutris/games")};
    for (const auto &folder : lutrisFolders) {
        const QDir directory(folder);
        const auto names = directory.entryList({QStringLiteral("*.yml"), QStringLiteral("*.yaml")}, QDir::Files | QDir::NoSymLinks);
        for (int i = 0; i < qMin(256, int(names.size())); ++i) {
            QString fallback = QFileInfo(names[i]).completeBaseName();
            fallback.remove(QRegularExpression(QStringLiteral("-[0-9]+$")));
            fallback.replace('-', ' ');
            append(games, lutrisConfig(read(directory.filePath(names[i])), fallback));
        }
    }
    // Bottles user-defined programs are stored as External_Programs in bottle.yml.
    // https://github.com/bottlesdevs/Bottles/blob/main/bottles/backend/managers/manager.py
    const QStringList bottlesFolders{data + QStringLiteral("/bottles/bottles"),
        home + QStringLiteral("/.var/app/com.usebottles.bottles/data/bottles/bottles")};
    for (const auto &folder : bottlesFolders) {
        const QDir directory(folder);
        const auto names = directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        for (int i = 0; i < qMin(128, int(names.size())); ++i) {
            const QString bottle = directory.filePath(names[i]);
            for (const auto &game : bottlesConfig(read(bottle + QStringLiteral("/bottle.yml")), bottle)) append(games, game);
        }
    }
#endif
    QJsonArray entries;
    qsizetype total = 0;
    for (const auto &game : games) {
        const QJsonObject object{{QStringLiteral("directory"), game.directory}, {QStringLiteral("title"), game.title},
                                 {QStringLiteral("executable"), game.executable}};
        total += QJsonDocument(object).toJson(QJsonDocument::Compact).size() + 1;
        if (total > 60000) break; // LauncherCache caps the child's complete output at 64 KiB.
        entries.append(object);
    }
    return {{QStringLiteral("games"), entries}};
}
}
