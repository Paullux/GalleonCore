// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingEnvironment.h"

#include <QDBusUnixFileDescriptor>

namespace galleond {

// Sleep and wake on Linux, from systemd-logind (system bus): PrepareForSleep says when the machine is about to sleep and
// when it woke, and a "delay" inhibitor lock makes it wait, up to its limit (5 s by default), until we have given the
// keys back and dropped the lock. Without logind, lighting is refused.
class LogindPowerEvents final : public PowerEvents
{
    Q_OBJECT

public:
    explicit LogindPowerEvents(QObject *parent = nullptr);

    [[nodiscard]] bool available(QString *why) const override;
    void holdSleep() override;
    void allowSleep() override;

private Q_SLOTS:
    void onPrepareForSleepSignal(bool starting);

private:

    QDBusUnixFileDescriptor m_lock;
};

} // namespace galleond
