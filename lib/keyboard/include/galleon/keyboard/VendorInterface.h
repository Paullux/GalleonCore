// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <galleon/Transport.h>

#include <memory>

namespace galleon::keyboard {

// The keyboard's vendor command interface (part 1b1c:2b0c, interface 1: MI_01 on Windows), opened through
// galleon::HidTransport, or null when it cannot be opened. It is opened only if it really is the vendor
// collection (usage page 0xFF42, usage 1); anything else is closed and refused, so nothing is ever sent to
// another interface. The one place that knows these numbers (docs/protocol/README.md); shared by galleond's
// KeyboardController and the watchdog tool.
[[nodiscard]] std::unique_ptr<galleon::Transport> openVendorInterface();

} // namespace galleon::keyboard
