// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LogindPowerEvents.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

namespace galleond {

namespace {
// Functions, not constants: a QString of static storage duration may throw before main.
QString service() { return QStringLiteral("org.freedesktop.login1"); }
QString path() { return QStringLiteral("/org/freedesktop/login1"); }
QString interfaceName() { return QStringLiteral("org.freedesktop.login1.Manager"); }
} // namespace

LogindPowerEvents::LogindPowerEvents(QObject *parent) : PowerEvents(parent)
{
    QDBusConnection::systemBus().connect(service(), path(), interfaceName(), QStringLiteral("PrepareForSleep"), this, SLOT(onPrepareForSleepSignal(bool)));
}

bool LogindPowerEvents::available(QString *why) const
{
    const QDBusConnection bus = QDBusConnection::systemBus();
    const QDBusConnectionInterface *interface = bus.interface();
    if (!bus.isConnected() || !interface || !interface->isServiceRegistered(service())) {
        *why = tr("This system does not tell galleond about sleep (no systemd-logind): the lighting stays off, because the keys could not be given back before sleeping.");
        return false;
    }
    return true;
}

void LogindPowerEvents::holdSleep()
{
    if (m_lock.isValid())
        return;
    QDBusInterface manager(service(), path(), interfaceName(), QDBusConnection::systemBus());
    const QDBusReply<QDBusUnixFileDescriptor> reply = manager.call(QStringLiteral("Inhibit"), QStringLiteral("sleep"), QStringLiteral("GalleonCore"),
                                                                  QStringLiteral("Give the keyboard lighting back before the machine sleeps"), QStringLiteral("delay"));
    if (reply.isValid())
        m_lock = reply.value();
    else
        qWarning() << "galleond: lighting: could not ask logind to wait before sleeping:" << reply.error().message();
}

void LogindPowerEvents::allowSleep()
{
    m_lock = QDBusUnixFileDescriptor(); // closing the descriptor drops the lock
}

void LogindPowerEvents::onPrepareForSleepSignal(bool starting)
{
    if (starting)
        Q_EMIT aboutToSleep();
    else
        Q_EMIT resumed();
}

} // namespace galleond
