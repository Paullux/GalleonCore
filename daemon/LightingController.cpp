// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingController.h"

#include "KeyMapFile.h"
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
      m_watchdog(watchdog), m_power(power), m_positions(builtInLedPositions()), m_keyMap(builtInKeyMap())
{
    connect(&m_watchdog, &WatchdogGuard::died, this, [this] {
        if (!m_active)
            return;
        if (m_sessionEnding && m_sessionEnding()) { // the system is stopping everything: a normal end, put back at the next start
            finish(tr("The computer is shutting down: the lighting was stopped and the keys given back."), true);
            return;
        }
        // Not announced yet: the keys are given back at once (safety first) and the end is remembered as a failure, unless the system announces the end of the
        // session in the next moments. The detail says what was seen, so that the cause can be read from lighting-state.json.
        QString detail = tr("The watchdog ended, so the keys are no longer protected: the lighting was stopped and the keys given back.");
        if (m_sessionEnding)
            detail += tr(" (the end of the session was not announced yet)");
        finish(detail);
        if (m_sessionEnding && m_releasedConfirmed) {
            m_graceLeft = m_graceAttempts;
            m_graceTimer.setInterval(m_graceIntervalMs);
            m_graceTimer.start();
        }
    });
    m_graceTimer.setSingleShot(false);
    connect(&m_graceTimer, &QTimer::timeout, this, [this] {
        if (m_active || !m_releasedConfirmed || !m_sessionEnding) { // switched on again, or nothing to decide
            m_graceTimer.stop();
            return;
        }
        if (m_sessionEnding()) {
            m_graceTimer.stop();
            remember(LightingMemory::CleanExit); // it was the end of the session after all
            setState(QStringLiteral("off"), tr("The computer is shutting down: the lighting was stopped and the keys given back."));
        } else if (--m_graceLeft <= 0) {
            m_graceTimer.stop(); // not a shutdown: the watchdog really failed, and it stays remembered as off
        }
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
    finish({}, true); // the keys are given back before galleond goes, and the watchdog is not needed any more: a clean exit
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
    m_matrix = false;
    m_graceTimer.stop(); // a late decision about the previous end is void now
    m_active = true;
    remember(LightingMemory::Running); // from now on only a clean exit says that it may be put back
    Q_EMIT activeChanged(true);
    startWorker();
    return {};
}

QString LightingController::restoreAtStart()
{
    const QString refusal = Enable(true);
    if (refusal.isEmpty())
        m_resumeTriesLeft = m_resumeAttempts; // the first opening is reported later, on this thread: see onWorkerState
    return refusal;
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
    if (m_night)
        return false; // it is night: nothing is shown over the black
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
    Q_EMIT m_announcer.appearanceSet(appearance);
    if (m_worker && !m_matrix) { // while the rain shows, the appearance is kept for when it stops
        const galleon::keyboard::Appearance shown = shownAppearance();
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), shown] { worker->setAppearance(shown); }, Qt::QueuedConnection);
    }
    return true;
}

bool LightingController::setMatrixRain(bool on)
{
    if (!m_active)
        return false;
    if (on == m_matrix)
        return true;
    m_matrix = on;
    if (on)
        m_tool.reset(); // the rain is what the keys show: a tool's scene is let go
    if (m_worker) {
        const galleon::keyboard::Appearance shown = shownAppearance(); // the rain, or the appearance again
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), shown] { worker->setAppearance(shown); }, Qt::QueuedConnection);
    }
    return true;
}

void LightingController::setNight(bool night)
{
    if (night == m_night)
        return;
    m_night = night;
    if (night)
        m_tool.reset(); // what a tool showed is let go: the keys go black
    if (!m_worker)
        return; // not on: the next start shows what the night says (startWorker)
    if (night) {
        // The appearance again, at level 0: this also lets go of a tool's scene on the worker, which would otherwise be shown over the black.
        const galleon::keyboard::Appearance shown = shownAppearance();
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), shown] { worker->setAppearance(shown); }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), level = effectiveLevel()] { worker->setLevel(level); }, Qt::QueuedConnection);
    }
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

double LightingController::signalSeconds() const
{
    return std::chrono::duration<double>(m_clock().time_since_epoch()).count(); // the same measure as the worker's
}

void LightingController::sendSignals()
{
    if (!m_worker)
        return; // asleep, or not started yet: startWorker hands the board over
    QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), board = m_signals] { worker->setSignals(board); }, Qt::QueuedConnection);
}

QString LightingController::setSignal(const QString &client, const QString &key, int red, int green, int blue, galleon::keyboard::SignalEffect effect,
                                      double seconds)
{
    using galleon::keyboard::SignalBoard;
    if (!m_active)
        return tr("The lighting is off: a signal is only shown while it is on.");
    for (const int channel : {red, green, blue}) {
        if (channel < 0 || channel > 255)
            return tr("A colour is made of three numbers from 0 to 255.");
    }
    const auto usage = galleon::keyboard::hidUsageFromKeyName(key.toStdString());
    if (!usage)
        return tr("This is not the name of a key.");
    const auto led = m_keyMap.ledForKey(*usage);
    if (!led)
        return tr("The light of this key has not been measured yet.");
    const galleon::keyboard::Rgb colour{static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green), static_cast<std::uint8_t>(blue)};
    switch (m_signals.set(client.toStdString(), *led, colour, effect, signalSeconds(), seconds)) {
    case SignalBoard::Result::Ok:
        sendSignals();
        return {};
    case SignalBoard::Result::BadClient: return tr("The name of the program is 1 to 64 plain characters.");
    case SignalBoard::Result::BadLed: return tr("This key cannot be lit.");
    case SignalBoard::Result::BadSeconds: return tr("A signal lasts from 1 second to 24 hours.");
    case SignalBoard::Result::TooManyClients: return tr("Too many programs have signals already.");
    case SignalBoard::Result::TooManySignals: return tr("Too many signals are lit already.");
    }
    return tr("The signal was refused.");
}

bool LightingController::clearSignals(const QString &client, const QString &key)
{
    bool cleared = false;
    if (key.isEmpty()) {
        cleared = m_signals.clear(client.toStdString());
    } else if (const auto usage = galleon::keyboard::hidUsageFromKeyName(key.toStdString())) {
        if (const auto led = m_keyMap.ledForKey(*usage))
            cleared = m_signals.clear(client.toStdString(), *led);
    }
    if (cleared)
        sendSignals();
    return cleared;
}

void LightingController::setKeyGlow(bool on)
{
    if (on == m_glowOn)
        return;
    m_glowOn = on;
    if (m_worker)
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), on] { worker->setGlow(on); }, Qt::QueuedConnection);
}

void LightingController::setKeyGlowColour(galleon::keyboard::Rgb colour)
{
    if (colour == m_glowColour)
        return;
    m_glowColour = colour;
    if (m_worker)
        QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), colour] { worker->setGlowColour(colour); }, Qt::QueuedConnection);
}

void LightingController::touchKeyGlow(std::uint16_t led)
{
    if (!m_glowOn || !m_worker)
        return;
    QMetaObject::invokeMethod(m_worker.get(), [worker = m_worker.get(), led] { worker->touchGlow(led); }, Qt::QueuedConnection);
}

galleon::keyboard::Appearance LightingController::shownAppearance() const
{
    galleon::keyboard::Appearance shown = m_appearance;
    if (m_matrix) {
        shown = {};
        shown.base = galleon::keyboard::BaseType::Matrix; // the hidden rain: no retouch, the level of the person
    }
    shown.level = effectiveLevel();
    return shown;
}

void LightingController::startWorker()
{
    setState(QStringLiteral("starting"), {});
    m_worker = std::make_unique<LightingWorker>(m_opener, m_clock, shownAppearance(), m_night ? std::nullopt : m_tool, m_positions, m_dayHours, m_skyDay);
    m_worker->setSignals(m_signals); // before it moves: nobody else touches it yet
    m_worker->setGlow(m_glowOn);
    m_worker->setGlowColour(m_glowColour);
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

void LightingController::remember(LightingMemory memory) const
{
    if (m_remember)
        m_remember(memory);
}

void LightingController::finish(const QString &detail, bool cleanExit)
{
    m_resumeTimer.stop();
    m_resumeTriesLeft = 0;
    m_matrix = false;
    m_signals.clearAll(); // a signal is for a lighting that is on: the next one starts clean
    const bool released = stopWorker();
    m_releasedConfirmed = released;
    if (!m_active)
        return;
    m_sleeping = false;
    // The watchdog is only let go once the keyboard is confirmed back in its own mode. After a release that could not be
    // confirmed it keeps watching: it gives the keys back when galleond ends (and starting the lighting again reuses it).
    if (released)
        m_watchdog.stop();
    m_power.allowSleep();
    m_active = false;
    // Put back at the next start only after a clean exit with the keys confirmed back; any other end leaves it off until asked for again.
    remember(released && cleanExit ? LightingMemory::CleanExit : LightingMemory::Off);
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

