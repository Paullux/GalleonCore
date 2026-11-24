// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeyPressSource.h"

#include <QSocketNotifier>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>

namespace galleond {

// True when a HID report descriptor opens with a Generic Desktop Keyboard application (05 01 09 06): the node that carries the keys. Pure. The four
// descriptors of the keyboard, as the owner's tool printed them, are in docs/protocol/README.md ("Keyboard input reports") and in the test.
[[nodiscard]] bool looksLikeKeyboardDescriptor(std::span<const std::uint8_t> descriptor);

// The Linux source: reads the hidraw node of the keyboard's key interface, READ-ONLY (O_RDONLY, no write, no exclusive grab: the system keeps receiving every
// key), while it is started. udev already lets the person's session open it (tools/udev/70-galleon.rules, `uaccess` on every hidraw node of 1b1c:2b0c), so no
// other permission is needed. Reports are read without blocking and turned into usages with galleon::keyboard::keysDown; the bytes of a report are cleared as
// soon as they have been used.
class HidrawKeyPressSource : public KeyPressSource
{
    Q_OBJECT

public:
    // Opens the node and returns its file descriptor (owned by the source from then on), or -1. The default finds the keyboard's hidraw node; a test gives a
    // socket pair that keeps report boundaries.
    using Opener = std::function<int()>;
    explicit HidrawKeyPressSource(Opener opener = defaultOpener(), QObject *parent = nullptr);
    ~HidrawKeyPressSource() override;
    HidrawKeyPressSource(const HidrawKeyPressSource &) = delete;
    HidrawKeyPressSource &operator=(const HidrawKeyPressSource &) = delete;

    void start() override;
    void stop() override;
    [[nodiscard]] bool watching() const override { return m_notifier != nullptr; }

    // The first hidraw node of 1b1c:2b0c whose descriptor is a keyboard, opened read-only and non-blocking; -1 when there is none or it cannot be opened.
    [[nodiscard]] static int openKeyboardNode();
    [[nodiscard]] static Opener defaultOpener();

    // At most this many reports are read each time the descriptor is ready, so that a flood cannot keep the loop of galleond busy; the rest is read at once on
    // the next turn.
    static constexpr int ReportsPerWakeup = 64;

private:
    void onReadable();
    void release(); // closes the node; not virtual, so that the destructor can call it
    void fail();

    Opener m_opener;
    int m_fd = -1;
    std::unique_ptr<QSocketNotifier> m_notifier;
    bool m_warnedShape = false; // one warning about a report of another shape, for each watch
};

} // namespace galleond
