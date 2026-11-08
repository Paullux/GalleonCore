// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MatrixRainController.h"

#include "ScreenRenderer.h"

#include <galleon/StreamDeck.h>

#include <QDebug>

namespace galleond {

namespace {

constexpr int MaxSendFailures = 20; // consecutive frames the module did not take
constexpr int ScreenCell = 18;      // pixels per letter on the screen (720 x 384)
constexpr int KeyCell = 20;         // and on a key (160 x 160)

} // namespace

MatrixRainController::MatrixRainController(galleon::StreamDeck *deck, ScreenRenderer *renderer, QObject *parent)
    : QObject(parent)
    , m_deck(deck)
    , m_renderer(renderer)
{
    m_clock.start();
    m_holdTimer.setSingleShot(true);
    // A timer may fire a few milliseconds early (coarse timers): the trigger says "not yet" and
    // the timer is armed again for what is left.
    m_holdTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_holdTimer, &QTimer::timeout, this, [this] {
        apply(m_trigger.tick(m_clock.elapsed()));
        armTimer();
    });
    m_frameTimer.setInterval(FrameIntervalMs);
    connect(&m_frameTimer, &QTimer::timeout, this, &MatrixRainController::drawFrame);

    using Kind = MatrixTrigger::Kind;
    connect(m_deck, &galleon::StreamDeck::keyPressed, this, [this](int key) { onEvent(Kind::KeyPressed, key); });
    connect(m_deck, &galleon::StreamDeck::keyReleased, this, [this](int key) { onEvent(Kind::KeyReleased, key); });
    connect(m_deck, &galleon::StreamDeck::dialPressed, this, [this](int dial) { onEvent(Kind::DialPressed, dial); });
    connect(m_deck, &galleon::StreamDeck::dialReleased, this, [this](int dial) { onEvent(Kind::DialReleased, dial); });
    connect(m_deck, &galleon::StreamDeck::dialRotated, this, [this](int dial, int) { onEvent(Kind::DialRotated, dial); });
    // A module that went away forgets which buttons were held.
    connect(m_deck, &galleon::StreamDeck::disconnected, this, [this] {
        const bool wasRunning = m_trigger.isRunning();
        m_trigger.reset();
        m_holdTimer.stop();
        if (wasRunning)
            stop();
    });
}

MatrixRainController::~MatrixRainController()
{
    m_frameTimer.stop();
}

void MatrixRainController::setHoldMs(qint64 ms)
{
    m_trigger = MatrixTrigger(ms);
}

void MatrixRainController::setInhibited(bool inhibited)
{
    m_inhibited = inhibited;
    if (inhibited && m_trigger.isRunning()) {
        m_trigger.reset();
        stop();
    }
}

void MatrixRainController::onEvent(MatrixTrigger::Kind kind, int index)
{
    if (m_inhibited || !m_renderer || !m_renderer->isValid())
        return;
    const bool wasRunning = m_trigger.isRunning();
    const MatrixTrigger::Action action = m_trigger.onEvent(kind, index, m_clock.elapsed());
    if (wasRunning && action == MatrixTrigger::Action::Stop) {
        // The press or turn that ends it must not also act on the page it was meant for: the
        // other controllers are still suspended when this runs (they get the signal after it).
        QMetaObject::invokeMethod(this, [this] { stop(); }, Qt::QueuedConnection);
        return;
    }
    apply(action);
    armTimer();
}

void MatrixRainController::apply(MatrixTrigger::Action action)
{
    if (action == MatrixTrigger::Action::Start)
        start();
}

void MatrixRainController::armTimer()
{
    const qint64 wait = m_trigger.msUntilStart(m_clock.elapsed());
    if (wait < 0)
        m_holdTimer.stop();
    else
        m_holdTimer.start(static_cast<int>(wait));
}

void MatrixRainController::start()
{
    m_holdTimer.stop();
    m_tick = 0;
    m_sendFailures = 0;
    m_lastCostMs = 0;
    m_sinceFrame.invalidate();
    m_run.start();
    qInfo() << "galleond: matrix rain on";
    Q_EMIT activeChanged(true);
    drawFrame();
    m_frameTimer.start();
}

void MatrixRainController::stop()
{
    m_frameTimer.stop();
    qInfo() << "galleond: matrix rain off";
    // The controllers draw their page again when they hear this.
    Q_EMIT activeChanged(false);
}

void MatrixRainController::drawFrame()
{
    if (!m_trigger.isRunning())
        return;
    if (m_run.hasExpired(MaxRunMs)) {
        m_trigger.reset();
        stop();
        return;
    }
    // Rendering and sending take the daemon's thread, which also reads the dials: a frame that
    // follows the previous one too closely is skipped.
    if (m_sinceFrame.isValid() && m_sinceFrame.elapsed() < m_lastCostMs)
        return;
    QElapsedTimer cost;
    cost.start();
    ++m_tick;
    constexpr QSize ScreenSize(galleon::protocol::ScreenWidth, galleon::protocol::ScreenHeight);
    constexpr QSize KeySize(galleon::protocol::KeyImageSize, galleon::protocol::KeyImageSize);
    bool ok = true;
    // Surfaces are drawn by number: 0 is the screen, 1..12 the keys (each rains differently).
    const QImage screen = m_renderer->render({{QStringLiteral("tick"), m_tick}, {QStringLiteral("seed"), 0}, {QStringLiteral("cell"), ScreenCell}}, ScreenSize);
    ok = !screen.isNull() && m_deck->setScreenImage(screen, 80, false);
    for (int key = 0; key < galleon::protocol::KeyCount; ++key) {
        const QImage image = m_renderer->render({{QStringLiteral("tick"), m_tick}, {QStringLiteral("seed"), key + 1}, {QStringLiteral("cell"), KeyCell}}, KeySize);
        ok = !image.isNull() && m_deck->setKeyImage(key, image, false) && ok;
    }
    if (ok) {
        m_sendFailures = 0;
    } else if (++m_sendFailures >= MaxSendFailures) {
        qWarning() << "galleond: the module does not take the matrix rain pictures: back to the normal screen";
        m_trigger.reset();
        stop();
        return;
    }
    m_lastCostMs = cost.elapsed();
    m_sinceFrame.start();
}

} // namespace galleond
