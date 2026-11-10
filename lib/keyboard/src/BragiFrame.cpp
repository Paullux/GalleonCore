// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/BragiFrame.h"

namespace galleon::keyboard {

std::array<std::uint8_t, WriteSize> getPropertyRequest(std::uint8_t property)
{
    std::array<std::uint8_t, WriteSize> request{};
    request[2] = 0x01;
    request[4] = CommandGet;
    request[5] = property;
    return request;
}

std::array<std::uint8_t, WriteSize> setPropertyRequest(std::uint8_t property, std::uint16_t value)
{
    std::array<std::uint8_t, WriteSize> request{};
    request[2] = 0x01;
    request[4] = CommandSet;
    request[5] = property;
    // request[6] is the reserved byte the Web Hub always sends as 00 (see BragiFrame.h).
    request[7] = static_cast<std::uint8_t>(value & 0xffU);
    request[8] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    return request;
}

} // namespace galleon::keyboard
