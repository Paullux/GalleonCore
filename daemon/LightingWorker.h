// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingController.h"

#include <galleon/keyboard/LiveFrame.h>
#include <galleon/keyboard/LiveLink.h>

#include <QDebug>
#include <QObject>
#include <QRandomGenerator>
#include <QString>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace galleond {

using galleon::keyboard::LiveClock;
using galleon::keyboard::LiveLink;

namespace lighting {
constexpr int TickMs = 50;                            // how often the link is given the time: well under its 1 s and 3 s steps
constexpr std::chrono::milliseconds LinkTimeout{500}; // one request and its answer
} // namespace lighting

// Lives on the worker thread: owns the transport and the link, and is the only code that talks to the keyboard.
class LightingWorker : public QObject
{
    Q_OBJECT

public:
    LightingWorker(LightingController::Opener opener, LightingController::Clock clock, int red, int green, int blue)
        : m_opener(std::move(opener)), m_clock(std::move(clock)), m_red(red), m_green(green), m_blue(blue)
    {
    }

public Q_SLOTS:
    void start()
    {
        m_transport = m_opener();
        if (!m_transport) {
            Q_EMIT reported(QStringLiteral("failed"), tr("The keyboard could not be opened (plugged in? rights? another program?)."));
            return;
        }
        LiveLink::Settings settings;
        settings.timeout = lighting::LinkTimeout;
        for (auto &byte : settings.sessionRandom)
            byte = static_cast<std::uint8_t>(QRandomGenerator::system()->bounded(256));
        m_link = std::make_unique<LiveLink>(*m_transport, settings);
        m_link->begin(m_clock());
        m_dirty = true;
        m_timer = std::make_unique<QTimer>();
        m_timer->setInterval(lighting::TickMs);
        connect(m_timer.get(), &QTimer::timeout, this, &LightingWorker::tick);
        m_timer->start();
        report(QStringLiteral("starting"));
    }

    void setColor(int red, int green, int blue)
    {
        m_red = red;
        m_green = green;
        m_blue = blue;
        m_dirty = true;
    }

    // Called with a blocking connection: it returns once the keys are given back and the keyboard is closed. True when the
    // keyboard is known to be back in its own mode (or nothing was held); false when the release could not be confirmed,
    // so the caller must not stop the watchdog: the keyboard may still be in host mode.
    [[nodiscard]] bool stopAndRelease()
    {
        m_timer.reset();
        bool released = true;
        if (m_link) {
            const auto outcome = m_link->release(m_clock());
            if (outcome == galleon::keyboard::ReleaseOutcome::Failed) {
                qWarning() << "galleond: lighting: the keys could not be confirmed as given back";
                released = false;
            }
            m_link.reset();
        }
        if (m_transport) {
            m_transport->close();
            m_transport.reset();
        }
        return released;
    }

Q_SIGNALS:
    void reported(const QString &name, const QString &detail);

private:
    void report(const QString &name, const QString &detail = {})
    {
        if (name == m_reported)
            return;
        m_reported = name;
        Q_EMIT reported(name, detail);
    }

    void tick()
    {
        const auto now = m_clock();
        m_link->tick(now);
        switch (m_link->status()) {
        case LiveLink::Status::Idle: report(QStringLiteral("starting")); break;
        case LiveLink::Status::Listening: report(QStringLiteral("listening")); break;
        case LiveLink::Status::Yielded: report(QStringLiteral("yielded"), tr("Another program is using the keyboard's lighting (the Web Hub?): nothing is sent until it has been quiet for a minute.")); break;
        case LiveLink::Status::Frozen: report(QStringLiteral("frozen"), tr("The lighting stopped after two failures in a row and gave the keys back.")); return;
        case LiveLink::Status::Disconnected: report(QStringLiteral("disconnected"), tr("The keyboard is gone.")); return;
        case LiveLink::Status::Released: return;
        case LiveLink::Status::Holding:
            report(m_link->keysTaken() ? QStringLiteral("holding") : QStringLiteral("taking"));
            break;
        }
        if (m_link->generation() != m_generation) { // the keys were (re)taken: the scene has to be sent again
            m_generation = m_link->generation();
            m_dirty = true;
        }
        if (!m_dirty || m_link->status() != LiveLink::Status::Holding || !m_link->keysTaken())
            return;
        std::vector<galleon::keyboard::LedColor> leds;
        leds.reserve(galleon::keyboard::LiveLedCount);
        for (std::uint16_t led = 0; led < galleon::keyboard::LiveLedCount; ++led)
            leds.push_back({led, static_cast<std::uint8_t>(m_red), static_cast<std::uint8_t>(m_green), static_cast<std::uint8_t>(m_blue)});
        const auto frame = galleon::keyboard::buildLiveFrame(leds);
        if (frame && m_link->show(*frame, now) == LiveLink::ShowResult::Sent)
            m_dirty = false; // dropped for pacing, or failed: it is tried again at the next tick
    }

    LightingController::Opener m_opener;
    LightingController::Clock m_clock;
    std::unique_ptr<galleon::Transport> m_transport;
    std::unique_ptr<LiveLink> m_link;
    std::unique_ptr<QTimer> m_timer;
    QString m_reported;
    std::uint32_t m_generation = 0;
    bool m_dirty = false;
    int m_red;
    int m_green;
    int m_blue;
};


} // namespace galleond
