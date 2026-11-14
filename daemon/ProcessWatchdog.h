// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingEnvironment.h"

#include <QProcess>

namespace galleond {

// The real watchdog: tools/keyboard-watchdog (galleon-keyboard-watchdog), started as a child of galleond with galleond's
// process ID. It waits for that process to end for any reason and then gives the keyboard back if it is still in host
// mode. A child process is not killed when its parent crashes, which is the point.
//
// start() returns true only when the program exists, started, and is STILL RUNNING a moment later: a watchdog that ends
// at once (it could not watch the process, or could not parse its arguments) protects nothing. It writes nothing to any
// file (Aaryush1's first Guard crashed at start by logging inside the read-only application folder, unnoticed for two
// builds): its one line of output goes to galleond's own output.
//
// If it is still wanted when this object goes (a release that was not confirmed left it running), a second watchdog is
// started detached for the same process before this one is ended: a QProcess child is killed with its parent object, and
// that is exactly the moment the keys must not be left unwatched.
class ProcessWatchdog final : public WatchdogGuard
{
    Q_OBJECT

public:
    // `program` empty: galleon-keyboard-watchdog next to the running executable.
    explicit ProcessWatchdog(QString program = {}, QObject *parent = nullptr);
    ~ProcessWatchdog() override;

    [[nodiscard]] bool start(qint64 pid, QString *why) override;
    void stop() override;

    [[nodiscard]] static QString defaultProgram();
    // How long a watchdog must stay alive after it started to be believed to be running.
    static constexpr int SettleMs = 300;

private:
    QString m_program;
    QProcess m_process;
    bool m_wanted = false;
    qint64 m_pid = 0;
};

} // namespace galleond
