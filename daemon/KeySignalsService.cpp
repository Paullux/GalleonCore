// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeySignalsService.h"

#include "LightingController.h"

#include <galleon/keyboard/KeySignals.h>

namespace galleond {

using galleon::keyboard::SignalEffect;

KeySignalsService::KeySignalsService(LightingController &lighting, Allowed allowed, Clock clock, QObject *parent)
    : QObject(parent), m_lighting(lighting), m_allowed(std::move(allowed)),
      m_clock(clock ? std::move(clock) : Clock([] { return std::chrono::steady_clock::now(); }))
{
    m_access.setInterval(SignalAccessCheckMs);
    connect(&m_access, &QTimer::timeout, this, [this] { checkAccess(); });
}

bool KeySignalsService::takeRequest()
{
    const auto now = m_clock();
    while (!m_recent.empty() && now - m_recent.front() >= std::chrono::seconds(1))
        m_recent.pop_front();
    if (m_recent.size() >= static_cast<std::size_t>(MaxSignalRequestsPerSecond))
        return false;
    m_recent.push_back(now);
    return true;
}

QString KeySignalsService::set(const QString &sender, const QString &key, int red, int green, int blue, const QString &effect, double seconds)
{
    if (!m_allowed())
        return tr("Other programs are not allowed to light keys: the person allows it in Bosun.");
    if (!takeRequest())
        return tr("Too many requests: wait a moment.");
    SignalEffect kind = SignalEffect::Steady;
    if (effect == QLatin1String("pulse"))
        kind = SignalEffect::Pulse;
    else if (effect != QLatin1String("steady"))
        return tr(R"(The effect is "steady" or "pulse".)");
    if (!m_senders.contains(sender) && m_senders.size() >= MaxTrackedSenders)
        return tr("Too many programs have signals already.");
    const QString refusal = m_lighting.setSignal(sender, key, red, green, blue, kind, seconds);
    if (refusal.isEmpty()) {
        m_senders.insert(sender);
        watchAccess();
    }
    return refusal;
}

bool KeySignalsService::clear(const QString &sender, const QString &key)
{
    if (key.isEmpty() || !m_allowed() || !takeRequest())
        return false;
    return m_lighting.clearSignals(sender, key);
}

bool KeySignalsService::clearAll(const QString &sender)
{
    if (!m_allowed() || !takeRequest())
        return false;
    m_senders.erase(sender);
    return m_lighting.clearSignals(sender);
}

void KeySignalsService::senderGone(const QString &sender)
{
    if (m_senders.erase(sender) > 0)
        m_lighting.clearSignals(sender);
    if (m_senders.empty())
        m_access.stop();
}

void KeySignalsService::watchAccess()
{
    if (!m_access.isActive())
        m_access.start();
}

void KeySignalsService::checkAccess()
{
    if (m_allowed())
        return;
    for (const QString &sender : m_senders)
        m_lighting.clearSignals(sender);
    m_senders.clear();
    m_access.stop();
}

} // namespace galleond
