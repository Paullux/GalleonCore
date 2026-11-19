// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// `count` bytes of `bytes` from `offset`, as a Qt byte array. A copy: no cast of
// the buffer's pointer and no pointer arithmetic. The caller has checked the sizes.
inline QByteArray bytesOf(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t count)
{
    QByteArray out;
    out.reserve(static_cast<qsizetype>(count));
    for (const std::uint8_t byte : bytes.subspan(offset, count))
        out.append(static_cast<char>(byte));
    return out;
}

// A key report as the module sends it: `01 00 0c 00`, then `keyCount` state bytes that all hold `fill`. It is built at its final size on purpose: a
// four byte list that is then resized made GCC 15 warn (-Warray-bounds, a false alarm) in an optimised build with -Werror.
inline std::vector<std::uint8_t> keyReport(std::size_t keyCount, std::uint8_t fill)
{
    std::vector<std::uint8_t> report(4 + keyCount, fill);
    report[0] = 0x01;
    report[1] = 0x00;
    report[2] = 0x0c;
    report[3] = 0x00;
    return report;
}
