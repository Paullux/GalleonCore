// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyGlowDriver.h"

#include <utility>

namespace galleond {

KeyGlowDriver::KeyGlowDriver(KeyPressSource &source, KeyGlowTarget &target, Allowed allowed, QObject *parent)
    : QObject(parent), m_source(source), m_target(target), m_allowed(std::move(allowed))
{
    m_clock.start();
    m_poll.setInterval(m_pollMs);
    connect(&m_poll, &QTimer::timeout, this, [this] { refresh(); });
    m_resume.setSingleShot(true);
    connect(&m_resume, &QTimer::timeout, this, [this] {
        if (!m_source.watching())
            say(false);
    });
    connect(&m_source, &KeyPressSource::keyDown, this, [this](quint16 usage) { onKeyDown(usage); });
    connect(&m_source, &KeyPressSource::unavailable, this, [this] { onUnavailable(); });
    m_poll.start();
}

KeyGlowDriver::~KeyGlowDriver()
{
    m_poll.stop();
    if (m_source.watching()) {
        m_source.stop();
        m_target.setGlow(false);
    }
    if (m_said && m_hook)
        m_hook(false);
}

void KeyGlowDriver::say(bool watching)
{
    if (watching == m_said)
        return;
    m_said = watching;
    if (m_hook)
        m_hook(watching);
}

void KeyGlowDriver::refresh()
{
    const bool want = shouldWatch(m_allowed ? m_allowed() : false, m_target.lightingOn());
    if (want) {
        if (!m_source.watching() && m_clock.elapsed() >= m_retryNotBefore)
            startWatching();
    } else if (m_source.watching()) {
        stopWatching();
    }
}

void KeyGlowDriver::startWatching()
{
    say(true); // the trace is paused BEFORE the first key can be read (a trace that was about to resume finds the source watching again and says nothing)
    m_source.start();
    if (!m_source.watching())
        return; // it said unavailable(): onUnavailable() has set the delay before the next try
    m_target.setGlow(true);
}

void KeyGlowDriver::stopWatching()
{
    m_target.setGlow(false); // the touches are forgotten first
    m_source.stop();
    m_resume.start(m_traceResumeMs);
}

void KeyGlowDriver::onKeyDown(quint16 usage)
{
    if (const auto led = m_target.ledForUsage(usage))
        m_target.touch(*led); // at once, and nothing is kept here
}

void KeyGlowDriver::onUnavailable()
{
    // The source stopped by itself (the keyboard is gone, or there is no node): the glow goes with it, and it is tried again later if it is still wanted.
    m_target.setGlow(false);
    m_retryNotBefore = m_clock.elapsed() + m_retryMs;
    m_resume.start(m_traceResumeMs);
}

} // namespace galleond
