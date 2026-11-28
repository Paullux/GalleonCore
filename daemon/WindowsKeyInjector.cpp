// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyInjector.h"
#include "PlatformBackends.h"

#include <Windows.h>

namespace galleond {
namespace {

bool sendWindowsKey(KeyCode key, bool down)
{
    // Virtual-key constants and MapVirtualKeyW are documented in Winuser.h.
    // Use scan codes for physical keypad semantics, including Num Lock state.
    UINT virtualKey = 0;
    switch (key) {
    case KeyCode::Kp0: virtualKey = VK_NUMPAD0; break;
    case KeyCode::Kp1: virtualKey = VK_NUMPAD1; break;
    case KeyCode::Kp2: virtualKey = VK_NUMPAD2; break;
    case KeyCode::Kp3: virtualKey = VK_NUMPAD3; break;
    case KeyCode::Kp4: virtualKey = VK_NUMPAD4; break;
    case KeyCode::Kp5: virtualKey = VK_NUMPAD5; break;
    case KeyCode::Kp6: virtualKey = VK_NUMPAD6; break;
    case KeyCode::Kp7: virtualKey = VK_NUMPAD7; break;
    case KeyCode::Kp8: virtualKey = VK_NUMPAD8; break;
    case KeyCode::Kp9: virtualKey = VK_NUMPAD9; break;
    case KeyCode::KpDot: virtualKey = VK_DECIMAL; break;
    case KeyCode::KpEnter: virtualKey = VK_RETURN; break;
    case KeyCode::KpPlus: virtualKey = VK_ADD; break;
    case KeyCode::KpMinus: virtualKey = VK_SUBTRACT; break;
    case KeyCode::KpAsterisk: virtualKey = VK_MULTIPLY; break;
    case KeyCode::KpSlash: virtualKey = VK_DIVIDE; break;
    case KeyCode::NumLock: virtualKey = VK_NUMLOCK; break;
    case KeyCode::Backspace: virtualKey = VK_BACK; break;
    case KeyCode::F13: virtualKey = VK_F13; break;
    case KeyCode::F14: virtualKey = VK_F14; break;
    case KeyCode::F15: virtualKey = VK_F15; break;
    case KeyCode::F16: virtualKey = VK_F16; break;
    case KeyCode::F17: virtualKey = VK_F17; break;
    case KeyCode::F18: virtualKey = VK_F18; break;
    case KeyCode::F19: virtualKey = VK_F19; break;
    case KeyCode::F20: virtualKey = VK_F20; break;
    case KeyCode::F21: virtualKey = VK_F21; break;
    case KeyCode::F22: virtualKey = VK_F22; break;
    case KeyCode::F23: virtualKey = VK_F23; break;
    case KeyCode::F24: virtualKey = VK_F24; break;
    case KeyCode::F1: virtualKey = VK_F1; break;
    case KeyCode::F2: virtualKey = VK_F2; break;
    case KeyCode::F3: virtualKey = VK_F3; break;
    case KeyCode::F4: virtualKey = VK_F4; break;
    case KeyCode::F5: virtualKey = VK_F5; break;
    case KeyCode::F6: virtualKey = VK_F6; break;
    case KeyCode::F7: virtualKey = VK_F7; break;
    case KeyCode::F8: virtualKey = VK_F8; break;
    case KeyCode::F9: virtualKey = VK_F9; break;
    case KeyCode::F10: virtualKey = VK_F10; break;
    case KeyCode::F11: virtualKey = VK_F11; break;
    case KeyCode::F12: virtualKey = VK_F12; break;
    // The arrows have extended scan codes (0xE0xx): the existing test below sets the extended flag for them.
    case KeyCode::Up: virtualKey = VK_UP; break;
    case KeyCode::Down: virtualKey = VK_DOWN; break;
    case KeyCode::Left: virtualKey = VK_LEFT; break;
    case KeyCode::Right: virtualKey = VK_RIGHT; break;
    }
    const UINT scan = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC_EX);
    if (virtualKey == 0 || scan == 0)
        return false;
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = static_cast<WORD>(scan & 0xffU);
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0U : KEYEVENTF_KEYUP);
    if ((scan & 0xff00U) == 0xe000U || key == KeyCode::KpEnter)
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    return SendInput(1, &input, static_cast<int>(sizeof(input))) == 1;
}
}

std::unique_ptr<KeyInjector> createKeyInjector()
{
    return std::make_unique<TrackedKeyboard>(sendWindowsKey);
}
} // namespace galleond
