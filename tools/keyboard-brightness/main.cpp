// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Manual test tool for lib/keyboard/ (property 0xEA, brightness): reads the current value through
// galleon::HidTransport, and writes a new one (0 to 1000) when given on the command line, reading
// it back to confirm. Close the Web Hub, OpenRGB and iCUE first: another reader steals the
// answers. This is the only place that actually writes to the keyboard from this library; running
// it against real hardware is for the owner to do, not automated here (see AGENTS.md).

#include <galleon/HidTransport.h>
#include <galleon/keyboard/Brightness.h>
#include <galleon/keyboard/KeyboardLink.h>

#include <QString>

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace {

constexpr std::uint16_t VendorId = 0x1b1c;
constexpr std::uint16_t KeyboardProductId = 0x2b0c;
constexpr int VendorInterface = 1; // MI_01, usage page 0xFF42 usage 1 (docs/protocol/README.md)
constexpr auto Timeout = std::chrono::milliseconds(500);

std::optional<std::uint16_t> parseValue(std::string_view text)
{
    unsigned long value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value > 0xffffU)
        return std::nullopt;
    return static_cast<std::uint16_t>(value);
}

} // namespace

int main(int argc, char **argv)
{
    std::optional<std::uint16_t> toWrite;
    if (argc == 2) {
        toWrite = parseValue(argv[1]);
        if (!toWrite || !galleon::keyboard::isValidBrightness(*toWrite)) {
            std::fprintf(stderr, "usage: %s [value]   value is 0 to 1000 (nothing: read only)\n", argv[0]);
            return 1;
        }
    } else if (argc > 2) {
        std::fprintf(stderr, "usage: %s [value]   value is 0 to 1000 (nothing: read only)\n", argv[0]);
        return 1;
    }

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

    using namespace galleon::keyboard;

    if (toWrite) {
        std::printf("writing brightness %u (%d %%)...\n", *toWrite, percentFromBrightness(*toWrite));
        const auto result = writeBrightness(transport, *toWrite, Timeout);
        if (result.error != LinkError::None) {
            std::printf("write failed: %s (echo status=%u, read back %u)\n", describe(result.error), result.status,
                        result.confirmed);
            return 1;
        }
        std::printf("confirmed: brightness is now %u (%d %%)\n", result.confirmed, percentFromBrightness(result.confirmed));
        return 0;
    }

    const auto result = readBrightness(transport, Timeout);
    if (result.error != LinkError::None) {
        std::printf("read failed: %s\n", describe(result.error));
        return 1;
    }
    std::printf("brightness: %u (%d %%), status=%u\n", result.value, percentFromBrightness(result.value), result.status);
    return 0;
}
