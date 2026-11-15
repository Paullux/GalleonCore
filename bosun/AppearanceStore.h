// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/Appearance.h>

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariantList>

namespace bosun {

// The appearance of the live lighting as the settings window holds it: a base (one colour or a rainbow), retouches on single LEDs, a level.
// It is read from lighting-appearance.json when the window starts (a missing or invalid file gives dark keys) and written back after every
// change, atomically, so that galleond (which watches the file while the lighting is on, and reads it when it is switched on) shows the
// same thing and so that the drawing is the same when the window is opened again. Bosun never opens the keyboard: the file is the only way.
// It describes colours only: nothing about key presses is read, kept or written here.
class AppearanceStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString baseType READ baseType NOTIFY changed)      // "uniform" or "rainbow"
    Q_PROPERTY(QColor colour READ colour NOTIFY changed)           // the colour of a uniform base (black = the keys are dark)
    Q_PROPERTY(int rainbowSpeed READ rainbowSpeed NOTIFY changed)  // thousandths of a turn per second, 0 = fixed
    Q_PROPERTY(int level READ level NOTIFY changed)                // 0 to 1000
    Q_PROPERTY(QVariantList retouches READ retouches NOTIFY changed) // [{led, colour}, ...]

public:
    // `path` is the file to read and write ("" = nothing is read or written, for the previews).
    explicit AppearanceStore(QString path, QObject *parent = nullptr);

    [[nodiscard]] QString baseType() const;
    [[nodiscard]] QColor colour() const;
    [[nodiscard]] int rainbowSpeed() const { return m_appearance.rainbowSpeed; }
    [[nodiscard]] int level() const { return m_appearance.level; }
    [[nodiscard]] QVariantList retouches() const;
    [[nodiscard]] const galleon::keyboard::Appearance &appearance() const { return m_appearance; }

    // Each returns "" when done, else why not (the value was refused, or the file could not be written). The change is kept in memory even
    // when the file cannot be written, so that the drawing stays coherent.
    Q_INVOKABLE [[nodiscard]] QString setUniform(int red, int green, int blue); // the base becomes this colour; the retouches stay
    Q_INVOKABLE [[nodiscard]] QString setRainbow(int speed);                    // the base becomes the rainbow; the retouches stay
    Q_INVOKABLE [[nodiscard]] QString retouch(int led, int red, int green, int blue); // this LED has its own colour over the base
    Q_INVOKABLE [[nodiscard]] QString clearRetouches();
    Q_INVOKABLE [[nodiscard]] QString setLevel(int level);
    Q_INVOKABLE [[nodiscard]] QString reset();                                  // dark keys, no retouch, full level

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] QString commit(const galleon::keyboard::Appearance &next);

    QString m_path;
    galleon::keyboard::Appearance m_appearance;
};

} // namespace bosun
