// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BrightnessCurve.h"
#include "ReplacedFileWatcher.h"

#include <QObject>
#include <QString>
#include <QTime>
#include <QTimer>

#include <functional>
#include <optional>

namespace galleond {

class KeyboardController;

// Applies the brightness curve of the day (BrightnessCurve.h) to the keyboard, through KeyboardController. It is
// the "curve" half of the choice between the manual slider and the curve: in slider mode this does nothing at all.
//
// Policy, kept here (the library knows nothing of it):
// - Only in curve mode. Switching to the slider in the file stops it at once; the keyboard keeps whatever it
//   holds, like after any other write (no restore). A missing or damaged file does not switch the curve off or
//   on: the curve in force stays (a file caught half-written must not change the keyboard); to stop it, choose
//   the slider.
// - It writes only when its target CHANGES (the point for the current time, so at most once an hour) and once
//   when it starts or when the curve is switched on. It never rewrites a value to "keep" it: if the Web Hub or
//   the user's slider changes the keyboard in between, the curve does not fight it until its next point.
// - Before writing it READS the keyboard, and does not write if the keyboard already holds the target: no
//   needless write to the keyboard's own memory at every start. If the read fails nothing is written and it tries
//   again at the next tick.
// - The curve file is untrusted: an invalid file is ignored whole (the curve in force stays), never partly read.
// - The time of day comes from an injected clock (tests), checked every `tickMs` (30 s by default): a change of
//   the system clock, a sleep or a daylight-saving jump is picked up at the next tick.
// It only calls KeyboardController (the keyboard half of the daemon) and listens to its signals; it never calls,
// and is never called by, the Stream Deck side (ScreenController, PagesController, TileRenderer, MprisController).
class BrightnessScheduler : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<QTime()>;

    BrightnessScheduler(KeyboardController &keyboard, const QString &curvePath, Clock clock, int tickMs = 30000, QObject *parent = nullptr);

    // Reads the curve file again and decides. Called when the file changes.
    void reload();
    // Decides now: public for tests, called by the timer.
    void evaluate();

    // The curve in force, if any valid file was read.
    [[nodiscard]] std::optional<BrightnessCurve> curve() const { return m_curve; }
    // The value this object last got the keyboard to hold (confirmed), if any.
    [[nodiscard]] std::optional<int> applied() const { return m_applied; }

private:
    void onRead(int value, bool ok);
    void onWritten(int value, bool ok);
    [[nodiscard]] bool active() const;

    KeyboardController &m_keyboard;
    QString m_path;
    Clock m_clock;
    ReplacedFileWatcher m_watcher;
    QTimer m_tick;
    std::optional<BrightnessCurve> m_curve;
    std::optional<int> m_applied;   // confirmed on the keyboard by this object
    std::optional<int> m_target;    // being checked or written
    bool m_awaitingRead = false;
    bool m_awaitingWrite = false;
};

} // namespace galleond
