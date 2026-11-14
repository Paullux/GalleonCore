// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace galleond {

// What a window message of Windows means for the live lighting: the keys must be given back before the machine sleeps or the
// session ends, and taken again after it woke. Pure, so that it is tested on any system; WindowsPowerEvents does the
// listening.
enum class PowerChange : std::uint8_t {
    None,
    GoingToSleep, // give the keys back now (sleep, hibernation, or the session ends: the process may be killed once this returns)
    Resumed       // the machine is awake again
};

// The numbers are those of the Windows SDK (winuser.h): WM_POWERBROADCAST 0x0218 with PBT_APMSUSPEND 0x0004,
// PBT_APMRESUMESUSPEND 0x0007 and PBT_APMRESUMEAUTOMATIC 0x0012 in wParam; WM_ENDSESSION 0x0016 with a non-zero wParam when the
// session really ends (a zero wParam says it was cancelled after WM_QUERYENDSESSION).
[[nodiscard]] PowerChange powerChangeFor(std::uint32_t message, std::uintptr_t wParam);

} // namespace galleond
