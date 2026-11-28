// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LocalControl.h"
#include "ProfileNames.h"

#include <QRegularExpression>

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <QLocalSocket>
#include <QTimer>
#include <QStandardPaths>
#include <QVariant>
#ifdef Q_OS_WIN
#include <Windows.h>
#include <sddl.h>
#include <QScopeGuard>
#include <vector>
#include <cstddef>
#endif

namespace galleond {
namespace {
constexpr qint64 MaxFrame = 4096;
constexpr int DeadlineMs = 1500;
constexpr int MaxConnections = 4;

// One channel of a colour: a JSON number that is a whole number from 0 to 255 (not 1.5, not "1", not 256).
std::optional<int> colourChannel(const QJsonObject &object, const QString &name)
{
    const QJsonValue value = object.value(name);
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (number < 0.0 || number > 255.0 || number != static_cast<int>(number))
        return std::nullopt;
    return static_cast<int>(number);
}

// The index of one LED of the live frame: a whole number from 0 to 145. 146 is galleon::keyboard::LiveLedCount; this file is in
// galleon_platform, shared with Bosun, which does not link the keyboard library, so the number is written here and the
// controller checks it again (singleLedScene) before anything is lit.
constexpr double LedCount = 146.0;
std::optional<int> ledIndex(const QJsonObject &object, const QString &name)
{
    const QJsonValue value = object.value(name);
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (number < 0.0 || number >= LedCount || number != static_cast<int>(number))
        return std::nullopt;
    return static_cast<int>(number);
}

// A short text of plain printable ASCII (0x20 to 0x7E), 1 to `longest` characters: a program's name, a key's name.
std::optional<QString> plainText(const QJsonObject &object, const QString &name, int longest)
{
    const QJsonValue value = object.value(name);
    if (!value.isString())
        return std::nullopt;
    QString text = value.toString();
    if (text.isEmpty() || text.size() > longest)
        return std::nullopt;
    for (const QChar c : text) {
        if (c.unicode() < 0x20 || c.unicode() > 0x7E)
            return std::nullopt;
    }
    return text;
}

constexpr int LongestSignalClient = 64; // as KeySignals.h: MaxClientNameBytes
constexpr int LongestKeyName = 32;
constexpr double LongestSignalSeconds = 24.0 * 60 * 60;

constexpr int LongestVideoName = 1024; // as the pages file allows for a path

// A whole number in [low, high] from a JSON number (not 1.5, not "1").
std::optional<int> boundedWhole(const QJsonObject &object, const QString &name, int low, int high)
{
    const QJsonValue value = object.value(name);
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (number < low || number > high || number != static_cast<int>(number))
        return std::nullopt;
    return static_cast<int>(number);
}

// The video of a screensaver as the list names it: a text of 1 to 1024 characters with no control character (a path may hold any other).
std::optional<QString> videoName(const QJsonObject &object)
{
    const QJsonValue value = object.value(QStringLiteral("video"));
    if (!value.isString())
        return std::nullopt;
    QString text = value.toString();
    if (text.isEmpty() || text.size() > LongestVideoName)
        return std::nullopt;
    for (const QChar c : text) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7F)
            return std::nullopt;
    }
    return text;
}

std::optional<LocalCommand> parseKeySignal(const QJsonObject &object, const QString &action)
{
    const auto client = plainText(object, QStringLiteral("client"), LongestSignalClient);
    if (!client)
        return std::nullopt;
    LocalCommand command{LocalAction::KeySignalClearAll, {}, {}};
    command.client = *client;
    if (object.size() == 2 && action == QStringLiteral("key-signal-clear-all"))
        return command;
    const auto key = plainText(object, QStringLiteral("key"), LongestKeyName);
    if (!key)
        return std::nullopt;
    command.key = *key;
    if (object.size() == 3 && action == QStringLiteral("key-signal-clear")) {
        command.action = LocalAction::KeySignalClear;
        return command;
    }
    if (object.size() != 8 || action != QStringLiteral("key-signal"))
        return std::nullopt;
    const auto red = colourChannel(object, QStringLiteral("red"));
    const auto green = colourChannel(object, QStringLiteral("green"));
    const auto blue = colourChannel(object, QStringLiteral("blue"));
    const QJsonValue effect = object.value(QStringLiteral("effect"));
    const QJsonValue seconds = object.value(QStringLiteral("seconds"));
    if (!red || !green || !blue || !effect.isString() || !seconds.isDouble())
        return std::nullopt;
    if (effect.toString() != QStringLiteral("steady") && effect.toString() != QStringLiteral("pulse"))
        return std::nullopt;
    const double length = seconds.toDouble();
    if (!(length >= 1.0) || length > LongestSignalSeconds) // also refuses NaN
        return std::nullopt;
    command.action = LocalAction::KeySignal;
    command.red = *red;
    command.green = *green;
    command.blue = *blue;
    command.effect = effect.toString();
    command.seconds = length;
    return command;
}
}

std::optional<LocalCommand> parseLocalCommand(const QByteArray &frame)
{
    if (frame.isEmpty() || frame.size() > MaxFrame)
        return std::nullopt;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(frame, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject object = document.object();
    const QString action = object.value(QStringLiteral("action")).toString();
    if (object.size() == 1 && action == QStringLiteral("ping"))
        return LocalCommand{LocalAction::Ping, {}, {}};
    if (object.size() == 1 && action == QStringLiteral("gaming"))
        return LocalCommand{LocalAction::Gaming, {}, {}};
    if (object.size() == 2 && action == QStringLiteral("theme")) {
        const QJsonValue page = object.value(QStringLiteral("page"));
        // A page name, as a pages file allows; the controller finds the theme, nothing else is done with it.
        static const QRegularExpression pattern(QStringLiteral("^[a-z0-9_-]{1,32}$"));
        if (!page.isString() || !pattern.match(page.toString()).hasMatch())
            return std::nullopt;
        return LocalCommand{LocalAction::Theme, {}, page.toString()};
    }
    if (object.size() == 2 && action == QStringLiteral("profile")) {
        const QJsonValue name = object.value(QStringLiteral("name"));
        if (!name.isString() || !isValidProfileName(name.toString()))
            return std::nullopt;
        LocalCommand command;
        command.action = LocalAction::Profile;
        command.profile = name.toString();
        return command;
    }
    if (object.size() == 2 && action == QStringLiteral("profile-step")) {
        // A whole number from -16 to 16, not 0 (a JSON 1.5 or "1" is refused).
        const QJsonValue steps = object.value(QStringLiteral("steps"));
        if (!steps.isDouble())
            return std::nullopt;
        const double number = steps.toDouble();
        if (number != std::floor(number) || number < -MaxProfiles || number > MaxProfiles || number == 0)
            return std::nullopt;
        LocalCommand command;
        command.action = LocalAction::ProfileStep;
        command.steps = static_cast<int>(number);
        return command;
    }
    if (object.size() == 2 && action == QStringLiteral("keyboard-brightness")) {
        // A whole number from 0 to 1000 (a JSON 3.5 or "3" is refused); the keyboard component applies its own policy.
        const QJsonValue value = object.value(QStringLiteral("value"));
        if (!value.isDouble())
            return std::nullopt;
        const double number = value.toDouble();
        if (number < 0.0 || number > 1000.0 || number != static_cast<int>(number))
            return std::nullopt;
        return LocalCommand{LocalAction::KeyboardBrightness, {}, {}, static_cast<int>(number)};
    }
    // Asks galleond to read the keyboard's brightness and publish it (keyboard-state.json): there is no reply here.
    if (object.size() == 1 && action == QStringLiteral("keyboard-brightness-read"))
        return LocalCommand{LocalAction::KeyboardBrightnessRead, {}, {}};
    // The live lighting (experimental, off until asked): there is no reply here, galleond publishes its state in
    // lighting-state.json. `on` must be a JSON true or false, and a colour three whole numbers from 0 to 255.
    if (object.size() == 2 && action == QStringLiteral("lighting-enable")) {
        const QJsonValue on = object.value(QStringLiteral("on"));
        if (!on.isBool())
            return std::nullopt;
        LocalCommand command{LocalAction::LightingEnable, {}, {}};
        command.on = on.toBool();
        return command;
    }
    if (object.size() == 4 && action == QStringLiteral("lighting-color")) {
        const auto red = colourChannel(object, QStringLiteral("red"));
        const auto green = colourChannel(object, QStringLiteral("green"));
        const auto blue = colourChannel(object, QStringLiteral("blue"));
        if (!red || !green || !blue)
            return std::nullopt;
        LocalCommand command{LocalAction::LightingColor, {}, {}};
        command.red = *red;
        command.green = *green;
        command.blue = *blue;
        return command;
    }
    if (object.size() == 5 && action == QStringLiteral("lighting-led")) {
        const auto led = ledIndex(object, QStringLiteral("led"));
        const auto red = colourChannel(object, QStringLiteral("red"));
        const auto green = colourChannel(object, QStringLiteral("green"));
        const auto blue = colourChannel(object, QStringLiteral("blue"));
        if (!led || !red || !green || !blue)
            return std::nullopt;
        LocalCommand command{LocalAction::LightingLed, {}, {}};
        command.led = *led;
        command.red = *red;
        command.green = *green;
        command.blue = *blue;
        return command;
    }
    if (object.size() == 2 && action == QStringLiteral("lighting-leds")) {
        const QJsonValue list = object.value(QStringLiteral("leds"));
        if (!list.isArray())
            return std::nullopt;
        const QJsonArray groups = list.toArray();
        if (groups.isEmpty() || groups.size() > static_cast<qsizetype>(LedCount))
            return std::nullopt;
        LocalCommand command{LocalAction::LightingLeds, {}, {}};
        for (const auto &group : groups) {
            // Each group is exactly [led, red, green, blue], whole numbers in their ranges.
            const QJsonArray numbers = group.isArray() ? group.toArray() : QJsonArray();
            if (numbers.size() != 4)
                return std::nullopt;
            QJsonObject asObject;
            asObject.insert(QStringLiteral("led"), numbers.at(0));
            asObject.insert(QStringLiteral("red"), numbers.at(1));
            asObject.insert(QStringLiteral("green"), numbers.at(2));
            asObject.insert(QStringLiteral("blue"), numbers.at(3));
            const auto led = ledIndex(asObject, QStringLiteral("led"));
            const auto red = colourChannel(asObject, QStringLiteral("red"));
            const auto green = colourChannel(asObject, QStringLiteral("green"));
            const auto blue = colourChannel(asObject, QStringLiteral("blue"));
            if (!led || !red || !green || !blue)
                return std::nullopt;
            command.ledValues << *led << *red << *green << *blue;
        }
        return command;
    }
    if (action == QStringLiteral("key-signal") || action == QStringLiteral("key-signal-clear") || action == QStringLiteral("key-signal-clear-all"))
        return parseKeySignal(object, action);
    if (object.size() == 3 && action == QStringLiteral("night-start")) {
        const auto hour = boundedWhole(object, QStringLiteral("hour"), 0, 23);
        const auto minute = boundedWhole(object, QStringLiteral("minute"), 0, 59);
        if (!hour || !minute)
            return std::nullopt;
        LocalCommand command{LocalAction::NightStart, {}, {}};
        command.hour = *hour;
        command.minute = *minute;
        return command;
    }
    if (object.size() == 1 && action == QStringLiteral("night-end"))
        return LocalCommand{LocalAction::NightEnd, {}, {}};
    if (object.size() == 2 && action == QStringLiteral("screensaver-test")) {
        const auto video = videoName(object);
        if (!video)
            return std::nullopt;
        LocalCommand command{LocalAction::ScreensaverTest, {}, {}};
        command.video = *video;
        return command;
    }
    if (object.size() == 1 && action == QStringLiteral("stop"))
        return LocalCommand{LocalAction::Stop, {}, {}};
    if (object.size() != 2 || action != QStringLiteral("wallpaper"))
        return std::nullopt;
    const QJsonValue value = object.value(QStringLiteral("name"));
    if (!value.isString())
        return std::nullopt;
    const QString name = value.toString();
    // The catalog performs the final lookup. No path or process is executed here.
    if (name.isEmpty() || name.size() > 1024 || name.contains(QChar::Null))
        return std::nullopt;
    return LocalCommand{LocalAction::Wallpaper, name, {}};
}

QByteArray encodeLocalCommand(const LocalCommand &command)
{
    QJsonObject object;
    switch (command.action) {
    case LocalAction::Ping: object.insert(QStringLiteral("action"), QStringLiteral("ping")); break;
    case LocalAction::Gaming: object.insert(QStringLiteral("action"), QStringLiteral("gaming")); break;
    case LocalAction::Theme:
        object.insert(QStringLiteral("action"), QStringLiteral("theme"));
        object.insert(QStringLiteral("page"), command.page);
        break;
    case LocalAction::Profile:
        object.insert(QStringLiteral("action"), QStringLiteral("profile"));
        object.insert(QStringLiteral("name"), command.profile);
        break;
    case LocalAction::ProfileStep:
        object.insert(QStringLiteral("action"), QStringLiteral("profile-step"));
        object.insert(QStringLiteral("steps"), command.steps);
        break;
    case LocalAction::KeyboardBrightness:
        object.insert(QStringLiteral("action"), QStringLiteral("keyboard-brightness"));
        object.insert(QStringLiteral("value"), command.brightness);
        break;
    case LocalAction::KeyboardBrightnessRead: object.insert(QStringLiteral("action"), QStringLiteral("keyboard-brightness-read")); break;
    case LocalAction::LightingEnable:
        object.insert(QStringLiteral("action"), QStringLiteral("lighting-enable"));
        object.insert(QStringLiteral("on"), command.on);
        break;
    case LocalAction::LightingColor:
        object.insert(QStringLiteral("action"), QStringLiteral("lighting-color"));
        object.insert(QStringLiteral("red"), command.red);
        object.insert(QStringLiteral("green"), command.green);
        object.insert(QStringLiteral("blue"), command.blue);
        break;
    case LocalAction::LightingLed:
        object.insert(QStringLiteral("action"), QStringLiteral("lighting-led"));
        object.insert(QStringLiteral("led"), command.led);
        object.insert(QStringLiteral("red"), command.red);
        object.insert(QStringLiteral("green"), command.green);
        object.insert(QStringLiteral("blue"), command.blue);
        break;
    case LocalAction::LightingLeds: {
        object.insert(QStringLiteral("action"), QStringLiteral("lighting-leds"));
        QJsonArray groups;
        for (qsizetype i = 0; i + 3 < command.ledValues.size(); i += 4)
            groups.append(QJsonArray{command.ledValues.at(i), command.ledValues.at(i + 1), command.ledValues.at(i + 2), command.ledValues.at(i + 3)});
        object.insert(QStringLiteral("leds"), groups);
        break;
    }
    case LocalAction::KeySignal:
        object.insert(QStringLiteral("action"), QStringLiteral("key-signal"));
        object.insert(QStringLiteral("client"), command.client);
        object.insert(QStringLiteral("key"), command.key);
        object.insert(QStringLiteral("red"), command.red);
        object.insert(QStringLiteral("green"), command.green);
        object.insert(QStringLiteral("blue"), command.blue);
        object.insert(QStringLiteral("effect"), command.effect);
        object.insert(QStringLiteral("seconds"), command.seconds);
        break;
    case LocalAction::KeySignalClear:
        object.insert(QStringLiteral("action"), QStringLiteral("key-signal-clear"));
        object.insert(QStringLiteral("client"), command.client);
        object.insert(QStringLiteral("key"), command.key);
        break;
    case LocalAction::KeySignalClearAll:
        object.insert(QStringLiteral("action"), QStringLiteral("key-signal-clear-all"));
        object.insert(QStringLiteral("client"), command.client);
        break;
    case LocalAction::NightStart:
        object.insert(QStringLiteral("action"), QStringLiteral("night-start"));
        object.insert(QStringLiteral("hour"), command.hour);
        object.insert(QStringLiteral("minute"), command.minute);
        break;
    case LocalAction::NightEnd: object.insert(QStringLiteral("action"), QStringLiteral("night-end")); break;
    case LocalAction::ScreensaverTest:
        object.insert(QStringLiteral("action"), QStringLiteral("screensaver-test"));
        object.insert(QStringLiteral("video"), command.video);
        break;
    case LocalAction::Stop: object.insert(QStringLiteral("action"), QStringLiteral("stop")); break;
    case LocalAction::Wallpaper:
        object.insert(QStringLiteral("action"), QStringLiteral("wallpaper"));
        object.insert(QStringLiteral("name"), command.wallpaper);
        break;
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

QString localControlName()
{
#ifdef Q_OS_WIN
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return {};
    const auto closeToken = qScopeGuard([token] { CloseHandle(token); });
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    if (size == 0 || size > 65536)
        return {};
    std::vector<std::byte> storage(size);
    if (!GetTokenInformation(token, TokenUser, storage.data(), size, &size))
        return {};
    const auto *user = reinterpret_cast<const TOKEN_USER *>(storage.data());
    LPWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sid))
        return {};
    const auto freeSid = qScopeGuard([sid] { LocalFree(sid); });
    const QByteArray identity = QString::fromWCharArray(sid).toUtf8();
#else
    const QByteArray identity = QDir::homePath().toUtf8();
#endif
    const QByteArray digest = QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(24);
    return QStringLiteral("galleoncore-") + QString::fromLatin1(digest);
}

LocalControl::LocalControl(QObject *parent) : QObject(parent)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.setMaxPendingConnections(MaxConnections);
    connect(&m_server, &QLocalServer::newConnection, this, &LocalControl::acceptConnections);
}

bool LocalControl::listen(const QString &name)
{
    if (name.isEmpty() || m_server.isListening()) {
        m_error = QStringLiteral("Invalid endpoint or server already listening");
        return false;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/galleoncore");
    if (!QDir().mkpath(folder)) {
        m_error = QStringLiteral("Cannot create the local control lock directory");
        return false;
    }
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256).toHex());
    m_lock = std::make_unique<QLockFile>(folder + QStringLiteral("/ipc-") + digest + QStringLiteral(".lock"));
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(0)) {
        m_error = QStringLiteral("Another instance owns the local control endpoint");
        return false;
    }
    if (!m_server.listen(name)) {
        m_error = m_server.errorString();
        m_lock.reset();
        return false;
    }
    m_error.clear();
    return true;
}

QString LocalControl::errorString() const
{
    return m_error;
}

void LocalControl::acceptConnections()
{
    while (m_server.hasPendingConnections()) {
        QLocalSocket *const socket = m_server.nextPendingConnection();
        if (!socket)
            continue;
        socket->setParent(this);
        if (m_connections >= MaxConnections) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        ++m_connections;
        socket->setReadBufferSize(MaxFrame + 1);
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QObject::destroyed, this, [this] { --m_connections; });
        QTimer::singleShot(DeadlineMs, socket, [socket] {
            socket->abort();
            socket->deleteLater();
        });
        const auto readFrame = [this, socket] {
            if (socket->property("handled").toBool())
                return;
            if (socket->bytesAvailable() > MaxFrame) {
                socket->abort();
                return;
            }
            if (!socket->canReadLine())
                return;
            const auto command = parseLocalCommand(socket->readLine(MaxFrame + 1));
            if (!command) {
                socket->abort();
                return;
            }
            // Consume a single request, disconnect before any callback can
            // enter a nested event loop and accidentally process it twice.
            socket->setProperty("handled", true);
            socket->write("{\"ok\":true}\n");
            socket->disconnectFromServer();
            switch (command->action) {
            case LocalAction::Gaming: Q_EMIT gamingRequested(); break;
            case LocalAction::Ping: break;
            case LocalAction::Wallpaper: Q_EMIT wallpaperRequested(command->wallpaper); break;
            case LocalAction::Theme: Q_EMIT themeRequested(command->page); break;
            case LocalAction::Profile: Q_EMIT profileRequested(command->profile); break;
            case LocalAction::ProfileStep: Q_EMIT profileStepRequested(command->steps); break;
            case LocalAction::KeyboardBrightness: Q_EMIT keyboardBrightnessRequested(command->brightness); break;
            case LocalAction::KeyboardBrightnessRead: Q_EMIT keyboardBrightnessReadRequested(); break;
            case LocalAction::LightingEnable: Q_EMIT lightingEnableRequested(command->on); break;
            case LocalAction::LightingColor: Q_EMIT lightingColorRequested(command->red, command->green, command->blue); break;
            case LocalAction::LightingLed: Q_EMIT lightingLedRequested(command->led, command->red, command->green, command->blue); break;
            case LocalAction::LightingLeds: Q_EMIT lightingLedsRequested(command->ledValues); break;
            case LocalAction::KeySignal:
                Q_EMIT keySignalRequested(command->client, command->key, command->red, command->green, command->blue, command->effect, command->seconds);
                break;
            case LocalAction::KeySignalClear: Q_EMIT keySignalClearRequested(command->client, command->key); break;
            case LocalAction::KeySignalClearAll: Q_EMIT keySignalClearAllRequested(command->client); break;
            case LocalAction::NightStart: Q_EMIT nightStartRequested(command->hour, command->minute); break;
            case LocalAction::NightEnd: Q_EMIT nightEndRequested(); break;
            case LocalAction::ScreensaverTest: Q_EMIT screensaverTestRequested(command->video); break;
            case LocalAction::Stop: Q_EMIT stopRequested(); break;
            }
        };
        connect(socket, &QLocalSocket::readyRead, socket, readFrame);
        readFrame(); // Data may already be buffered when the connection is accepted.
    }
}

void sendLocalCommand(const QString &name, const LocalCommand &command, QObject *owner)
{
    auto *socket = new QLocalSocket(owner);
    const QByteArray frame = encodeLocalCommand(command);
    QObject::connect(socket, &QLocalSocket::connected, socket, [socket, frame] {
        socket->write(frame);
        socket->disconnectFromServer();
    });
    QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    QObject::connect(socket, &QLocalSocket::errorOccurred, socket, &QObject::deleteLater);
    QTimer::singleShot(DeadlineMs, socket, [socket] { socket->abort(); socket->deleteLater(); });
    socket->connectToServer(name);
}
} // namespace galleond
