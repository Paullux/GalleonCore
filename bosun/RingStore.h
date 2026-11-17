// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <RingSettings.h>

#include <QObject>
#include <QString>

namespace bosun {

// The settings of the two rings around the dials as the settings window holds them: lit or out, and a level. Read from dial-rings.json when the
// window starts (a missing or invalid file gives "lit, full level", as before the file existed) and written back after every change, atomically, so
// that galleond (which watches the file) shows the same thing. Bosun never opens the module: the file is the only way. An empty path (a preview run)
// keeps the settings in memory and writes nothing.
class RingStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool lit READ lit NOTIFY changed)
    Q_PROPERTY(int level READ level NOTIFY changed) // 0 to 1000

public:
    explicit RingStore(QString path = {}, QObject *parent = nullptr);

    [[nodiscard]] bool lit() const { return m_settings.lit; }
    [[nodiscard]] int level() const { return m_settings.level; }

    // Each returns "" when done, else why not (the settings in memory are kept even if the file could not be written, as the drawing follows them).
    Q_INVOKABLE [[nodiscard]] QString setLit(bool lit);
    Q_INVOKABLE [[nodiscard]] QString setLevel(int level);

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] QString commit(const galleond::RingSettings &next);

    QString m_path;
    galleond::RingSettings m_settings;
};

} // namespace bosun
