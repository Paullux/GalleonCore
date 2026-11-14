// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PowerMessages.h"

namespace galleond {
namespace {

// From the Windows SDK, winuser.h.
constexpr std::uint32_t WmPowerBroadcast = 0x0218;
constexpr std::uintptr_t PbtApmSuspend = 0x0004;
constexpr std::uintptr_t PbtApmResumeSuspend = 0x0007;
constexpr std::uintptr_t PbtApmResumeAutomatic = 0x0012;
constexpr std::uint32_t WmEndSession = 0x0016;

} // namespace

PowerChange powerChangeFor(std::uint32_t message, std::uintptr_t wParam)
{
    if (message == WmPowerBroadcast) {
        if (wParam == PbtApmSuspend)
            return PowerChange::GoingToSleep;
        if (wParam == PbtApmResumeAutomatic || wParam == PbtApmResumeSuspend)
            return PowerChange::Resumed;
        return PowerChange::None;
    }
    // WM_QUERYENDSESSION (0x0011) is only a question and may be refused by another program: the keys are given back when
    // the session really ends, which is WM_ENDSESSION with wParam TRUE.
    if (message == WmEndSession && wParam != 0)
        return PowerChange::GoingToSleep;
    return PowerChange::None;
}

} // namespace galleond
