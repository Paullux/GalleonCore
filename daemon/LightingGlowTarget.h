// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeyGlowDriver.h"
#include "LightingController.h"

namespace galleond {

// What the glow needs of the lighting, taken from the lighting controller (KeyGlowDriver.h). It is not a QObject and has no slot or signal: nothing of it is on
// the bus, and the controller's own glow functions are plain methods for the same reason.
class LightingGlowTarget final : public KeyGlowTarget
{
public:
    explicit LightingGlowTarget(LightingController &lighting) : m_lighting(lighting) {}

    [[nodiscard]] bool lightingOn() const override { return m_lighting.active(); }
    void setGlow(bool on) override { m_lighting.setKeyGlow(on); }
    void setColour(galleon::keyboard::Rgb colour) override { m_lighting.setKeyGlowColour(colour); }
    void touch(std::uint16_t led) override { m_lighting.touchKeyGlow(led); }
    [[nodiscard]] std::optional<std::uint16_t> ledForUsage(galleon::keyboard::HidUsage usage) const override { return m_lighting.ledForUsage(usage); }

private:
    LightingController &m_lighting;
};

} // namespace galleond
