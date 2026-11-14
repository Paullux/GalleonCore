// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingEnvironment.h"

#include <galleon/Transport.h>
#include <galleon/keyboard/LivePolicy.h>

#include <QObject>
#include <QString>
#include <QThread>

#include <functional>
#include <memory>

namespace galleond {

class LightingWorker;

// The live per-key lighting of the keyboard (part 1b1c:2b0c) as a component of galleond: EXPERIMENTAL and OFF. It is
// the keyboard half of the daemon, like KeyboardController, and shares nothing with the Stream Deck half
// (ScreenController, PagesController, TileRenderer, MprisController): it never calls them and they never call it.
// Exposed on D-Bus as org.galleoncore.Lighting (Linux).
//
// What is decided here (the library, galleon::keyboard::LiveLink, holds the protocol and the safety rules):
// - It does nothing until Enable(true), and it is never switched on by itself, nor remembered across starts.
// - Enable(true) is REFUSED, and the keyboard is not touched, unless (1) the system tells us about sleep in time, and
//   (2) the watchdog process is running. The watchdog is started for galleond's own process ID before the keyboard is
//   opened. If it ends while the lighting is on, the lighting is stopped and the keys given back.
// - The blocking exchanges with the keyboard run on one worker thread, never in the device loop that sends the Stream
//   Deck's keep-alive. The worker owns its transport; the keys are given back, on that thread, before it ends.
// - The keys are given back on Enable(false), at exit (the destructor waits for it), before the machine sleeps, and
//   when the link stops by itself (two failures in a row, the keyboard gone): it then stays off until asked again.
// - While it is on, brightness requests of KeyboardController are paused by whoever wires the two (main.cpp).
// - After a sleep it takes the keys again, going through the 3 s of listening of the link.
// Nothing is saved to the keyboard's own memory and nothing is sent that LiveCommandGuard does not allow.
class LightingController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.Lighting")

public:
    using Opener = std::function<std::unique_ptr<galleon::Transport>()>;
    using Clock = std::function<galleon::keyboard::LiveClock::time_point()>;

    // `watchdog` and `power` must outlive this object. `clock` is for tests; the default is the steady clock.
    LightingController(Opener opener, WatchdogGuard &watchdog, PowerEvents &power, Clock clock = {}, QObject *parent = nullptr);
    ~LightingController() override;

    // "off", "starting", "listening", "taking", "holding", "yielded", "sleeping".
    [[nodiscard]] QString state() const { return m_state; }
    // True from an accepted Enable(true) until the keys have been given back.
    [[nodiscard]] bool active() const { return m_active; }

public Q_SLOTS:
    // "" when accepted, else why not. Enable(false) gives the keys back (and waits for it) and is always accepted.
    QString Enable(bool on);
    // One colour for every key, 0 to 255 each; false when the lighting is off or a value is out of range.
    bool SetColor(int red, int green, int blue);
    [[nodiscard]] QString State() const { return m_state; }

Q_SIGNALS:
    void stateChanged(const QString &state, const QString &detail);
    void activeChanged(bool active);

private:
    void startWorker();
    bool stopWorker();
    void finish(const QString &detail);
    void setState(const QString &state, const QString &detail);
    void onWorkerState(const QString &state, const QString &detail);

    Opener m_opener;
    Clock m_clock;
    WatchdogGuard &m_watchdog;
    PowerEvents &m_power;
    QThread m_thread;
    std::unique_ptr<LightingWorker> m_worker; // moved to m_thread; handed over to it to be deleted there
    QString m_state = QStringLiteral("off");
    bool m_active = false;
    bool m_sleeping = false;
    int m_red = 90;
    int m_green = 90;
    int m_blue = 90;
};

} // namespace galleond
