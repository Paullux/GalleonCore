// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingController.h"

#include "LightingWorker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QRandomGenerator>
#include <QTimer>

#include <chrono>
#include <vector>

namespace galleond {

using galleon::keyboard::LiveClock;

LightingController::LightingController(Opener opener, WatchdogGuard &watchdog, PowerEvents &power, Clock clock, QObject *parent)
    : QObject(parent), m_opener(std::move(opener)), m_clock(clock ? std::move(clock) : Clock([] { return LiveClock::now(); })),
      m_watchdog(watchdog), m_power(power)
{
    connect(&m_watchdog, &WatchdogGuard::died, this, [this] {
        if (m_active)
            finish(tr("The watchdog ended, so the keys are no longer protected: the lighting was stopped and the keys given back."));
    });
    m_resumeTimer.setSingleShot(true);
    connect(&m_resumeTimer, &QTimer::timeout, this, [this] {
        if (m_active && !m_sleeping && !m_worker)
            startWorker();
    });
    connect(&m_power, &PowerEvents::aboutToSleep, this, [this] {
        m_resumeTimer.stop();
        m_resumeTriesLeft = 0;
        if (!m_active || m_sleeping)
            return;
        stopWorker(); // give the keys back before the machine sleeps
        m_sleeping = true;
        setState(QStringLiteral("sleeping"), tr("The keys were given back for the night; they are taken again on waking."));
        m_power.allowSleep();
    });
    connect(&m_power, &PowerEvents::resumed, this, [this] {
        if (!m_active || !m_sleeping)
            return;
        m_sleeping = false;
        m_power.holdSleep();
        m_resumeTriesLeft = m_resumeAttempts; // the keyboard may still be on its way back: see onWorkerState
        startWorker();
    });
}

LightingController::~LightingController()
{
    finish({}); // the keys are given back before galleond goes, and the watchdog is not needed any more
}

QString LightingController::Enable(bool on)
{
    if (!on) {
        if (m_active)
            finish({});
        return {};
    }
    if (m_active)
        return tr("The lighting is already on.");
    QString why;
    if (!m_power.available(&why)) {
        setState(QStringLiteral("off"), why);
        return why;
    }
    // The watchdog first, and the keyboard only if it runs: with no one to give the keys back after a crash, the
    // keyboard would stay in host mode until it is unplugged.
    if (!m_watchdog.start(QCoreApplication::applicationPid(), &why)) {
        setState(QStringLiteral("off"), why);
        return why;
    }
    m_power.holdSleep();
    m_active = true;
    Q_EMIT activeChanged(true);
    startWorker();
    return {};
}

bool LightingController::SetColor(int red, int green, int blue)
{
    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
        return false;
    m_red = red;
    m_green = green;
    m_blue = blue;
    if (m_worker)
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), red, green, blue] { worker->setColor(red, green, blue); }, Qt::QueuedConnection);
    return m_active;
}

void LightingController::startWorker()
{
    setState(QStringLiteral("starting"), {});
    m_worker = std::make_unique<LightingWorker>(m_opener, m_clock, m_red, m_green, m_blue);
    m_worker->moveToThread(&m_thread);
    connect(m_worker.get(), &LightingWorker::reported, this, &LightingController::onWorkerState);
    m_thread.start();
    QMetaObject::invokeMethod(m_worker.get(), &LightingWorker::start, Qt::QueuedConnection);
}

// Gives the keys back on the worker thread, waits for it, and ends the thread. False when the release could not be
// confirmed (the keyboard may still be in host mode); true when it was, or when nothing was held.
bool LightingController::stopWorker()
{
    if (!m_worker)
        return true;
    bool released = true;
    QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), &released] { released = worker->stopAndRelease(); }, Qt::BlockingQueuedConnection);
    LightingWorker *worker = m_worker.release(); // it belongs to its thread now: deleted there
    disconnect(worker, nullptr, this, nullptr);
    worker->deleteLater();
    m_thread.quit();
    m_thread.wait();
    return released;
}

void LightingController::finish(const QString &detail)
{
    m_resumeTimer.stop();
    m_resumeTriesLeft = 0;
    const bool released = stopWorker();
    if (!m_active)
        return;
    m_sleeping = false;
    // The watchdog is only let go once the keyboard is confirmed back in its own mode. After a release that could not be
    // confirmed it keeps watching: it gives the keys back when galleond ends (and starting the lighting again reuses it).
    if (released)
        m_watchdog.stop();
    m_power.allowSleep();
    m_active = false;
    setState(QStringLiteral("off"), released ? detail
                                             : tr("The keys could not be confirmed as given back. The watchdog keeps watching and gives them back when galleond ends.")
                                                   + (detail.isEmpty() ? QString() : QLatin1Char(' ') + detail));
    Q_EMIT activeChanged(false);
}

void LightingController::setState(const QString &state, const QString &detail)
{
    if (state == m_state && detail.isEmpty())
        return;
    m_state = state;
    Q_EMIT stateChanged(state, detail);
}

void LightingController::onWorkerState(const QString &state, const QString &detail)
{
    if (!m_active || m_sleeping)
        return;
    if (state != QStringLiteral("failed"))
        m_resumeTriesLeft = 0; // the keyboard was opened: no more waiting for it
    // Just after waking: the keyboard is not back yet, so try again in a moment instead of giving up (stopWorker only closes
    // what failed to open: there is nothing to release).
    if (state == QStringLiteral("failed") && m_resumeTriesLeft > 0) {
        --m_resumeTriesLeft;
        static_cast<void>(stopWorker());
        setState(QStringLiteral("starting"), tr("Waiting for the keyboard to come back after waking up."));
        m_resumeTimer.start(m_resumeRetryMs);
        return;
    }
    // The link stopped by itself, or the keyboard could not be opened: it stays off until asked again.
    if (state == QStringLiteral("failed") || state == QStringLiteral("frozen") || state == QStringLiteral("disconnected")) {
        finish(detail);
        return;
    }
    setState(state, detail);
}

} // namespace galleond

