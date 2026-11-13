// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "DaemonStatus.h"
#include "LocalControl.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QProcess>

namespace bosun {
DaemonStatus::DaemonStatus(QString systemctl, QObject *parent)
    : DaemonStatus(std::move(systemctl), galleond::localControlName(), parent)
{
}
DaemonStatus::DaemonStatus(QString program, QString endpoint, QObject *parent)
    : QObject(parent), m_systemctl(std::move(program)), m_endpoint(std::move(endpoint))
{
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, &DaemonStatus::look);
    connect(&m_probe, &QLocalSocket::connected, this, [this] {
        setRunning(true);
        m_probe.abort();
    });
    connect(&m_probe, &QLocalSocket::errorOccurred, this, [this] { setRunning(false); });
    // The keyboard's brightness comes back through the file galleond writes (the endpoint has no reply). Only a value
    // written after this window started is taken: an older one describes an earlier session.
    m_keyboardState = new KeyboardStateWatcher(galleond::keyboardStatePath(), QDateTime::currentSecsSinceEpoch(), this);
    connect(m_keyboardState, &KeyboardStateWatcher::brightnessChanged, this, [this](int value) { setKeyboardBrightnessKnown(nearestBrightnessStop(value)); });
    m_poll.start();
    look();
}

void DaemonStatus::setRunning(bool running)
{
    if (m_running != running) {
        m_running = running;
        Q_EMIT changed();
        if (m_running)
            static_cast<void>(readKeyboardBrightness());
    }
}

void DaemonStatus::look()
{
    if (m_probe.state() != QLocalSocket::UnconnectedState)
        return;
    m_probe.connectToServer(m_endpoint);
    QTimer::singleShot(500, &m_probe, [this] {
        if (m_probe.state() != QLocalSocket::UnconnectedState) {
            m_probe.abort();
            setRunning(false);
        }
    });
}

QStringList DaemonStatus::userCommand(const QString &verb)
{
    Q_UNUSED(verb)
    return {};
}

QString DaemonStatus::ask(const QString &verb)
{
    if (verb == QStringLiteral("stop")) {
        galleond::sendLocalCommand(m_endpoint, {galleond::LocalAction::Stop, {}}, this);
        return {};
    }
    const QString program = m_systemctl.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/galleond.exe") : m_systemctl;
    if (!QFileInfo(program).isAbsolute() || !QFileInfo(program).isFile()
        || !QProcess::startDetached(program, {}))
        return QCoreApplication::translate("DaemonStatus", "galleond could not be started.");
    return {};
}
QString DaemonStatus::showGaming() {
    if (!running()) return tr("Start the daemon to display Gaming on the pad");
    galleond::sendLocalCommand(m_endpoint, {galleond::LocalAction::Gaming, {}}, this);
    return {};
}
QString DaemonStatus::showTheme(const QString &page) {
    if (!running()) return {};
    galleond::sendLocalCommand(m_endpoint, {galleond::LocalAction::Theme, {}, page}, this);
    return {};
}
int DaemonStatus::nearestBrightnessStop(int value) {
    if (value < 0 || value > 1000) return -1;
    return (value + 50) / 100 * 100;
}
// The local endpoint has no reply: galleond reads the keyboard and publishes the value in keyboard-state.json,
// which m_keyboardState watches.
QString DaemonStatus::readKeyboardBrightness() {
    if (!running()) return {};
    galleond::sendLocalCommand(m_endpoint, {galleond::LocalAction::KeyboardBrightnessRead, {}, {}}, this);
    return {};
}
void DaemonStatus::onBrightnessRead(int, bool, const QString &) {}
void DaemonStatus::onBrightnessWritten(int, bool, const QString &) {}
void DaemonStatus::setKeyboardBrightnessKnown(int value) {
    if (value == m_keyboardBrightness) return;
    m_keyboardBrightness = value;
    Q_EMIT keyboardBrightnessChanged();
}
QString DaemonStatus::setKeyboardBrightness(int value) {
    if (!running()) return tr("Start galleond to change the keyboard brightness");
    galleond::sendLocalCommand(m_endpoint, {galleond::LocalAction::KeyboardBrightness, {}, {}, value}, this);
    return {};
}
QString DaemonStatus::start() { return running() ? QString{} : ask(QStringLiteral("start")); }
QString DaemonStatus::stop() { return running() ? ask(QStringLiteral("stop")) : QString{}; }
} // namespace bosun
