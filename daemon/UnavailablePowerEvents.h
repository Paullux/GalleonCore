// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingEnvironment.h"

namespace galleond {

// For a system where galleond cannot yet be told about sleep (Windows: its power notifications are not wired): lighting
// is refused there rather than run without giving the keys back before the machine sleeps.
class UnavailablePowerEvents final : public PowerEvents
{
    Q_OBJECT

public:
    using PowerEvents::PowerEvents;
    [[nodiscard]] bool available(QString *why) const override
    {
        *why = tr("galleond is not told about sleep on this system yet: the lighting stays off, because the keys could not be given back before sleeping.");
        return false;
    }
    void holdSleep() override {}
    void allowSleep() override {}
};

} // namespace galleond
