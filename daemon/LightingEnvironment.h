// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace galleond {

// What the live lighting needs from the system, as two small interfaces so that LightingController is tested without a
// system, a process or a sleep.

// The watchdog: a separate process that gives the keys back if galleond dies for any reason (tools/keyboard-watchdog,
// docs/WINDOWS_LIVE_LIGHTING_CSHARP.md "Release is the host's job"). The keyboard stays in host mode after a crash with
// no timeout, so lighting is NEVER switched on without it.
class WatchdogGuard : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    // Starts it for this process ID and returns once it is known to be running; false, with why, when it is not (not
    // installed, refused to start, ended at once). Called BEFORE the keyboard is touched.
    [[nodiscard]] virtual bool start(qint64 pid, QString *why) = 0;
    // The writer stopped normally and gave the keys back: the watchdog is not needed any more.
    virtual void stop() = 0;

Q_SIGNALS:
    // It ended while it was wanted: the keys are no longer protected.
    void died();
};

// Sleep and wake. The keys must be given back BEFORE the machine sleeps (the keyboard would otherwise wake up in host
// mode with a stale frame), so the system is asked to wait for it.
class PowerEvents : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    // Whether this system tells us about sleep in time. Without it lighting is refused, not run unprotected.
    [[nodiscard]] virtual bool available(QString *why) const = 0;
    // Ask the system to wait for us before it sleeps, and say that it may now.
    virtual void holdSleep() = 0;
    virtual void allowSleep() = 0;

Q_SIGNALS:
    void aboutToSleep(); // give the keys back now, then allowSleep()
    void resumed();      // the machine is awake again
};

} // namespace galleond
