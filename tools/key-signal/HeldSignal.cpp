// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HeldSignal.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusReply>

namespace keysignal {
namespace {
constexpr int CallTimeoutMs = 1500;
const QString &signalsPath()
{
    static const QString path = QStringLiteral("/org/galleoncore/KeySignals");
    return path;
}
const QString &signalsInterface()
{
    static const QString name = QStringLiteral("org.galleoncore.KeySignals");
    return name;
}
} // namespace

HeldSignal::HeldSignal(QDBusConnection bus, QString service) : m_bus(std::move(bus)), m_service(std::move(service)) {}

QString HeldSignal::light(const Arguments &arguments)
{
    if (!m_bus.isConnected())
        return QStringLiteral("there is no session bus");
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, signalsPath(), signalsInterface(), QStringLiteral("Set"));
    message << arguments.key << arguments.red << arguments.green << arguments.blue << (arguments.pulse ? QStringLiteral("pulse") : QStringLiteral("steady"))
            << static_cast<double>(arguments.seconds);
    // BlockWithGui keeps the event loop running while it waits: a service of this same process (the tests') can answer.
    const QDBusMessage reply = m_bus.call(message, QDBus::BlockWithGui, CallTimeoutMs);
    if (reply.type() == QDBusMessage::ErrorMessage)
        return QStringLiteral("galleond did not answer (is it running, and up to date?): %1").arg(reply.errorMessage());
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 1)
        return QStringLiteral("galleond gave an answer that is not understood");
    QString refusal = reply.arguments().constFirst().toString();
    if (!refusal.isEmpty())
        return refusal;
    m_key = arguments.key;
    m_lit = true;
    return {};
}

void HeldSignal::release()
{
    if (!m_lit)
        return;
    m_lit = false;
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, signalsPath(), signalsInterface(), QStringLiteral("Clear"));
    message << m_key;
    static_cast<void>(m_bus.call(message, QDBus::BlockWithGui, CallTimeoutMs)); // best effort: closing the connection takes it away as well
}

} // namespace keysignal
