// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LivePolicy.h"

#include <algorithm>

namespace galleon::keyboard {

RateLimiter::RateLimiter(LiveClock::time_point start, double ratePerSecond, double burst)
    : m_rate(ratePerSecond), m_burst(burst), m_tokens(burst), m_last(start)
{
}

bool RateLimiter::tryTake(LiveClock::time_point now)
{
    if (now > m_last) {
        const double elapsed = std::chrono::duration<double>(now - m_last).count();
        m_tokens = std::min(m_burst, m_tokens + elapsed * m_rate);
        m_last = now;
    }
    if (m_tokens < 1.0)
        return false;
    m_tokens -= 1.0;
    return true;
}

void FreezePolicy::success()
{
    if (!m_tripped)
        m_failures = 0;
}

void FreezePolicy::failure()
{
    if (m_tripped)
        return;
    if (++m_failures >= Threshold)
        m_tripped = true;
}

void FreezePolicy::reset()
{
    m_failures = 0;
    m_tripped = false;
}

} // namespace galleon::keyboard
