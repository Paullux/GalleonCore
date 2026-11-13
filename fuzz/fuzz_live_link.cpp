// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <galleon/Transport.h>
#include <galleon/keyboard/LiveCommandGuard.h>
#include <galleon/keyboard/LiveFrame.h>
#include <galleon/keyboard/LiveLink.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

using namespace galleon::keyboard;

namespace {

// A keyboard that says whatever the fuzzer wants: the input is cut into answers that come back, in order, for
// whatever is written. Nothing it says can be trusted, which is what the live link has to cope with.
class ScriptedKeyboard final : public galleon::Transport
{
public:
    explicit ScriptedKeyboard(std::span<const std::uint8_t> script) : m_script(script) {}

    bool open() override { return true; }
    void close() override {}
    [[nodiscard]] bool isOpen() const override { return true; }
    [[nodiscard]] QString serialNumber() const override { return QString(); }
    bool sendFeatureReport(std::span<const std::uint8_t> /*report*/) override { return false; }

    bool write(std::span<const std::uint8_t> report) override
    {
        sent.emplace_back(report.begin(), report.end());
        // The next piece of the script is the answer: a length byte, then that many bytes (the rest is zeros).
        if (m_at < m_script.size()) {
            const std::size_t length = std::min<std::size_t>(m_script[m_at] % (PayloadSize + 1), m_script.size() - m_at - 1);
            std::vector<std::uint8_t> answer(PayloadSize, 0);
            std::copy_n(m_script.begin() + static_cast<std::ptrdiff_t>(m_at) + 1, length, answer.begin());
            // Half of the time the answer echoes the command asked: the other half is anything.
            if (m_at % 2 == 0)
                answer[3] = report[4];
            m_at += 1 + length;
            m_answers.push_back(std::move(answer));
        }
        return true;
    }

    int read(std::span<std::uint8_t> buffer) override
    {
        if (m_answers.empty())
            return 0;
        const auto next = m_answers.front();
        m_answers.pop_front();
        std::copy_n(next.begin(), std::min(next.size(), buffer.size()), buffer.begin());
        return static_cast<int>(next.size());
    }

    std::vector<std::vector<std::uint8_t>> sent;

private:
    std::span<const std::uint8_t> m_script;
    std::size_t m_at = 0;
    std::deque<std::vector<std::uint8_t>> m_answers;
};

[[noreturn]] void forbidden() { __builtin_trap(); }

// Whatever the keyboard said, only these shapes may have left: the five kinds of report of the allowlist, and for a set
// only the mode, to 1 or 2.
void checkWhatWasSent(const std::vector<std::vector<std::uint8_t>> &sent)
{
    for (const auto &report : sent) {
        if (report.size() != WriteSize)
            forbidden();
        switch (report[4]) {
        case CommandSessionStart: case CommandGet: case CommandPing: case CommandUnbind:
        case CommandWriteBegin: case CommandWriteMore:
            break;
        case CommandSet:
            if (report[5] != PropertyMode || (report[7] != 1 && report[7] != 2) || report[8] != 0)
                forbidden();
            break;
        case CommandOpenFile:
            if (report[5] != 0 || report[6] != 0x22 || report[7] != 0)
                forbidden();
            break;
        default:
            forbidden();
        }
    }
}

} // namespace

// Run: ./fuzz_live_link -max_total_time=60
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < 2)
        return 0;
    // The first bytes drive what the caller does; the rest is what the keyboard says.
    const std::size_t steps = std::min<std::size_t>(data[0] % 24 + 1, size - 1);
    const std::span<const std::uint8_t> script(data + 1 + steps, size - 1 - steps);
    ScriptedKeyboard keyboard(script);
    LiveLink::Settings settings;
    settings.timeout = std::chrono::milliseconds(1);
    LiveLink link(keyboard, settings);
    const auto start = LiveClock::now();
    auto now = start;
    link.begin(now);
    const LiveFrame frame{};
    for (std::size_t i = 0; i < steps; ++i) {
        now += std::chrono::milliseconds(data[1 + i] % 8 * 400); // up to ~3 s: the 3 s, 1 s and 60 s thresholds are reachable
        switch (data[1 + i] / 8 % 5) {
        case 0: case 1: link.tick(now); break;
        case 2: static_cast<void>(link.show(frame, now)); break;
        case 3: static_cast<void>(link.release(now)); break;
        default: link.resetAfterReplug(); link.begin(now); break;
        }
    }
    checkWhatWasSent(keyboard.sent);
    return 0;
}
