// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QTimer>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <optional>

namespace galleond {

// Notices that "something was done at the computer" (the mouse, or the keyboard) from how long the computer says it has been idle, and from nothing else.
// The system only gives a duration: nothing is known of what was done, which key, which button, where, and nothing is kept.
//
// The pure part: given the idle times read one after the other, says when one of them is shorter than the one before (the idle time starts again from 0 at
// any action), with a little slack for the readings that are not exact.
class IdleActivityDetector
{
public:
    static constexpr qint64 SlackMs = 80;

    // True when this reading shows an action since the previous one. The first reading is only remembered.
    bool onReading(qint64 idleMs)
    {
        const bool acted = m_last.has_value() && idleMs + SlackMs < *m_last;
        m_last = idleMs;
        return acted;
    }
    void reset() { m_last.reset(); }

private:
    std::optional<qint64> m_last;
};

// Reads the idle time of the user's session, in milliseconds, and says it to `done` (nothing when the system does not say). The system's own call: KDE and
// GNOME on Linux (D-Bus), GetLastInputInfo on Windows. It may answer later (a D-Bus call).
using IdleReader = std::function<void(std::function<void(std::optional<qint64>)> done)>;
[[nodiscard]] IdleReader systemIdleReader();

// Something that says once that "something was done at the computer", while it watches, and that watches only on demand (the hidden rain only): nothing
// is observed between stop() and start(). It says `unavailable()` once, with no detail, when the system cannot tell.
class InputActivitySource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool watching() const = 0;

Q_SIGNALS:
    void activity(); // once per watch: the watch is over when it is said
    void unavailable();
};

// Watching by reading the idle time every `intervalMs` (D-Bus on KDE X11 and GNOME, GetLastInputInfo on Windows) and saying `activity()` when an action is
// seen, then stopping.
class InputActivityWatcher : public InputActivitySource
{
    Q_OBJECT

public:
    explicit InputActivityWatcher(IdleReader reader = systemIdleReader(), int intervalMs = 250, QObject *parent = nullptr);

    void start() override;
    void stop() override;
    [[nodiscard]] bool watching() const override { return m_timer.isActive(); }

private:
    void poll();

    IdleReader m_reader;
    QTimer m_timer;
    IdleActivityDetector m_detector;
    bool m_waiting = false;      // a reading is on its way
    bool m_reportedMissing = false;
    int m_generation = 0;        // a reading that comes after stop() is not for the current watch
};

// The best way that this system has to tell: on a Wayland session whose compositor has the idle notification protocol (KDE Plasma, where the idle time
// is not given by D-Bus), the compositor says when the session was idle and when it is used again; otherwise the idle time is read. Either way, only
// "an action" is known, never which one.
[[nodiscard]] std::unique_ptr<InputActivitySource> makeInputActivitySource();

} // namespace galleond
