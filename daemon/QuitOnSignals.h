// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

#include <initializer_list>
#include <vector>

namespace galleond {

// Linux: makes SIGTERM (systemctl stop, a shutdown), SIGINT and SIGHUP end galleond through QCoreApplication::quit(), so
// that its destructors run and the live lighting gives the keys back. Without it those signals kill the process at once
// and only the watchdog is left to give them back, a moment later. The handler only writes a byte to a pipe (the one
// thing a signal handler may safely do); the event loop does the rest. At most one instance.
class QuitOnSignals final : public QObject
{
    Q_OBJECT

public:
    explicit QuitOnSignals(std::initializer_list<int> numbers, QObject *parent = nullptr);
    ~QuitOnSignals() override;

    // Number of signals received so far (for tests).
    [[nodiscard]] int received() const { return m_received; }

Q_SIGNALS:
    void signalReceived(int number);

private:
    void onReadable();

    std::vector<int> m_numbers; // a copy: an initializer_list would point at a temporary that is gone
    int m_received = 0;
};

} // namespace galleond
