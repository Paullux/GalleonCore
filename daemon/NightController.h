// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

namespace galleon {
class StreamDeck;
}

namespace galleond {

// "Lights off until morning" for the module and the dial rings (the pure part, and the file, are in NightMode.h). While it lasts the module is dark:
// brightness 0 and a black picture on the screen and on the 12 keys, held in host mode (so the module does not fall back to its stored standby
// image, the lesson Aaryush1 gave on #111); and everything else that draws on it, the keys, the screen, the rings, is silent. This class draws the black
// and the brightness, keeps the end time, and says when the night starts and ends (`activeChanged`); whoever wires it (main.cpp) puts the others
// to sleep with the same signals as for the screensaver (PagesController::setSuspended, which also puts the rings out, and ScreenController::setSuspended)
// and keeps the screensaver and the green rain from starting. A key or a dial used during the night does nothing: the night ends by itself at its time,
// or when asked (Bosun, D-Bus EndNight).
//
// It touches only the module (the Stream Deck part, 1b1c:2b18). The keyboard is not part of this step, and nothing here is written to any flash.
//
// The end time is kept in a file written by this class only, so that a restart of galleond does not bring the lights back early: `resume()` reads it at
// the start. A file that is damaged, expired, or more than 24 h away means no night.
class NightController : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<QDateTime()>;

    // `deck` is not owned and must outlive the controller; it may not be open yet (the black is applied again when it is, see reapply()).
    // `path` is the file; empty keeps nothing across a restart. `now` is for the tests; the default is the clock of the computer.
    NightController(galleon::StreamDeck *deck, QString path, Clock now = {}, QObject *parent = nullptr);

    // Starts the night, ending at the next HH:MM. False, and nothing changed, for an hour outside 0 to 23 or a minute outside 0 to 59. Asked again while a
    // night lasts, it only moves the end. The night starts even if the file cannot be written (the person asked for the dark): it then does not survive a restart.
    bool start(int hour, int minute);
    // Ends the night now: the brightness is given back, the file removed, `activeChanged(false)` says so.
    void end();
    // At the start of galleond: takes up a night that was going on. True when there is one.
    bool resume();
    // The module came back (unplugged, replugged): the black and the brightness are sent again while the night lasts.
    void reapply();

    [[nodiscard]] bool isActive() const { return m_until > 0; }
    // The end, in seconds since 1970; 0 when there is no night.
    [[nodiscard]] qint64 until() const { return m_until; }

    // How often the end time is looked at. The clock of the computer can jump (it sleeps, it is set): each look compares with it. For the tests.
    void setTickMs(int ms) { m_tick.setInterval(ms); }

Q_SIGNALS:
    void activeChanged(bool active);

private:
    void begin(qint64 until);
    void apply();
    void tick();
    [[nodiscard]] qint64 nowSeconds() const;

    galleon::StreamDeck *m_deck;
    QString m_path;
    Clock m_now;
    QTimer m_tick;
    qint64 m_until = 0;
};

} // namespace galleond
