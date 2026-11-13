// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveCommandGuard.h"

#include "galleon/keyboard/LiveFrame.h"

#include <algorithm>

namespace galleon::keyboard {
namespace {

constexpr std::size_t CommandAt = 4;
constexpr std::size_t PayloadAt = 5;

bool zeroFrom(std::span<const std::uint8_t> report, std::size_t from)
{
    return std::all_of(report.begin() + static_cast<std::ptrdiff_t>(from), report.end(), [](std::uint8_t b) { return b == 0; });
}

std::uint32_t lengthAt(std::span<const std::uint8_t> report, std::size_t at)
{
    return static_cast<std::uint32_t>(report[at]) | static_cast<std::uint32_t>(report[at + 1]) << 8U
         | static_cast<std::uint32_t>(report[at + 2]) << 16U | static_cast<std::uint32_t>(report[at + 3]) << 24U;
}

} // namespace

const char *describe(Refusal refusal)
{
    switch (refusal) {
    case Refusal::None: return "allowed";
    case Refusal::BadLength: return "not a 129-byte report";
    case Refusal::BadHeader: return "bad report header";
    case Refusal::UnknownCommand: return "command not on the allowlist";
    case Refusal::BadPayload: return "payload differs from the allowed shape";
    case Refusal::PropertyNotAllowed: return "property not allowed";
    case Refusal::ModeNotAllowed: return "operating mode must be 1 or 2";
    case Refusal::FileNotAllowed: return "only file 0x0022 may be opened";
    case Refusal::BufferNotAllowed: return "only buffer 0 may be used";
    case Refusal::NotOpen: return "write while the live file is not open";
    case Refusal::BadFrameLength: return "a live frame is exactly 440 bytes";
    case Refusal::OutOfSequence: return "write out of sequence";
    }
    return "unknown";
}

Refusal LiveCommandGuard::check(std::span<const std::uint8_t> report) const
{
    if (report.size() != WriteSize)
        return Refusal::BadLength;
    if (report[0] != 0x00 || report[1] != 0x00 || report[2] != 0x01)
        return Refusal::BadHeader;
    const std::uint8_t session = report[3];
    switch (report[CommandAt]) {
    case CommandSessionStart:
        // `01`, four random bytes, then zeros; the session byte is 0 because none is open yet.
        return session == 0 && report[PayloadAt] == 0x01 && zeroFrom(report, PayloadAt + 5) ? Refusal::None : Refusal::BadPayload;
    case CommandGet:
        return zeroFrom(report, PayloadAt + 1) ? Refusal::None : Refusal::BadPayload;
    case CommandSet: {
        if (report[PayloadAt] != PropertyMode)
            return Refusal::PropertyNotAllowed;
        if (report[PayloadAt + 1] != 0x00 || !zeroFrom(report, PayloadAt + 4))
            return Refusal::BadPayload;
        const unsigned value = report[PayloadAt + 2] | (static_cast<unsigned>(report[PayloadAt + 3]) << 8U);
        return value == 1 || value == 2 ? Refusal::None : Refusal::ModeNotAllowed;
    }
    case CommandPing:
        return zeroFrom(report, PayloadAt) ? Refusal::None : Refusal::BadPayload;
    case CommandOpenFile: {
        if (report[PayloadAt] != 0x00)
            return Refusal::BufferNotAllowed;
        const unsigned file = report[PayloadAt + 1] | (static_cast<unsigned>(report[PayloadAt + 2]) << 8U);
        if (file != LiveFileId)
            return Refusal::FileNotAllowed;
        return zeroFrom(report, PayloadAt + 3) ? Refusal::None : Refusal::BadPayload;
    }
    case CommandUnbind:
        return report[PayloadAt] == 0x01 && zeroFrom(report, PayloadAt + 1) ? Refusal::None : Refusal::BadPayload;
    case CommandWriteBegin: {
        if (!m_open)
            return Refusal::NotOpen;
        if (report[PayloadAt] != 0x00)
            return Refusal::BufferNotAllowed;
        if (lengthAt(report, PayloadAt + 1) != LiveFrameSize)
            return Refusal::BadFrameLength;
        return m_expected == 0 ? Refusal::None : Refusal::OutOfSequence;
    }
    case CommandWriteMore: {
        if (!m_open)
            return Refusal::NotOpen;
        if (report[PayloadAt] != 0x00)
            return Refusal::BufferNotAllowed;
        if (m_expected == 0)
            return Refusal::OutOfSequence;
        // The bytes after the last frame byte are padding and must be zero.
        const std::size_t used = std::min(WriteMoreData, m_expected);
        return zeroFrom(report, PayloadAt + 1 + used) ? Refusal::None : Refusal::BadPayload;
    }
    default:
        return Refusal::UnknownCommand;
    }
}

void LiveCommandGuard::confirmed(std::span<const std::uint8_t> report)
{
    if (check(report) != Refusal::None)
        return; // never move on a report the guard would not have let through
    switch (report[CommandAt]) {
    case CommandOpenFile: m_open = true; break;
    case CommandUnbind: reset(); break;
    case CommandWriteBegin: m_expected = LiveFrameSize - WriteBeginData; break;
    case CommandWriteMore: m_expected -= std::min(WriteMoreData, m_expected); break;
    default: break;
    }
}

void LiveCommandGuard::reset()
{
    m_open = false;
    m_expected = 0;
}

} // namespace galleon::keyboard
