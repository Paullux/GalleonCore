// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <chrono>
#include <deque>
#include <functional>
#include <set>

namespace galleond {

class LightingController;

// How many requests a second the service takes from every program together; the others are refused with a word, not queued.
inline constexpr int MaxSignalRequestsPerSecond = 20;
// How many programs the service keeps track of at once (the board itself holds fewer: KeySignals.h).
inline constexpr std::size_t MaxTrackedSenders = 32;
// How often, while some program has signals, the choice of the person is looked at again: switching it off in Bosun takes the signals away within this time.
inline constexpr int SignalAccessCheckMs = 1000;

// The door of the key signals for other programs (the D-Bus object of Linux, the local IPC of Windows later), without anything of either: it is given the
// sender of each request by the transport, which is what makes a program's signals its own (it cannot ask for another program's signals to be taken away, and
// it cannot choose another name). Declarative and write-only, as AGENTS.md asks (rules 1 to 8): a request says what to light and for how long, an answer
// says only whether it was accepted and, if not, why; nothing here returns a key, a press, a colour or a list of signals.
//
// Off unless the person allowed it (`allowed`, read from key-signals.json, written by Bosun only). When it is switched off while signals are lit, they are taken
// away within SignalAccessCheckMs. A program that disappears has its signals taken away (senderGone), and every signal ends by itself anyway (a duration is
// mandatory). Nothing here is written to disk, logged with a key name, or kept after the daemon stops.
class KeySignalsService : public QObject
{
    Q_OBJECT

public:
    using Allowed = std::function<bool()>;
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    // `lighting` must outlive this object. `clock` is for tests.
    KeySignalsService(LightingController &lighting, Allowed allowed, Clock clock = {}, QObject *parent = nullptr);

    // "" when accepted, else why not, for a person to read. `effect` is "steady" or "pulse".
    QString set(const QString &sender, const QString &key, int red, int green, int blue, const QString &effect, double seconds);
    // False when there was nothing of this program's to take away (or it is not allowed to ask).
    bool clear(const QString &sender, const QString &key);
    bool clearAll(const QString &sender);
    // The program is gone from the bus: its signals go with it.
    void senderGone(const QString &sender);

private:
    [[nodiscard]] bool takeRequest();
    void watchAccess();
    void checkAccess();

    LightingController &m_lighting;
    Allowed m_allowed;
    Clock m_clock;
    std::set<QString> m_senders;
    std::deque<std::chrono::steady_clock::time_point> m_recent; // when the last accepted requests were made
    QTimer m_access;
};

} // namespace galleond
