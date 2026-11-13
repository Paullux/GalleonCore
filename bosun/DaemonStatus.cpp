// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DaemonStatus.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusConnectionInterface>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace bosun {

DaemonStatus::DaemonStatus(QString systemctl, QObject *parent)
    : QObject(parent)
    , m_systemctl(systemctl.isEmpty() ? QStringLiteral("systemctl") : std::move(systemctl))
    , m_watcher(QString::fromLatin1(BusName), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForOwnerChange)
{
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] { look(); });
    auto bus = QDBusConnection::sessionBus();
    const QString keyboardPath = QStringLiteral("/org/galleoncore/Keyboard");
    const QString keyboardInterface = QStringLiteral("org.galleoncore.Keyboard");
    bus.connect(QString::fromLatin1(BusName), keyboardPath, keyboardInterface, QStringLiteral("brightnessRead"), this, SLOT(onBrightnessRead(int,bool,QString)));
    bus.connect(QString::fromLatin1(BusName), keyboardPath, keyboardInterface, QStringLiteral("brightnessWritten"), this, SLOT(onBrightnessWritten(int,bool,QString)));
    bus.connect(QString::fromLatin1(BusName), QStringLiteral("/org/galleoncore/Lighting"), QStringLiteral("org.galleoncore.Lighting"), QStringLiteral("stateChanged"), this, SLOT(onLightingStateChanged(QString,QString)));
    look();
}

int DaemonStatus::nearestBrightnessStop(int value)
{
    if (value < 0 || value > 1000)
        return -1;
    return (value + 50) / 100 * 100;
}

void DaemonStatus::setKeyboardBrightnessKnown(int value)
{
    if (value == m_keyboardBrightness)
        return;
    m_keyboardBrightness = value;
    Q_EMIT keyboardBrightnessChanged();
}

void DaemonStatus::onBrightnessRead(int value, bool ok, const QString & /*reason*/)
{
    // A failed read leaves what was known: the slider is never moved by a guess.
    if (ok)
        setKeyboardBrightnessKnown(nearestBrightnessStop(value));
}

void DaemonStatus::onBrightnessWritten(int value, bool ok, const QString & /*reason*/)
{
    if (ok)
        setKeyboardBrightnessKnown(nearestBrightnessStop(value));
}

bool DaemonStatus::lightingSupported()
{
    return true;
}

void DaemonStatus::setLighting(const QString &state, const QString &detail)
{
    if (state == m_lightingState && detail == m_lightingDetail)
        return;
    m_lightingState = state;
    m_lightingDetail = detail;
    Q_EMIT lightingChanged();
}

void DaemonStatus::onLightingStateChanged(const QString &state, const QString &detail)
{
    setLighting(state, detail);
}

namespace {
QDBusMessage lightingCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QString::fromLatin1(DaemonStatus::BusName), QStringLiteral("/org/galleoncore/Lighting"), QStringLiteral("org.galleoncore.Lighting"), method);
}
} // namespace

QString DaemonStatus::enableLighting(bool on)
{
    if (!running()) return tr("Start galleond to switch the lighting on or off");
    auto message = lightingCall(QStringLiteral("Enable"));
    message << on;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 8000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, on] {
        const QDBusPendingReply<QString> reply = *watcher;
        if (reply.isError())
            setLighting(QStringLiteral("off"), tr("The lighting could not be reached: restart the updated daemon (%1)").arg(reply.error().message()));
        else if (!reply.value().isEmpty())
            setLighting(QStringLiteral("off"), reply.value()); // refused: the reason is the daemon's own
        else if (!on)
            setLighting(QStringLiteral("off"), {});
        watcher->deleteLater();
    });
    return {};
}

QString DaemonStatus::setLightingColor(int red, int green, int blue)
{
    if (!running()) return tr("Start galleond to change the lighting");
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
        return tr("A colour has three values from 0 to 255");
    auto message = lightingCall(QStringLiteral("SetColor"));
    message << red << green << blue;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher] {
        watcher->deleteLater();
    });
    return {};
}

QString DaemonStatus::refreshLighting()
{
    if (!running()) return {};
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(lightingCall(QStringLiteral("State")), 1500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<QString> reply = *watcher;
        // A daemon without the lighting object never answers: that is "off", not an error to show.
        if (!reply.isError())
            setLighting(reply.value(), {});
        watcher->deleteLater();
    });
    return {};
}

QString DaemonStatus::readKeyboardBrightness()
{
    if (!running()) return {};
    // The answer is the brightnessRead signal; a daemon without this method just never answers, which is not an error to show.
    const auto message = QDBusMessage::createMethodCall(QString::fromLatin1(BusName), QStringLiteral("/org/galleoncore/Keyboard"), QStringLiteral("org.galleoncore.Keyboard"), QStringLiteral("ReadBrightness"));
    QDBusConnection::sessionBus().asyncCall(message, 1500);
    return {};
}

void DaemonStatus::look()
{
    const QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    const bool running = bus && bus->isServiceRegistered(QString::fromLatin1(BusName));
    if (running == m_running)
        return;
    m_running = running;
    Q_EMIT changed();
    if (m_running) {
        static_cast<void>(readKeyboardBrightness());
        static_cast<void>(refreshLighting());
    } else {
        setLighting(QStringLiteral("off"), {}); // no daemon, no lighting: whatever it held was given back or the watchdog did
    }
}

QString DaemonStatus::showGaming()
{
    if (!running()) return tr("Start the daemon to display Gaming on the pad");
    const auto message = QDBusMessage::createMethodCall(QString::fromLatin1(BusName), QStringLiteral("/org/galleoncore/Deck"), QString::fromLatin1(BusName), QStringLiteral("ShowGaming"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<bool> reply = *watcher;
        if (reply.isError()) Q_EMIT commandError(tr("Cannot show Gaming: restart the updated daemon (%1)").arg(reply.error().message()));
        else if (!reply.value()) Q_EMIT commandError(tr("Gaming could not be shown: check the pad connection"));
        watcher->deleteLater();
    });
    return {};
}

QString DaemonStatus::showTheme(const QString &page)
{
    if (!running()) return {}; // the pad shows the pages file when the daemon starts
    auto message = QDBusMessage::createMethodCall(QString::fromLatin1(BusName), QStringLiteral("/org/galleoncore/Deck"), QString::fromLatin1(BusName), QStringLiteral("ShowTheme"));
    message << page;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<bool> reply = *watcher;
        // False is normal when the daemon has not read the new pages file yet: it keeps the request and does it then.
        if (reply.isError()) Q_EMIT commandError(tr("Cannot show the theme on the pad: restart the updated daemon (%1)").arg(reply.error().message()));
        watcher->deleteLater();
    });
    return {};
}

QString DaemonStatus::setKeyboardBrightness(int value)
{
    if (!running()) return tr("Start galleond to change the keyboard brightness");
    auto message = QDBusMessage::createMethodCall(QString::fromLatin1(BusName), QStringLiteral("/org/galleoncore/Keyboard"), QStringLiteral("org.galleoncore.Keyboard"), QStringLiteral("SetBrightness"));
    message << value;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1500), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        const QDBusPendingReply<bool> reply = *watcher;
        if (reply.isError()) Q_EMIT commandError(tr("Cannot set the keyboard brightness: restart the updated daemon (%1)").arg(reply.error().message()));
        else if (!reply.value()) Q_EMIT commandError(tr("The keyboard brightness was refused (steps of 10 % from 0 to 100)"));
        watcher->deleteLater();
    });
    return {};
}

QStringList DaemonStatus::userCommand(const QString &verb)
{
    return {QStringLiteral("--user"), verb, QString::fromLatin1(ServiceName)};
}

QString DaemonStatus::ask(const QString &verb)
{
    const QString program = QFileInfo(m_systemctl).isAbsolute() ? m_systemctl : QStandardPaths::findExecutable(m_systemctl);
    if (program.isEmpty())
        return QCoreApplication::translate("DaemonStatus", "systemctl was not found: start galleond by hand.");
    // The request goes out and is forgotten: whether it worked shows when the name appears on the bus.
    if (!QProcess::startDetached(program, userCommand(verb)))
        return QCoreApplication::translate("DaemonStatus", "systemctl could not be started.");
    return {};
}

QString DaemonStatus::start()
{
    return running() ? QString() : ask(QStringLiteral("start"));
}

QString DaemonStatus::stop()
{
    return running() ? ask(QStringLiteral("stop")) : QString();
}

} // namespace bosun
