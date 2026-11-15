// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingController.h"

#include <galleon/keyboard/AppearancePlayer.h>
#include <galleon/keyboard/LiveFrame.h>
#include <galleon/keyboard/LiveLink.h>
#include <galleon/keyboard/LiveScene.h>

#include <QDebug>
#include <QObject>
#include <QRandomGenerator>
#include <QString>
#include <QTimer>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
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
    // What it shows: the appearance (played over time, the positions of the LEDs given for it) and, over it, the scene of a tool if there is
    // one (the calibration, the keyboard drawing): the tool wins until the appearance is set again.
    LightingWorker(LightingController::Opener opener, LightingController::Clock clock, const galleon::keyboard::Appearance &appearance,
                   std::optional<galleon::keyboard::LiveScene> tool, std::vector<galleon::keyboard::LedPosition> positions)
        : m_opener(std::move(opener)), m_clock(std::move(clock)), m_tool(tool), m_toolDirty(tool.has_value())
    {
        m_player.setPositions(std::move(positions));
        static_cast<void>(m_player.setAppearance(appearance, 0.0)); // the controller only hands over valid appearances
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
        m_origin = m_clock();
        m_link->begin(m_origin);
        m_player.resend();
        m_toolDirty = m_tool.has_value();
        m_pending.reset();
        m_timer = std::make_unique<QTimer>();
        m_timer->setInterval(lighting::TickMs);
        connect(m_timer.get(), &QTimer::timeout, this, &LightingWorker::tick);
        m_timer->start();
        report(QStringLiteral("starting"));
    }

    // A tool shows this scene, whatever the appearance is, until the appearance is set again.
    void setToolScene(galleon::keyboard::LiveScene scene)
    {
        m_tool = scene;
        m_toolDirty = true;
    }

    // The appearance is played from now on, and the scene of a tool, if any, is let go: the appearance has been asked for.
    void setAppearance(const galleon::keyboard::Appearance &appearance)
    {
        m_tool.reset();
        m_toolDirty = false;
        m_pending.reset();
        static_cast<void>(m_player.setAppearance(appearance, secondsSince(m_clock())));
    }

    // The level of the appearance only (a curve of the day changes it every hour): the rainbow keeps its place.
    void setLevel(int level) { static_cast<void>(m_player.setLevel(level)); }

    // Called with a blocking connection: it returns once the keys are given back and the keyboard is closed. True when the
    // keyboard is known to be back in its own mode (or nothing was held); false when the release could not be confirmed,
    // so the caller must not stop the watchdog: the keyboard may still be in host mode.
    [[nodiscard]] bool stopAndRelease()
    {
        m_timer.reset();
        bool released = true;
        if (m_link) {
            // A keyboard that stopped answering ("gone") has nothing to release as far as the link can tell, but a read error
            // is not proof that it was unplugged: its mode was never seen back at 1, so this is not a confirmed release.
            const bool answeredUntilNow = m_link->status() != LiveLink::Status::Disconnected;
            const auto outcome = m_link->release(m_clock());
            if (outcome == galleon::keyboard::ReleaseOutcome::Failed || !answeredUntilNow) {
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
        if (m_link->generation() != m_generation) { // the keys were (re)taken: what is shown has to be sent again
            m_generation = m_link->generation();
            m_toolDirty = m_tool.has_value();
            m_player.resend();
            m_pending.reset();
        }
        if (m_link->status() != LiveLink::Status::Holding || !m_link->keysTaken())
            return;
        if (m_tool) { // a tool is showing something: that is what the keyboard gets
            if (m_toolDirty) {
                const auto frame = galleon::keyboard::sceneFrame(*m_tool);
                if (frame && m_link->show(*frame, now) == LiveLink::ShowResult::Sent) {
                    m_toolDirty = false; // dropped for pacing, or failed: it is tried again at the next tick
                    noteSent(*frame);
                }
            }
            return;
        }
        if (!m_pending)
            m_pending = m_player.sceneAt(secondsSince(now)); // nothing when it is the scene already shown
        if (!m_pending)
            return;
        const auto frame = galleon::keyboard::sceneFrame(*m_pending);
        if (!frame) {
            m_pending.reset(); // a scene that cannot be made into a frame is dropped, never sent half
            return;
        }
        if (m_link->show(*frame, now) == LiveLink::ShowResult::Sent) {
            noteSent(*frame);
            m_pending.reset(); // else it is kept for the next tick (dropped for pacing, or failed)
        }
    }

    // The number of LEDs of a frame that are not black. Said in the log when it changes, so that a dark picture does not pass for a lit one.
    void noteSent(const galleon::keyboard::LiveFrame &frame)
    {
        int lit = 0;
        for (std::size_t led = 0; led < galleon::keyboard::LiveLedCount; ++led) {
            const std::size_t at = 2 + 3 * led;
            if (frame.at(at) != 0 || frame.at(at + 1) != 0 || frame.at(at + 2) != 0)
                ++lit;
        }
        if (lit != m_loggedLit) {
            m_loggedLit = lit;
            qInfo() << "galleond: lighting: a picture of" << lit << "lit LEDs was sent";
        }
    }

    [[nodiscard]] double secondsSince(LiveClock::time_point now) const { return std::chrono::duration<double>(now - m_origin).count(); }

    LightingController::Opener m_opener;
    LightingController::Clock m_clock;
    std::unique_ptr<galleon::Transport> m_transport;
    std::unique_ptr<LiveLink> m_link;
    std::unique_ptr<QTimer> m_timer;
    QString m_reported;
    std::uint32_t m_generation = 0;
    LiveClock::time_point m_origin;
    galleon::keyboard::AppearancePlayer m_player;
    std::optional<galleon::keyboard::LiveScene> m_pending; // a scene from the player that has not been sent yet
    std::optional<galleon::keyboard::LiveScene> m_tool;
    bool m_toolDirty = false;
    int m_loggedLit = -1; // the number of lit LEDs last said in the log
};


} // namespace galleond
