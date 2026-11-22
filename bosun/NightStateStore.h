// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ReplacedFileWatcher.h>

#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>

namespace bosun {

// Whether "lights off until morning" is on, and until when, as the settings window shows it: read from night-mode.json, which galleond writes
// and this window never writes (the requests go to galleond; see DaemonStatus::startNight). The file is untrusted input: a damaged, expired or
// far-away end reads as no night (galleond::readNightUntil). It is read again when the file changes and every half minute, so that a night that has ended
// is not shown. An empty path (a preview run) shows no night.
class NightStateStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    // The end as a time of day in the language of the person ("07:00" or "7:00 AM"), empty when there is no night.
    Q_PROPERTY(QString endTime READ endTime NOTIFY changed)

public:
    using Clock = std::function<qint64()>; // seconds since 1970
    explicit NightStateStore(QString path = {}, Clock now = {}, QObject *parent = nullptr);

    [[nodiscard]] bool active() const { return m_until > 0; }
    [[nodiscard]] QString endTime() const;
    [[nodiscard]] qint64 until() const { return m_until; }

    // Reads the file now and says so if it changed what is shown. Public for the tests.
    void refresh();

Q_SIGNALS:
    void changed();

private:
    QString m_path;
    Clock m_now;
    galleond::ReplacedFileWatcher m_watcher;
    QTimer m_look;
    qint64 m_until = 0;
};

} // namespace bosun
