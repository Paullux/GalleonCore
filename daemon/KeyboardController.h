// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/Transport.h>

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace galleond {

// The keyboard half of galleond (part 1b1c:2b0c): sets the global brightness (property 0xEA) when the user
// asks for it from Bosun. Exposed on D-Bus as org.galleoncore.Keyboard (Linux) and through the local control
// endpoint (Windows). Wired in main.cpp only: it never calls, and is never called by, ScreenController,
// PagesController, TileRenderer or MprisController (AGENTS.md "Maintaining architecture over time").
//
// Policy, kept here and not in the library:
// - Nothing is opened at start-up and nothing is held: the keyboard's vendor interface is opened for one
//   request and closed again, so the Web Hub or any other program is never kept out. A write is only ever the
//   answer to a request; nothing is rewritten on its own.
// - The value is kept by the keyboard itself (it survives a power cycle, like a change made in the Web Hub),
//   so no restore on exit is wanted: the user chose it.
// - Only whole steps of 100 (0 to 1000) are accepted, one request is served at a time, the newest request
//   replaces one still waiting, and two writes are never closer than MinIntervalMs: each write may reach
//   the keyboard's own memory, so a script or a dragged slider cannot hammer it.
// - Writing blocks for up to a few reports' timeout: it runs on one worker thread, never in the device loop
//   that also sends the Stream Deck keep-alive. The worker owns its transport for the length of one job.
//   The destructor waits for a running job; the answer of a job is delivered on this object's thread.
class KeyboardController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.Keyboard")

public:
    // Opens the keyboard's vendor interface, or returns null when it cannot. Called on the worker thread.
    using Opener = std::function<std::unique_ptr<galleon::Transport>()>;

    static constexpr int StepSize = 100;
    static constexpr int MinIntervalMs = 300;

    explicit KeyboardController(Opener opener, QObject *parent = nullptr);
    ~KeyboardController() override;

    // The real keyboard: 1b1c:2b0c, interface 1, and only if it really is the vendor collection (usage page
    // 0xFF42, usage 1); anything else is closed and refused.
    [[nodiscard]] static Opener hidOpener();

    // Pure: the value to write, or nothing when it is not a step of 100 from 0 to 1000.
    [[nodiscard]] static std::optional<std::uint16_t> acceptedBrightness(int value);

public Q_SLOTS:
    // True when the request was accepted (it is served soon, newest first); false when the value is refused.
    bool SetBrightness(int value);

Q_SIGNALS:
    // The result of one write: `value` as asked, `ok` once the keyboard read it back identical.
    void brightnessWritten(int value, bool ok, const QString &reason);

private:
    void startNext();
    void finished(int value, bool ok, const QString &reason);

    Opener m_opener;
    QThreadPool m_pool;
    QTimer m_retry;
    QElapsedTimer m_sinceStart;
    std::optional<int> m_pending;
    bool m_busy = false;
    bool m_started = false;
};

} // namespace galleond
