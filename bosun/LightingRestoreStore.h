// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace bosun {

// The choice "put the live lighting back when galleond starts", as the settings window holds it: OFF unless the person made it. Read from
// lighting-restore.json when the window starts (a missing or invalid file means off) and written back at each change, atomically; galleond reads the file
// when it starts. This window never writes what galleond remembers (lighting-memory.json): the two files have one writer each. An empty path (a preview
// run) keeps the choice in memory and writes nothing.
class LightingRestoreStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool restore READ restore NOTIFY changed)

public:
    explicit LightingRestoreStore(QString path = {}, QObject *parent = nullptr);

    [[nodiscard]] bool restore() const { return m_restore; }

    // "" when done, else why not (the choice in memory is kept even if the file could not be written).
    Q_INVOKABLE [[nodiscard]] QString setRestore(bool restore);

Q_SIGNALS:
    void changed();

private:
    QString m_path;
    bool m_restore = false;
};

} // namespace bosun
