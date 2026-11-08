// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
//
// READ-ONLY: reads the firmware (property 0x13) and the brightness (property 0xEA) of the
// keyboard through galleon::HidTransport, to check on Linux and on Windows that the same class
// the daemon uses carries the exchange. See KeyboardReadProbe.h. Close the Web Hub, OpenRGB and
// iCUE first: another reader steals the answers.

#include "KeyboardReadProbe.h"

#include <galleon/HidTransport.h>

#include <QString>

#include <chrono>
#include <cstdio>

namespace {

constexpr std::uint16_t VendorId = 0x1b1c;
constexpr std::uint16_t KeyboardProductId = 0x2b0c;
constexpr int VendorInterface = 1; // MI_01, usage page 0xFF42 usage 1 (docs/protocol/README.md)
constexpr auto Timeout = std::chrono::milliseconds(500);

void printRaw(const std::vector<std::uint8_t> &raw)
{
    std::printf(" raw(%zu bytes)", raw.size());
    for (std::size_t i = 0; i < raw.size() && i < 12; ++i)
        std::printf(" %02x", raw[i]);
}

bool readAndPrint(galleon::HidTransport &transport, std::uint8_t property)
{
    const auto result = keyboardprobe::readProperty(transport, property, Timeout);
    std::printf("prop 0x%02x:", property);
    if (result.error != keyboardprobe::ReadError::None) {
        std::printf(" %s (discarded %d stale, %d unexpected)\n", keyboardprobe::describe(result.error),
                    result.staleDiscarded, result.unexpected);
        return false;
    }
    printRaw(result.raw);
    std::printf("  status=%u value=0x%x (%u)", result.reply.status, result.reply.value, result.reply.value);
    if (result.reply.status == 0 && property == keyboardprobe::PropertyFirmware)
        std::printf("  firmware %u.%u.%u", (result.reply.value) & 0xffU, (result.reply.value >> 8) & 0xffU,
                    (result.reply.value >> 16) & 0xffffU);
    if (result.reply.status == 0 && property == keyboardprobe::PropertyBrightness)
        std::printf("  brightness %u %%", result.reply.value / 10U);
    std::printf("  (discarded %d stale, %d unexpected)\n", result.staleDiscarded, result.unexpected);
    return result.reply.status == 0;
}

} // namespace

int main()
{
    galleon::HidTransport transport(VendorId, KeyboardProductId, VendorInterface);
    if (!transport.open()) {
        std::puts("keyboard 1b1c:2b0c, interface 1: cannot open it (plugged in? rights? another program?)");
        return 1;
    }
    const QString collection = transport.openedCollection();
    std::printf("opened %s\n", collection.toUtf8().constData());
    // Interface 1 must be the vendor one: never talk to anything else.
    if (!collection.contains(QStringLiteral("usagePage=0xff42")) || !collection.contains(QStringLiteral("usage=0x0001"))) {
        std::puts("this is not the vendor interface (usage page 0xFF42, usage 1): nothing sent");
        return 1;
    }
    bool allOk = true;
    for (const std::uint8_t property : keyboardprobe::ReadableProperties)
        allOk = readAndPrint(transport, property) && allOk;
    return allOk ? 0 : 1;
}
