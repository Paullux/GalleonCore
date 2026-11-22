// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>

namespace galleond {

class KeySignalsService;

// The D-Bus face of KeySignalsService, on the session bus of the person (never the system bus, never the network): `/org/galleoncore/KeySignals`, interface
// `org.galleoncore.KeySignals`. Only these three methods, and nothing else: no property, no signal, no getter. The sender of a call is the unique name the bus
// gives it, so a program's signals are its own and a program that leaves the bus has them taken away.
//
//   Set(key s, red i, green i, blue i, effect s, seconds d) -> s    "" when accepted, else why not. effect: "steady" or "pulse"; seconds: 1 to 86400
//   Clear(key s) -> b                                               takes away this program's signal on that key
//   ClearAll() -> b                                                 takes away every signal of this program
//
// Off unless the person allowed it in Bosun. `key` is the name of a key as KeyIdentity.h names it ("F3", "Left Control"), the place of the key on a QWERTY keyboard.
class KeySignalsBus : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.KeySignals")

public:
    explicit KeySignalsBus(KeySignalsService &service, QObject *parent = nullptr);

public Q_SLOTS:
    QString Set(const QString &key, int red, int green, int blue, const QString &effect, double seconds);
    bool Clear(const QString &key);
    bool ClearAll();

private:
    [[nodiscard]] QString sender();

    KeySignalsService &m_service;
    QDBusServiceWatcher m_watcher;
};

} // namespace galleond
