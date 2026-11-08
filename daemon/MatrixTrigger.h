// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/Protocol.h>

#include <QtGlobal>

#include <array>
#include <cstdint>

namespace galleond {

// Decides when the "green letter rain" easter egg starts and stops. Pure: no timer, no I/O, no
// clock of its own; the caller gives the time of each event and asks again at the deadline.
//
// Start: both dial buttons held together for `holdMs` (5 s). Releasing either one before that,
// turning a dial or pressing a key while holding, cancels it: the hold must be clean. It fires
// once per hold; a new hold needs a fresh press of the second button.
// Stop: while it runs, any key, dial rotation or dial press stops it. The release of the two
// dials that started it is ignored (the fingers are still on them), once each.
class MatrixTrigger
{
public:
    static constexpr qint64 DefaultHoldMs = 5000;

    enum class Kind : std::uint8_t { KeyPressed, KeyReleased, DialPressed, DialReleased, DialRotated };
    enum class Action : std::uint8_t { None, Start, Stop };

    explicit MatrixTrigger(qint64 holdMs = DefaultHoldMs) : m_holdMs(holdMs) {}

    // `index`: key 0..11 or dial 0..1. An index out of range is dropped (no change).
    Action onEvent(Kind kind, int index, qint64 nowMs);
    // Call at the deadline (or any later time): Start once the hold is long enough.
    Action tick(qint64 nowMs);
    // Milliseconds until the hold is long enough; -1 when no hold is pending.
    [[nodiscard]] qint64 msUntilStart(qint64 nowMs) const;

    [[nodiscard]] bool isRunning() const { return m_running; }
    // The device went away or the daemon stops: forget everything.
    void reset();

private:
    std::array<bool, galleon::protocol::DialCount> m_down{};
    std::array<bool, galleon::protocol::DialCount> m_ignoreRelease{};
    bool m_armed = false;
    bool m_running = false;
    qint64 m_armedAt = 0;
    qint64 m_holdMs;
};

} // namespace galleond
