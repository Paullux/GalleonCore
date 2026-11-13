// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The watchdog of the keyboard's live lighting: started by the process that writes the frames, with that
// process's ID. It waits for it to end, for any reason, then opens a fresh session and, ONLY if the keyboard is
// still in host mode (property 3 = 2), gives the keys back (unbind, mode 1, read back). If the writer had released
// already it finds mode 1 and changes nothing. All the logic is in lib/keyboard (LiveRelease), tested with a fake
// keyboard; this file only waits and opens the real device. Nothing in this repository starts it yet: there is no
// live writer. Running it against real hardware is for the owner to do (see AGENTS.md).
//
// Exit status: 0 nothing to do or released, 1 bad arguments, 2 the release could not be confirmed, 3 the
// keyboard could not be opened, 4 the process could not be watched (then nothing was sent).

#include "ProcessWait.h"
#include "WatchdogArgs.h"

#include <galleon/keyboard/LiveRelease.h>
#include <galleon/keyboard/VendorInterface.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace {
constexpr auto Timeout = std::chrono::milliseconds(500);
constexpr auto RetryDelay = std::chrono::milliseconds(300);
} // namespace

int main(int argc, char **argv)
{
    const std::span<char *> all(argv, static_cast<std::size_t>(argc));
    std::vector<std::string_view> arguments;
    for (const char *argument : all.subspan(std::min<std::size_t>(1, all.size())))
        arguments.emplace_back(argument);
    const auto pid = watchdog::parseArguments(arguments);
    if (!pid) {
        static_cast<void>(std::fprintf(stderr, "usage: %s --pid <process id of the writer>\n", all.empty() ? "watchdog" : all.front()));
        return 1;
    }

    switch (watchdog::waitForExit(*pid)) {
    case watchdog::WaitResult::CannotWatch:
        static_cast<void>(std::fputs("cannot watch that process: nothing was sent to the keyboard\n", stderr));
        return 4;
    case watchdog::WaitResult::AlreadyGone: // the writer died before this started: still check the keyboard
    case watchdog::WaitResult::Exited:
        break;
    }

    auto transport = galleon::keyboard::openVendorInterface();
    if (!transport) {
        static_cast<void>(std::puts("keyboard 1b1c:2b0c, interface 1: cannot open it (unplugged? another program?)"));
        return 3;
    }
    std::random_device device;
    std::array<std::uint8_t, 4> random{};
    for (auto &byte : random)
        byte = static_cast<std::uint8_t>(device() & 0xffU);

    const auto result = galleon::keyboard::releaseIfHostMode(*transport, random, Timeout, RetryDelay);
    transport->close();
    const std::string before = result.modeBefore ? std::to_string(*result.modeBefore) : "unreadable";
    static_cast<void>(std::printf("%s (attempts: %d, mode before: %s)\n", galleon::keyboard::describe(result.outcome), result.attempts, before.c_str()));
    return result.outcome == galleon::keyboard::ReleaseOutcome::Failed ? 2 : 0;
}
