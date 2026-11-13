// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A fake keyboard that answers like the real one (a set echo, then the value on a get), for the tests of the
// daemon's keyboard side. Never opens real hardware: every opener here returns a fake.

#include "KeyboardController.h"

#include <galleon/Transport.h>
#include <galleon/keyboard/BragiFrame.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <span>

namespace fakekeyboard {

using namespace galleon::keyboard;

// State shared with the worker thread, which owns the transport for one job.
struct Keyboard
{
    std::mutex mutex;
    std::vector<int> setValues; // every value written, in order
    int opens = 0;
    int closes = 0;
    int shownValue = 500;       // what the keyboard currently holds
    bool answersSet = true;
    bool available = true;
};

class FakeKeyboard final : public galleon::Transport
{
public:
    explicit FakeKeyboard(std::shared_ptr<Keyboard> state) : m_state(std::move(state)) {}

    bool open() override { m_open = true; return true; }
    void close() override
    {
        if (m_open) {
            const std::scoped_lock lock(m_state->mutex);
            ++m_state->closes;
        }
        m_open = false;
    }
    [[nodiscard]] bool isOpen() const override { return m_open; }
    [[nodiscard]] QString serialNumber() const override { return QStringLiteral("FAKE"); }
    bool sendFeatureReport(std::span<const std::uint8_t> /*report*/) override { return false; }

    bool write(std::span<const std::uint8_t> report) override
    {
        const std::scoped_lock lock(m_state->mutex);
        std::vector<std::uint8_t> answer(PayloadSize, 0);
        if (report.size() != WriteSize)
            return false;
        answer[3] = report[4];
        if (report[4] == CommandSet) {
            m_state->shownValue = report[7] | (report[8] << 8);
            m_state->setValues.push_back(m_state->shownValue);
            answer[4] = 0;
        } else {
            answer[4] = 0;
            for (std::size_t i = 0; i < 4; ++i)
                answer[5 + i] = static_cast<std::uint8_t>((m_state->shownValue >> (8U * i)) & 0xff);
        }
        if (report[4] != CommandSet || m_state->answersSet)
            m_answers.push_back(answer);
        return true;
    }

    int read(std::span<std::uint8_t> buffer) override
    {
        if (m_answers.empty())
            return 0;
        const auto answer = m_answers.front();
        m_answers.pop_front();
        std::copy_n(answer.begin(), std::min(answer.size(), buffer.size()), buffer.begin());
        return static_cast<int>(answer.size());
    }

private:
    std::shared_ptr<Keyboard> m_state;
    std::deque<std::vector<std::uint8_t>> m_answers;
    bool m_open = false;
};

inline galleond::KeyboardController::Opener openerFor(const std::shared_ptr<Keyboard> &state)
{
    return [state]() -> std::unique_ptr<galleon::Transport> {
        {
            const std::scoped_lock lock(state->mutex);
            if (!state->available)
                return nullptr;
            ++state->opens;
        }
        auto fake = std::make_unique<FakeKeyboard>(state);
        fake->open();
        return fake;
    };
}


} // namespace fakekeyboard
