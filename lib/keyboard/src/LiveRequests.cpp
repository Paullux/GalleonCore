// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/LiveRequests.h"

#include "galleon/keyboard/LiveCommandGuard.h"

namespace galleon::keyboard {

namespace {

Request base(std::uint8_t session, std::uint8_t command)
{
    Request request{};
    request[2] = 0x01; // request marker
    request[3] = session;
    request[4] = command;
    return request;
}

} // namespace

Request sessionStartRequest(const std::array<std::uint8_t, 4> &random)
{
    Request request = base(0, CommandSessionStart);
    request[5] = 0x01;
    for (std::size_t i = 0; i < random.size(); ++i)
        request.at(6 + i) = random.at(i);
    return request;
}

Request getPropertyIn(std::uint8_t session, std::uint8_t property)
{
    Request request = base(session, CommandGet);
    request[5] = property;
    return request;
}

Request setModeRequest(std::uint8_t session, std::uint8_t mode)
{
    Request request = base(session, CommandSet);
    request[5] = PropertyMode;
    request[7] = mode; // value, 16 bits little-endian: the high byte stays 0
    return request;
}

Request unbindRequest(std::uint8_t session)
{
    Request request = base(session, CommandUnbind);
    request[5] = 0x01; // as in the capture; its meaning is not established beyond it
    return request;
}

Request pingRequest(std::uint8_t session)
{
    return base(session, CommandPing);
}

Request openLiveFileRequest(std::uint8_t session)
{
    Request request = base(session, CommandOpenFile);
    request[6] = static_cast<std::uint8_t>(LiveFileId & 0xffU);
    request[7] = static_cast<std::uint8_t>((LiveFileId >> 8U) & 0xffU);
    return request;
}

} // namespace galleon::keyboard
