// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProcessWatchdog.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace galleond {

ProcessWatchdog::ProcessWatchdog(QString program, QObject *parent)
    : WatchdogGuard(parent), m_program(program.isEmpty() ? defaultProgram() : std::move(program))
{
    m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    connect(&m_process, &QProcess::finished, this, [this] {
        if (m_wanted) {
            m_wanted = false;
            Q_EMIT died(); // it ended while it was wanted: the keys are no longer protected
        }
    });
}

ProcessWatchdog::~ProcessWatchdog()
{
    // Still wanted: the keys were not confirmed back. Hand the watching to a process that outlives this object (and galleond).
    if (m_wanted && m_process.state() != QProcess::NotRunning)
        QProcess::startDetached(m_program, {QStringLiteral("--pid"), QString::number(m_pid)});
    stop();
}

QString ProcessWatchdog::defaultProgram()
{
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("galleon-keyboard-watchdog.exe");
#else
    const QString name = QStringLiteral("galleon-keyboard-watchdog");
#endif
    return QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
}

bool ProcessWatchdog::start(qint64 pid, QString *why)
{
    if (m_process.state() != QProcess::NotRunning) {
        m_wanted = true;
        return true; // already watching this process
    }
    const QFileInfo program(m_program);
    if (!program.isFile() || !program.isExecutable()) {
        *why = tr("The watchdog program (galleon-keyboard-watchdog) is not installed next to galleond: the lighting stays off, because nothing would give the keys back after a crash.");
        return false;
    }
    m_wanted = false; // a watchdog that ends at once is not a death to report: start() says it
    m_process.start(m_program, {QStringLiteral("--pid"), QString::number(pid)});
    if (!m_process.waitForStarted(2000)) {
        *why = tr("The watchdog program could not be started (%1): the lighting stays off.").arg(m_process.errorString());
        return false;
    }
    // It waits for galleond to end: if it is gone after a moment it protects nothing.
    if (m_process.waitForFinished(SettleMs)) {
        *why = tr("The watchdog ended at once (code %1): the lighting stays off.").arg(m_process.exitCode());
        return false;
    }
    m_wanted = true;
    m_pid = pid;
    return true;
}

void ProcessWatchdog::stop()
{
    m_wanted = false;
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_process.kill(); // it only waits: nothing to clean up
    m_process.waitForFinished(2000);
}

} // namespace galleond
