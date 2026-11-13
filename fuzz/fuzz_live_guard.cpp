// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <galleon/keyboard/LiveCommandGuard.h>
#include <galleon/keyboard/LiveFrame.h>

#include <cstddef>
#include <cstdint>
#include <span>

using namespace galleon::keyboard;

// Run: ./fuzz_live_guard -max_total_time=60
// Whatever the bytes, a report the allowlist lets through must be one of the few shapes it documents:
// the guard is the last check before the device, so a hole here is a hole in the safety of the keyboard.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    LiveCommandGuard guard;
    // The input is cut into reports so that sequences (open, write, unbind) are exercised too; a short tail
    // is checked as it is, which must be refused for its length.
    for (std::size_t at = 0; at < size; at += WriteSize) {
        const std::span<const std::uint8_t> report(data + at, std::min(WriteSize, size - at));
        if (guard.check(report) != Refusal::None) {
            guard.confirmed(report); // must be a no-op for a refused report
            continue;
        }
        if (report.size() != WriteSize)
            __builtin_trap();
        switch (report[4]) {
        case CommandSet: {
            const unsigned value = report[7] | (static_cast<unsigned>(report[8]) << 8U);
            if (report[5] != PropertyMode || (value != 1 && value != 2))
                __builtin_trap();
            break;
        }
        case CommandOpenFile:
            if (report[5] != 0 || report[6] != 0x22 || report[7] != 0)
                __builtin_trap();
            break;
        case CommandWriteBegin:
        case CommandWriteMore:
            if (!guard.fileOpen() || report[5] != 0)
                __builtin_trap();
            break;
        case CommandSessionStart:
        case CommandGet:
        case CommandPing:
        case CommandUnbind:
            break;
        default:
            __builtin_trap(); // any other command let through is a hole
        }
        guard.confirmed(report);
        if (guard.bytesExpected() > LiveFrameSize)
            __builtin_trap();
    }
    return 0;
}
