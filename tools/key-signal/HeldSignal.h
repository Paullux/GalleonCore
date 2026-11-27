// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeySignalArgs.h"

#include <QDBusConnection>
#include <QString>
#include <QStringList>

namespace keysignal {

// The signals on Linux (one for each key named): asked of galleond through the D-Bus interface org.galleoncore.KeySignals, and HELD by this object's connection. The signal belongs to the connection that asked
// for it and goes when that connection closes, so whoever uses this keeps the program alive for as long as the key should stay lit, and calls release() (or just ends).
class HeldSignal
{
public:
    // The bus and the name of the service are for tests, which put a fake galleond on the bus of the test.
    explicit HeldSignal(QDBusConnection bus = QDBusConnection::sessionBus(), QString service = QStringLiteral("org.galleoncore.Deck"));

    // "" when galleond took every request, else why not (galleond's own sentence when it refused: the box is not ticked, the lighting is off, a limit...). A call is taken whole or
    // not at all: when a key is refused, the ones already lit by this call are taken away again and nothing is held.
    [[nodiscard]] QString light(const Arguments &arguments);
    // Takes the signals away (galleond would anyway when the connection closes): one Clear for a single key, else one ClearAll (one request, not one for each key, as galleond takes
    // 20 a second). Nothing happens when nothing is lit.
    void release();
    [[nodiscard]] bool isLit() const { return !m_keys.isEmpty(); }

private:
    QDBusConnection m_bus;
    QString m_service;
    QStringList m_keys; // the keys that this object lit and still holds
};

} // namespace keysignal
