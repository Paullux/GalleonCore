// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/ReactiveGlow.h"

#include <algorithm>
#include <cmath>

namespace galleon::keyboard {
namespace {

std::uint8_t mixed(std::uint8_t from, double target, double share)
{
    const double value = static_cast<double>(from) * (1.0 - share) + target * share;
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

bool isValidGlowFade(double seconds)
{
    return std::isfinite(seconds) && seconds >= MinGlowFadeSeconds && seconds <= MaxGlowFadeSeconds;
}

double glowShare(double age, double fade)
{
    if (!isValidGlowFade(fade))
        fade = DefaultGlowFadeSeconds;
    if (!std::isfinite(age) || age <= 0)
        return 1.0;
    if (age >= fade)
        return 0.0;
    const double left = 1.0 - age / fade;
    return left * left;
}

bool GlowBoard::touch(std::uint16_t led, double now)
{
    if (led >= LiveLedCount || !std::isfinite(now))
        return false;
    std::erase_if(m_touches, [led](const Touch &touch) { return touch.led == led; });
    if (m_touches.size() >= MaxGlowing)
        m_touches.erase(m_touches.begin());
    m_touches.push_back({led, now});
    return true;
}

void GlowBoard::expire(double now, double fade)
{
    if (!isValidGlowFade(fade))
        fade = DefaultGlowFadeSeconds;
    std::erase_if(m_touches, [&](const Touch &touch) {
        return now - touch.time >= fade; // the fade is never above MaxGlowFadeSeconds, so nothing outlives it
    });
}

void GlowBoard::clear()
{
    m_touches.clear();
}

std::vector<LedShare> GlowBoard::shares(double now, double fade) const
{
    std::vector<LedShare> glow;
    glow.reserve(m_touches.size());
    for (const Touch &touch : m_touches) {
        const double share = glowShare(now - touch.time, fade);
        if (share > 0)
            glow.push_back({touch.led, share});
    }
    std::ranges::sort(glow, [](const LedShare &a, const LedShare &b) { return a.led < b.led; });
    return glow;
}

LiveFrame withGlow(LiveFrame frame, std::span<const LedShare> glow, Rgb colour, int level)
{
    const double brightness = static_cast<double>(std::clamp(level, 0, LevelFull)) / LevelFull;
    for (const LedShare &entry : glow) {
        if (entry.led >= LiveLedCount || !std::isfinite(entry.share))
            continue; // never beyond the frame
        const double share = std::clamp(entry.share, 0.0, 1.0);
        const std::size_t at = 2 + 3 * static_cast<std::size_t>(entry.led);
        frame.at(at) = mixed(frame.at(at), colour.red * brightness, share);
        frame.at(at + 1) = mixed(frame.at(at + 1), colour.green * brightness, share);
        frame.at(at + 2) = mixed(frame.at(at + 2), colour.blue * brightness, share);
    }
    return frame;
}

} // namespace galleon::keyboard
