// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <RingSettings.h>

#include <QObject>
#include <QString>

namespace bosun {

// The settings of the two rings around the dials as the settings window holds them: lit or out, a level, and a colour for each ring. Read from dial-rings.json when the
// window starts (a missing or invalid file gives "lit, full level", as before the file existed) and written back after every change, atomically, so
// that galleond (which watches the file) shows the same thing. Bosun never opens the module: the file is the only way. An empty path (a preview run)
// keeps the settings in memory and writes nothing.
class RingStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool lit READ lit NOTIFY changed)
    Q_PROPERTY(int level READ level NOTIFY changed) // 0 to 1000
    Q_PROPERTY(QString leftColour READ leftColour NOTIFY changed)   // "default" (the colour of the theme) or "#rrggbb"
    Q_PROPERTY(QString rightColour READ rightColour NOTIFY changed) // "default" (white) or "#rrggbb"

public:
    explicit RingStore(QString path = {}, QObject *parent = nullptr);

    [[nodiscard]] bool lit() const { return m_settings.lit; }
    [[nodiscard]] int level() const { return m_settings.level; }
    [[nodiscard]] QString leftColour() const { return galleond::ringColourText(m_settings.left); }
    [[nodiscard]] QString rightColour() const { return galleond::ringColourText(m_settings.right); }

    // Each returns "" when done, else why not (the settings in memory are kept even if the file could not be written, as the drawing follows them).
    Q_INVOKABLE [[nodiscard]] QString setLit(bool lit);
    Q_INVOKABLE [[nodiscard]] QString setLevel(int level);
    // "default" or "#rrggbb"; any other text changes nothing and says why.
    Q_INVOKABLE [[nodiscard]] QString setLeftColour(const QString &colour);
    Q_INVOKABLE [[nodiscard]] QString setRightColour(const QString &colour);

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] QString commit(const galleond::RingSettings &next);

    QString m_path;
    galleond::RingSettings m_settings;
};

} // namespace bosun
