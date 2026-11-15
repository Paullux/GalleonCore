// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "DaemonStatus.h"
#include "KeyCalibration.h"

namespace bosun {

// Lights the keyboard for the calibration by asking galleond (Bosun never opens the keyboard): the LED alone, or one colour for every key.
// `daemon` must outlive this object.
class DaemonCalibrationLights final : public CalibrationLights
{
public:
    explicit DaemonCalibrationLights(DaemonStatus &daemon) : m_daemon(daemon) {}
    [[nodiscard]] bool ready() const override { return m_daemon.running() && m_daemon.lightingActive(); }
    [[nodiscard]] QString showLed(int led, int red, int green, int blue) override { return m_daemon.setLightingLed(led, red, green, blue); }
    [[nodiscard]] QString showAll(int red, int green, int blue) override { return m_daemon.setLightingColor(red, green, blue); }

private:
    DaemonStatus &m_daemon;
};

} // namespace bosun
