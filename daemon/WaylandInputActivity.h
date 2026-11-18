// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "InputActivity.h"

#include <cstdint>
#include <memory>

namespace galleond {

// Says that "something was done at the computer" on a Wayland session, with the compositor's idle notification (the protocol ext-idle-notify-v1: KDE Plasma
// has it, where the idle time is not given by D-Bus). While it watches, the compositor says when the session has been idle for a short time and when it is
// used again; that second message is the action. The compositor does not say which input it was (the mouse or the keyboard), which key or button, or where,
// and nothing is kept: this is only the same "idle then used again" that the screen locker uses.
//
// Nothing is observed unless start() was called: the notification object is created by start() and destroyed by stop() and by the action itself, so that
// the compositor watches nothing for us in between (the effect that uses this is off by default). The connection is to the user's own session
// (WAYLAND_DISPLAY), it asks for no privilege, and it answers to the event loop of the daemon without ever blocking it.
class WaylandInputActivity : public InputActivitySource
{
    Q_OBJECT

public:
    // The compositor says "idle" after this long without an action: an action in the first moments of a watch is missed, the next ones are seen.
    static constexpr std::uint32_t TimeoutMs = 250;

    // Connects to the compositor (`socketName`: its socket, nothing for the one of the session). Nothing when there is none to talk to.
    [[nodiscard]] static std::unique_ptr<WaylandInputActivity> connectToSession(const char *socketName = nullptr);
    ~WaylandInputActivity() override;

    void start() override;
    void stop() override;
    [[nodiscard]] bool watching() const override;

    // False once it is known that the compositor has no idle notification or no seat (it is known a moment after connecting; true until then).
    [[nodiscard]] bool supported() const;

private:
    class Impl;
    explicit WaylandInputActivity(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> m_impl;
};

} // namespace galleond
