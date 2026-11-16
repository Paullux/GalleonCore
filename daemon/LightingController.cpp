// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingController.h"

#include "LedPositions.h"
#include "LightingWorker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QRandomGenerator>
#include <QTime>
#include <QTimer>

#include <chrono>
#include <vector>

namespace galleond {

using galleon::keyboard::LiveClock;

LightingController::LightingController(Opener opener, WatchdogGuard &watchdog, PowerEvents &power, Clock clock, QObject *parent)
    : QObject(parent), m_opener(std::move(opener)), m_clock(clock ? std::move(clock) : Clock([] { return LiveClock::now(); })),
      m_watchdog(watchdog), m_power(power), m_positions(builtInLedPositions())
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
    m_tool.reset(); // what a tool showed before is not carried over a switching off
    m_active = true;
    Q_EMIT activeChanged(true);
    startWorker();
    return {};
}

bool LightingController::SetColor(int red, int green, int blue)
{
    const auto scene = galleon::keyboard::uniformScene(red, green, blue);
    return scene && showScene(*scene);
}

bool LightingController::SetLed(int led, int red, int green, int blue)
{
    const auto scene = galleon::keyboard::singleLedScene(led, red, green, blue);
    return scene && showScene(*scene);
}

bool LightingController::SetLeds(const QList<int> &values)
{
    constexpr qsizetype Group = 4;
    if (values.isEmpty() || values.size() % Group != 0 || values.size() / Group > static_cast<qsizetype>(galleon::keyboard::LiveLedCount))
        return false;
    std::vector<galleon::keyboard::LedColor> leds;
    leds.reserve(static_cast<std::size_t>(values.size() / Group));
    for (qsizetype i = 0; i < values.size(); i += Group) {
        const int led = values.at(i);
        const int red = values.at(i + 1);
        const int green = values.at(i + 2);
        const int blue = values.at(i + 3);
        const auto inRange = [](int value) { return value >= 0 && value <= 255; };
        if (led < 0 || led >= static_cast<int>(galleon::keyboard::LiveLedCount) || !inRange(red) || !inRange(green) || !inRange(blue))
            return false;
        leds.push_back({static_cast<std::uint16_t>(led), static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green), static_cast<std::uint8_t>(blue)});
    }
    const auto scene = galleon::keyboard::perLedScene(leds);
    return scene && showScene(*scene);
}

bool LightingController::showScene(const galleon::keyboard::LiveScene &scene)
{
    m_tool = scene; // a tool shows this, over the appearance, until the appearance is set again
    if (m_worker)
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), scene] { worker->setToolScene(scene); }, Qt::QueuedConnection);
    return m_active;
}

bool LightingController::setAppearance(const galleon::keyboard::Appearance &appearance)
{
    if (!galleon::keyboard::isValid(appearance))
        return false;
    m_appearance = appearance; // kept even while off, so the next start shows it
    m_tool.reset();
    if (m_worker) {
        galleon::keyboard::Appearance shown = appearance;
        shown.level = effectiveLevel();
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), shown] { worker->setAppearance(shown); }, Qt::QueuedConnection);
    }
    return true;
}

bool LightingController::setLevelOverride(std::optional<int> level)
{
    if (level && (*level < 0 || *level > galleon::keyboard::LevelFull))
        return false;
    if (level == m_levelOverride)
        return true;
    m_levelOverride = level;
    if (m_worker)
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), shown = effectiveLevel()] { worker->setLevel(shown); }, Qt::QueuedConnection);
    return true;
}

double systemDayHours()
{
    const QTime now = QTime::currentTime();
    return now.hour() + now.minute() / 60.0 + now.second() / 3600.0;
}

galleon::keyboard::Appearance LightingController::shownAppearance() const
{
    galleon::keyboard::Appearance shown = m_appearance;
    shown.level = effectiveLevel();
    return shown;
}

void LightingController::startWorker()
{
    setState(QStringLiteral("starting"), {});
    m_worker = std::make_unique<LightingWorker>(m_opener, m_clock, shownAppearance(), m_tool, m_positions, m_dayHours);
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

