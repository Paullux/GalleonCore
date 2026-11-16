// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/Appearance.h>
#include <galleon/keyboard/SunTimes.h>

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>
#include <optional>

namespace bosun {

// The appearance of the live lighting as the settings window holds it: a base (one colour or a rainbow), retouches on single LEDs, a level.
// It is read from lighting-appearance.json when the window starts (a missing or invalid file gives dark keys) and written back after every
// change, atomically, so that galleond (which watches the file while the lighting is on, and reads it when it is switched on) shows the
// same thing and so that the drawing is the same when the window is opened again. Bosun never opens the keyboard: the file is the only way.
// It describes colours only: nothing about key presses is read, kept or written here.
class AppearanceStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString baseType READ baseType NOTIFY changed)      // "uniform", "rainbow" or "sky"
    Q_PROPERTY(QColor colour READ colour NOTIFY changed)           // the colour of a uniform base (black = the keys are dark)
    Q_PROPERTY(int rainbowSpeed READ rainbowSpeed NOTIFY changed)  // thousandths of a turn per second, 0 = fixed
    Q_PROPERTY(int level READ level NOTIFY changed)                // 0 to 1000
    Q_PROPERTY(QVariantList retouches READ retouches NOTIFY changed) // [{led, colour}, ...]
    Q_PROPERTY(bool followSun READ followSun NOTIFY changed)         // the sky follows the real sunrise and sunset (off by default)
    Q_PROPERTY(bool hasSkyPlace READ hasSkyPlace NOTIFY changed)     // a place was typed in the settings of the sky
    Q_PROPERTY(QString skyLatitude READ skyLatitude NOTIFY changed)  // "" when there is none (a decimal point, as the weather page)
    Q_PROPERTY(QString skyLongitude READ skyLongitude NOTIFY changed)

public:
    // `path` is the file to read and write ("" = nothing is read or written, for the previews).
    using Clock = std::function<QDateTime()>;
    // `widgetsPath` is the file of the weather widget (read only while the sky follows the sun, to find the place in use: "" = never read) and `clock`
    // the date and time of the computer (a test gives another).
    explicit AppearanceStore(QString path, QString widgetsPath = {}, Clock clock = &AppearanceStore::systemClock, QObject *parent = nullptr);
    [[nodiscard]] static QDateTime systemClock();

    [[nodiscard]] QString baseType() const;
    [[nodiscard]] QColor colour() const;
    [[nodiscard]] int rainbowSpeed() const { return m_appearance.rainbowSpeed; }
    [[nodiscard]] int level() const { return m_appearance.level; }
    [[nodiscard]] QVariantList retouches() const;
    [[nodiscard]] bool followSun() const { return m_appearance.followSun; }
    [[nodiscard]] bool hasSkyPlace() const { return m_appearance.skyPlace.has_value(); }
    [[nodiscard]] QString skyLatitude() const;
    [[nodiscard]] QString skyLongitude() const;
    [[nodiscard]] const galleon::keyboard::Appearance &appearance() const { return m_appearance; }

    // Each returns "" when done, else why not (the value was refused, or the file could not be written). The change is kept in memory even
    // when the file cannot be written, so that the drawing stays coherent.
    Q_INVOKABLE [[nodiscard]] QString setUniform(int red, int green, int blue); // the base becomes this colour; the retouches stay
    Q_INVOKABLE [[nodiscard]] QString setRainbow(int speed);                    // the base becomes the rainbow; the retouches stay
    Q_INVOKABLE [[nodiscard]] QString setSky();                                 // the base becomes the sky of the hour of the day; the retouches stay
    // The sky follows the real sunrise and sunset (or goes back to the fixed hours). Refused unless the sky is the base.
    Q_INVOKABLE [[nodiscard]] QString setFollowSun(bool follow);
    // The place typed in the settings of the sky, as text with a decimal point (what the weather page takes). Refused when it is not a latitude from
    // -90 to 90 and a longitude from -180 to 180. Used only when the weather widget has no place.
    Q_INVOKABLE [[nodiscard]] QString setSkyPlace(const QString &latitude, const QString &longitude);
    Q_INVOKABLE [[nodiscard]] QString clearSkyPlace();
    // Where the sunrise comes from now: "weather" (the place of the weather widget), "sky" (the place typed here), "none" (the fixed hours of the mockup,
    // because no place is known) or "off" (the sky does not follow the sun: nothing about a place is read).
    Q_INVOKABLE [[nodiscard]] QString sunSource() const;
    Q_INVOKABLE [[nodiscard]] QString retouch(int led, int red, int green, int blue); // this LED has its own colour over the base
    Q_INVOKABLE [[nodiscard]] QString clearRetouches();
    Q_INVOKABLE [[nodiscard]] QString setLevel(int level);
    Q_INVOKABLE [[nodiscard]] QString reset();                                  // dark keys, no retouch, full level

    // For the drawing: the colours of the sky at this hour (0 to 24), as the keyboard gets them: `rows` has the six rows from the top, `strip` the
    // light strip. The same functions as galleond's (SkyPalette.h), so the drawing and the keyboard cannot disagree.
    Q_INVOKABLE [[nodiscard]] QVariantMap skyAt(double hours) const;
    // The row of the sky (0 to 5) of a key at this height (millimetres, y downwards), or -1 for the light strip.
    Q_INVOKABLE [[nodiscard]] static int skyRowOf(double y);

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] QString commit(const galleon::keyboard::Appearance &next);
    [[nodiscard]] std::optional<galleon::keyboard::GeoPlace> placeInUse() const; // nothing unless the sky follows the sun
    [[nodiscard]] galleon::keyboard::SkyDay skyDay() const;

    QString m_path;
    QString m_widgetsPath;
    Clock m_clock;
    galleon::keyboard::Appearance m_appearance;
};

} // namespace bosun
