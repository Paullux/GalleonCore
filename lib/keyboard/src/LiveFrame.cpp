// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveFrame.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace galleon::keyboard {

std::optional<LiveFrame> buildLiveFrame(std::span<const LedColor> leds)
{
    if (leds.empty() || std::ranges::any_of(leds, [](const LedColor &led) { return led.led >= LiveLedCount; }))
        return std::nullopt;
    LiveFrame frame{};
    frame.at(0) = 0x12; // frame header `12 00`, from the capture in docs/protocol/README.md
    for (const LedColor &led : leds) {
        const std::size_t at = 2 + 3 * static_cast<std::size_t>(led.led);
        frame.at(at) = led.red;
        frame.at(at + 1) = led.green;
        frame.at(at + 2) = led.blue;
    }
    return frame;
}

std::optional<LiveReports> liveFrameReports(const LiveFrame &frame, std::uint8_t session)
{
    if (session == 0)
        return std::nullopt;
    LiveReports reports{};
    std::size_t sent = 0;
    for (std::size_t i = 0; i < LiveReportCount; ++i) {
        auto &report = reports.at(i);
        report[2] = 0x01; // request
        report[3] = session;
        const bool first = i == 0;
        report[4] = first ? CommandWriteBegin : CommandWriteMore;
        report[5] = 0x00; // buffer 0
        std::size_t dataAt = 6;
        if (first) {
            for (std::size_t b = 0; b < 4; ++b)
                report.at(6 + b) = static_cast<std::uint8_t>((LiveFrameSize >> (8U * b)) & 0xffU);
            dataAt = 10;
        }
        const std::size_t room = first ? WriteBeginData : WriteMoreData;
        const std::size_t count = std::min(room, LiveFrameSize - sent);
        std::copy_n(frame.begin() + static_cast<std::ptrdiff_t>(sent), count, report.begin() + static_cast<std::ptrdiff_t>(dataAt));
        sent += count;
    }
    return reports;
}

} // namespace galleon::keyboard
