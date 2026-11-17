// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QTimer>
#include <QtGlobal>

#include <functional>
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

// While it watches, it reads the idle time every `intervalMs` and says `activity()` once when an action is seen, then stops watching. It starts and stops
// on demand (the hidden rain only), and says `unavailable()` once, with no detail, if the system does not give an idle time.
class InputActivityWatcher : public QObject
{
    Q_OBJECT

public:
    explicit InputActivityWatcher(IdleReader reader = systemIdleReader(), int intervalMs = 250, QObject *parent = nullptr);

    void start();
    void stop();
    [[nodiscard]] bool watching() const { return m_timer.isActive(); }

Q_SIGNALS:
    void activity();
    void unavailable();

private:
    void poll();

    IdleReader m_reader;
    QTimer m_timer;
    IdleActivityDetector m_detector;
    bool m_waiting = false;      // a reading is on its way
    bool m_reportedMissing = false;
    int m_generation = 0;        // a reading that comes after stop() is not for the current watch
};

} // namespace galleond
