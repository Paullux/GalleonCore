// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

// A fake keyboard that behaves like the one of the capture in docs/protocol/README.md "Live per-key lighting
// (Windows, 2026-10-25)": a mode, a session, a live file that must be open, frames that arrive in four reports, and a
// Web Hub that can speak or take the mode. Shared by the tests of the live lighting link and of galleond's lighting
// controller. Never opens real hardware, and nothing here proves the behaviour of a real keyboard.

#include <galleon/Transport.h>
#include <galleon/keyboard/LiveCommandGuard.h>
#include <galleon/keyboard/LiveFrame.h>
#include <galleon/keyboard/LiveRequests.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

namespace fakelive {

using namespace galleon::keyboard;

class FakeDevice final : public galleon::Transport
{
public:
    // What the keyboard holds.
    std::uint8_t mode = 1;
    std::uint8_t sessionId = 1;
    bool fileOpen = false;
    int framesComplete = 0;
    std::vector<std::uint8_t> lastFrame;

    // How it misbehaves.
    bool silentFrames = false;   // the frame reports (06, 07) get no answer
    int silentFramesTimes = -1;  // ... only this many times (-1: always while silentFrames)
    int frameReportsBeforeSilence = -1; // answer this many frame reports, then leave ONE unanswered (-1: not used)
    bool unreadableMode = false; // the answer to a read of the mode carries a value that cannot be a mode
    bool expireNext = false;     // the next answer says the session expired
    bool disconnected = false;
    bool spam = false;           // an answer nobody asked for, every time it is read

    // Called with the command of each report that was answered, once its answer is queued: lets a test make another host
    // speak at an exact point of a sequence.
    std::function<void(std::uint8_t)> afterCommand;

    std::vector<std::vector<std::uint8_t>> sent;
    // Taken by write() and read(). A test that looks at the fields while another thread drives the device takes it too.
    mutable std::recursive_mutex mutex;

    bool open() override { return true; }
    void close() override {}
    [[nodiscard]] bool isOpen() const override { return true; }
    [[nodiscard]] QString serialNumber() const override { return QStringLiteral("FAKE"); }
    bool sendFeatureReport(std::span<const std::uint8_t> /*report*/) override { return false; }

    // Another host speaks: an answer to one of ITS commands (the Web Hub polls the keyboard).
    void otherHostSpeaks() { const std::scoped_lock lock(mutex); m_answers.push_back(answer(CommandGet, 0)); }
    // The Web Hub connects and takes the keyboard back to its own mode.
    void webHubTakes() { const std::scoped_lock lock(mutex); mode = 1; fileOpen = false; }

    bool write(std::span<const std::uint8_t> report) override
    {
        const std::scoped_lock lock(mutex);
        sent.emplace_back(report.begin(), report.end());
        const std::uint8_t command = report[4];
        const bool isFrame = command == CommandWriteBegin || command == CommandWriteMore;
        if (isFrame && mode == 2 && fileOpen)
            collect(report);
        if (isFrame && frameReportsBeforeSilence >= 0) {
            if (frameReportsBeforeSilence == 0) {
                frameReportsBeforeSilence = -1;
                return true; // this one goes unanswered
            }
            --frameReportsBeforeSilence;
        }
        if (isFrame && silentFrames && silentFramesTimes != 0) {
            if (silentFramesTimes > 0)
                --silentFramesTimes;
            return true;
        }
        std::vector<std::uint8_t> reply = answer(command, 0);
        if (command == CommandSessionStart) {
            for (std::size_t i = 0; i < 4; ++i)
                reply[5 + i] = report[6 + i];
            reply[9] = sessionId;
        } else if (command == CommandGet) {
            reply[5] = unreadableMode ? 0xff : mode;
            reply[6] = unreadableMode ? 0xff : 0; // a value above 255: not a mode
        } else if (command == CommandSet) {
            mode = report[7];
        } else if (command == CommandOpenFile) {
            fileOpen = true;
        } else if (command == CommandUnbind) {
            fileOpen = false;
        }
        if (expireNext) {
            expireNext = false;
            reply[1] = 15;
        }
        m_answers.push_back(reply);
        if (afterCommand)
            afterCommand(command);
        return true;
    }

    int read(std::span<std::uint8_t> buffer) override
    {
        const std::scoped_lock lock(mutex);
        if (disconnected)
            return -1;
        if (spam)
            m_answers.push_back(answer(CommandGet, 0));
        if (m_answers.empty())
            return 0;
        const auto next = m_answers.front();
        m_answers.pop_front();
        std::copy_n(next.begin(), std::min(next.size(), buffer.size()), buffer.begin());
        return static_cast<int>(next.size());
    }

    [[nodiscard]] std::vector<std::uint8_t> commands() const
    {
        const std::scoped_lock lock(mutex);
        std::vector<std::uint8_t> list;
        list.reserve(sent.size());
        for (const auto &report : sent)
            list.push_back(report[4]);
        return list;
    }

private:
    static std::vector<std::uint8_t> answer(std::uint8_t command, std::uint8_t status)
    {
        std::vector<std::uint8_t> reply(PayloadSize, 0);
        reply[3] = command;
        reply[4] = status;
        return reply;
    }

    void collect(std::span<const std::uint8_t> report)
    {
        if (report[4] == CommandWriteBegin) {
            m_partial.assign(report.begin() + 10, report.end());
        } else {
            m_partial.insert(m_partial.end(), report.begin() + 6, report.end());
        }
        if (m_partial.size() >= LiveFrameSize) {
            lastFrame.assign(m_partial.begin(), m_partial.begin() + LiveFrameSize);
            ++framesComplete;
            m_partial.clear();
        }
    }

    std::deque<std::vector<std::uint8_t>> m_answers;
    std::vector<std::uint8_t> m_partial;
};


} // namespace fakelive
