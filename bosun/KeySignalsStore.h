// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace bosun {

// The choice "other programs may light single keys", as the settings window holds it: OFF unless the person made it. Read from key-signals.json when the
// window starts (a missing or invalid file means off) and written back at each change, atomically; galleond reads the file and never writes it, and a program
// on the bus cannot change it. An empty path (a preview run) keeps the choice in memory and writes nothing.
class KeySignalsStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool allow READ allow NOTIFY changed)

public:
    explicit KeySignalsStore(QString path = {}, QObject *parent = nullptr);

    [[nodiscard]] bool allow() const { return m_allow; }

    // "" when done, else why not (the choice in memory is kept even if the file could not be written).
    Q_INVOKABLE [[nodiscard]] QString setAllow(bool allow);

Q_SIGNALS:
    void changed();

private:
    QString m_path;
    bool m_allow = false;
};

} // namespace bosun
