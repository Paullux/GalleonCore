// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/VendorInterface.h"

#include <galleon/HidTransport.h>

#include <QString>

#include <cstdint>
#include <memory>

namespace galleon::keyboard {

namespace {
constexpr std::uint16_t VendorId = 0x1b1c;
constexpr std::uint16_t KeyboardProductId = 0x2b0c;
constexpr int VendorInterface = 1; // MI_01, usage page 0xFF42 usage 1 (docs/protocol/README.md)
} // namespace

std::unique_ptr<galleon::Transport> openVendorInterface()
{
    auto transport = std::make_unique<galleon::HidTransport>(VendorId, KeyboardProductId, VendorInterface);
    if (!transport->open())
        return nullptr;
    const QString collection = transport->openedCollection();
    // Interface 1 must be the vendor one: never talk to anything else.
    if (!collection.contains(QStringLiteral("usagePage=0xff42")) || !collection.contains(QStringLiteral("usage=0x0001"))) {
        transport->close();
        return nullptr;
    }
    return transport;
}

} // namespace galleon::keyboard
