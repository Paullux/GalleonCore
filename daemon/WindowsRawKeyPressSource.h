// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeyPressSource.h"

#include <memory>

namespace galleond {

struct RawKeyWindow; // the native window and what it keeps: out of this header so that it does not pull <windows.h>

// The Windows source of key presses for the key glow (KeyPressSource.h), by Raw Input: the keyboard events are asked of Windows for a window that is never shown, only
// between start() and stop(), and only the events of the Galleon keyboard are read.
//   - NO low-level keyboard hook (WH_KEYBOARD_LL, the way keyloggers listen): Raw Input is registered for the usage page of the keyboards, with RIDEV_INPUTSINK so that
//     the events come while another program has the focus, and WITHOUT RIDEV_NOLEGACY, so the keys still reach the program that has the focus as before. Nothing is
//     blocked, changed or injected.
//   - A message is first read by its HEADER ONLY, which says which device sent it: the key of any other keyboard of the computer (the laptop's, the one of a friend) is
//     never read at all. Only the Galleon keyboard's message is read further (RIDEV scan code and flags), at once turned into a usage, and the structure is cleared.
//   - Between stop() and start() nothing is registered: Windows sends nothing to this process. stop() unregisters before anything else.
//   - The window is a top-level one that is never shown, as the one of WindowsPowerEvents is; it must live on the thread of the event loop, which delivers the messages.
// Not seen on Windows hardware yet: the glow is not validated there until the owner has tried it (docs/KEY_GLOW.md). A security product may take a program that listens to
// the keyboard for a spy: this one does only while the person chose the effect.
class WindowsRawKeyPressSource final : public KeyPressSource
{
    Q_OBJECT

public:
    explicit WindowsRawKeyPressSource(QObject *parent = nullptr);
    ~WindowsRawKeyPressSource() override;
    WindowsRawKeyPressSource(const WindowsRawKeyPressSource &) = delete;
    WindowsRawKeyPressSource &operator=(const WindowsRawKeyPressSource &) = delete;

    void start() override;
    void stop() override;
    [[nodiscard]] bool watching() const override { return m_watching; }

    // For the tests: the native window (an HWND), or null when there is none.
    [[nodiscard]] void *windowForTests() const;
    // At most this many devices are remembered as accepted or refused (by their handle, not by what they sent).
    static constexpr int MaxKnownDevices = 32;

private:
    void release();

    std::unique_ptr<RawKeyWindow> m_window;
    bool m_watching = false;
};

} // namespace galleond
