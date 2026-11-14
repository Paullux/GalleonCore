// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/Transport.h>

#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>

namespace galleond {

// A diagnostic of the live lighting, off unless asked for (the variable GALLEON_LIGHTING_LOG is set to 1 in the environment of
// galleond): a text file with one line per report that goes to the keyboard or comes back from it, and one per change of state
// of the lighting. Nothing else is written, nothing is sent: the transport underneath is used exactly as before. A line has
// the time since the file was opened, the direction, the length and the first bytes of the report (the header, the command and
// the start of what it carries: never the whole of a report, and no path, name or user setting). It exists because the
// lighting works on one system and not on another, and the only way to see why is to see what the keyboard answers.
inline constexpr std::size_t TraceBytesPerReport = 16;
inline constexpr std::size_t TraceFileLimit = std::size_t{256} * 1024; // the file stops there: a long run cannot fill a disk

// One line for a report. Pure.
[[nodiscard]] QString traceLine(std::int64_t milliseconds, const char *direction, std::span<const std::uint8_t> report);

// Where the lines go; it takes them from any thread.
class TraceSink
{
public:
    using Write = std::function<void(const QString &)>;
    explicit TraceSink(Write write, std::size_t limit = TraceFileLimit);

    void line(const QString &text); // stops, with a last line saying so, once `limit` bytes were written

private:
    Write m_write;
    std::size_t m_limit;
    std::size_t m_written = 0;
    bool m_full = false;
    std::mutex m_mutex;
};

// A transport that passes everything to another one and says what went through. Reads that return nothing are not told.
class TracedTransport final : public galleon::Transport
{
public:
    using Clock = std::function<std::int64_t()>; // milliseconds since the trace began

    TracedTransport(std::unique_ptr<galleon::Transport> inner, std::shared_ptr<TraceSink> sink, Clock clock);

    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const override { return m_inner->isOpen(); }
    [[nodiscard]] QString serialNumber() const override { return {}; } // never into the log
    int getFeatureReport(std::span<std::uint8_t> report) override { return m_inner->getFeatureReport(report); }
    bool sendFeatureReport(std::span<const std::uint8_t> report) override;
    bool write(std::span<const std::uint8_t> report) override;
    int read(std::span<std::uint8_t> buffer) override;

private:
    std::unique_ptr<galleon::Transport> m_inner;
    std::shared_ptr<TraceSink> m_sink;
    Clock m_clock;
};

// The file of this run, in the logs folder of the user (the same folder as the HID diagnostic); nothing when it cannot be made.
[[nodiscard]] std::shared_ptr<TraceSink> openLightingTrace();

} // namespace galleond
