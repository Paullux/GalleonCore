// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "MatrixTrigger.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace galleon {
class StreamDeck;
}

namespace galleond {

class ScreenRenderer;

// Easter egg: holding both dial buttons for 5 seconds makes green letters fall over the screen
// and the 12 keys (widgets/MatrixRain.qml) until anything is pressed or turned. Same contract as
// the screensaver: `activeChanged` tells the other controllers to stop drawing, then to draw again
// (which hands the screen and the keys back to the current page).
//
// The decision lives in MatrixTrigger (pure); this class gives it the clock and the deck's
// signals, and draws. Frames are paced by the time the last one took to send (never more than half
// of the daemon's time, as the dials are read by the same thread), and the whole run is bounded
// (MaxRunMs), so it cannot go on forever if the stop is lost. Nothing is written to the keyboard.
class MatrixRainController : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxRunMs = 5 * 60 * 1000;
    static constexpr int FrameIntervalMs = 90; // about 11 pictures per second at most

    // `deck` and `renderer` are not owned and must outlive the controller.
    // `renderer` shows MatrixRain.qml; a null or invalid one disables the easter egg.
    MatrixRainController(galleon::StreamDeck *deck, ScreenRenderer *renderer, QObject *parent = nullptr);
    ~MatrixRainController() override;

    [[nodiscard]] bool isActive() const { return m_trigger.isRunning(); }
    // A live game (or anything that owns the screen) keeps the easter egg from starting.
    void setInhibited(bool inhibited);
    // For the tests: a shorter hold.
    void setHoldMs(qint64 ms);

Q_SIGNALS:
    void activeChanged(bool active);

private:
    void onEvent(MatrixTrigger::Kind kind, int index);
    void apply(MatrixTrigger::Action action);
    void armTimer();
    void start();
    void stop();
    void drawFrame();

    galleon::StreamDeck *m_deck;
    ScreenRenderer *m_renderer;
    MatrixTrigger m_trigger;
    QElapsedTimer m_clock; // the time given to the trigger
    QTimer m_holdTimer;    // fires when the hold is long enough
    QTimer m_frameTimer;
    QElapsedTimer m_run;
    QElapsedTimer m_sinceFrame; // since the last picture was sent
    qint64 m_lastCostMs = 0;
    int m_tick = 0;
    int m_sendFailures = 0;
    bool m_inhibited = false;
};

} // namespace galleond
