// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/CalibrationFlow.h>

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>
#include <optional>

namespace bosun {

// How the calibration lights the keyboard, behind an interface so it is tested without a daemon: Bosun never opens the keyboard,
// it only asks galleond (DaemonStatus::setLightingLed / setLightingColor).
class CalibrationLights
{
public:
    CalibrationLights() = default;
    CalibrationLights(const CalibrationLights &) = delete;
    CalibrationLights &operator=(const CalibrationLights &) = delete;
    virtual ~CalibrationLights() = default;
    // True when the live lighting is on, so a LED can be lit.
    [[nodiscard]] virtual bool ready() const = 0;
    // "" when the request was sent, else why not.
    [[nodiscard]] virtual QString showLed(int led, int red, int green, int blue) = 0;
    [[nodiscard]] virtual QString showAll(int red, int green, int blue) = 0;
};

// The calibration of the key map: one LED is lit alone and the person presses the key that lit up. It is the first part of
// the software that reads key presses, so it follows AGENTS.md, "Security and privacy by design":
//  - OFF until begin() is called, and ignoring every key until then (handleKey does nothing when it is not active);
//  - it receives key codes only from the window that has the focus (the QML hands over the event of its own window): no global
//    capture, no hook, no extra privilege;
//  - it keeps no key press: the code is turned into a key identity, written into the map at once, and forgotten. No order, no
//    time, no history; nothing is logged, and the only text shown is the last answer, replaced by the next;
//  - the file it writes (keymap.json) holds LED to key answers only.
// Output goes through CalibrationLights; the map is saved after every answer so that closing the window loses nothing.
class KeyCalibration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(int currentLed READ currentLed NOTIFY changed)    // the LED lit now, or -1
    Q_PROPERTY(int passed READ passed NOTIFY changed)            // LEDs answered, skipped or already known
    Q_PROPERTY(int total READ total CONSTANT)
    Q_PROPERTY(int skipped READ skipped NOTIFY changed)
    Q_PROPERTY(bool finished READ finished NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)      // what to tell the person about the last action

public:
    // `lights` must outlive this object. `mapPath` is where keymap.json is read and written.
    KeyCalibration(CalibrationLights &lights, QString mapPath, QObject *parent = nullptr);
    ~KeyCalibration() override;

    [[nodiscard]] bool active() const { return m_flow.has_value(); }
    [[nodiscard]] int currentLed() const;
    [[nodiscard]] int passed() const;
    [[nodiscard]] static int total();
    [[nodiscard]] int skipped() const;
    [[nodiscard]] bool finished() const;
    [[nodiscard]] QString message() const { return m_message; }

    // Starts the calibration and lights the first LED with no answer. `startOver` ignores an earlier keymap.json (it is
    // replaced as the answers come). "" when started, else why not (the live lighting is off, or it is running already).
    Q_INVOKABLE [[nodiscard]] QString begin(bool startOver);
    // Stops, gives the colour back to every key and stops reading keys. The answers are already saved.
    Q_INVOKABLE void end();

    // A key press of the window that has the focus: QKeyEvent::nativeScanCode. Nothing happens when not active.
    Q_INVOKABLE void handleKey(unsigned nativeScanCode);
    // The keys that the system keeps from the window, or that send no code, to answer by choosing them (not by pressing them):
    // a list of {label, usage}. The same identities as a key press gives; the labels are translated.
    Q_INVOKABLE [[nodiscard]] static QVariantList specialKeys();
    Q_INVOKABLE void answerNamedKey(int usage);
    Q_INVOKABLE void answerNone();                // "no key lit up"
    Q_INVOKABLE void answerStrip(int position);   // "a section of the light strip lit up", 1 = leftmost
    Q_INVOKABLE void skip();
    Q_INVOKABLE void back();

Q_SIGNALS:
    void changed();

private:
    void applyKey(galleon::keyboard::HidUsage key);
    void settle(const QString &message); // lights the next LED (or finishes), saves, tells the person
    void say(const QString &message);
    [[nodiscard]] bool save();

    CalibrationLights &m_lights;
    QString m_path;
    std::optional<galleon::keyboard::CalibrationFlow> m_flow;
    QString m_message;
};

} // namespace bosun
