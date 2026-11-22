// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeySignalsBus.h"

#include "KeySignalsService.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace galleond {

KeySignalsBus::KeySignalsBus(KeySignalsService &service, QObject *parent)
    : QObject(parent), m_service(service), m_watcher(QString(), QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForUnregistration)
{
    connect(&m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
        m_service.senderGone(name);
        m_watcher.removeWatchedService(name);
    });
}

QString KeySignalsBus::sender()
{
    const QString name = message().service(); // the unique name (":1.42") the bus gave the caller
    if (!name.isEmpty() && !m_watcher.watchedServices().contains(name))
        m_watcher.addWatchedService(name);
    return name;
}

QString KeySignalsBus::Set(const QString &key, int red, int green, int blue, const QString &effect, double seconds)
{
    const QString who = sender();
    if (who.isEmpty())
        return tr("The program could not be told apart.");
    return m_service.set(who, key, red, green, blue, effect, seconds);
}

bool KeySignalsBus::Clear(const QString &key)
{
    const QString who = sender();
    return !who.isEmpty() && m_service.clear(who, key);
}

bool KeySignalsBus::ClearAll()
{
    const QString who = sender();
    return !who.isEmpty() && m_service.clearAll(who);
}

} // namespace galleond
