// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeySignalArgs.h"

#include <QDBusConnection>
#include <QString>

namespace keysignal {

// A signal on Linux: asked of galleond through the D-Bus interface org.galleoncore.KeySignals, and HELD by this object's connection. The signal belongs to the connection that asked
// for it and goes when that connection closes, so whoever uses this keeps the program alive for as long as the key should stay lit, and calls release() (or just ends).
class HeldSignal
{
public:
    // The bus and the name of the service are for tests, which put a fake galleond on the bus of the test.
    explicit HeldSignal(QDBusConnection bus = QDBusConnection::sessionBus(), QString service = QStringLiteral("org.galleoncore.Deck"));

    // "" when galleond took the request, else why not (galleond's own sentence when it refused: the box is not ticked, the lighting is off, a limit...).
    [[nodiscard]] QString light(const Arguments &arguments);
    // Takes the signal away (galleond would anyway when the connection closes). Nothing happens when nothing is lit.
    void release();
    [[nodiscard]] bool isLit() const { return m_lit; }

private:
    QDBusConnection m_bus;
    QString m_service;
    QString m_key;
    bool m_lit = false;
};

} // namespace keysignal
