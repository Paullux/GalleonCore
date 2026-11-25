// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WindowsRawKeyPressSource.h"

#include "RawKeyboard.h"

#include <QString>

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

namespace galleond {

// The flags of RawKeyboard.h are the ones of winuser.h: checked here, where the real macros exist.
static_assert(RawKeyBreak == static_cast<unsigned>(RI_KEY_BREAK), "RI_KEY_BREAK");
static_assert(RawKeyE0 == static_cast<unsigned>(RI_KEY_E0), "RI_KEY_E0");
static_assert(RawKeyE1 == static_cast<unsigned>(RI_KEY_E1), "RI_KEY_E1");

struct RawKeyWindow
{
    HWND handle = nullptr;
    WindowsRawKeyPressSource *owner = nullptr;
    bool registered = false;      // RegisterRawInputDevices was done and not yet undone
    bool dispatching = false;     // inside the signal: a slot may stop the source, and the window is then destroyed after the message
    bool destroyAfter = false;
    std::vector<std::pair<HANDLE, bool>> devices; // the handle of a device and whether it is the Galleon keyboard: bounded, no key in it
};

namespace {

constexpr wchar_t WindowClass[] = L"GalleonCoreRawKeys";
constexpr USHORT UsagePageGeneric = 0x01; // HID usage page "Generic Desktop"
constexpr USHORT UsageKeyboard = 0x06;    // HID usage "Keyboard"
constexpr UINT LongestDeviceName = 1024;  // in characters: a name that is longer is refused

// Whether this device is the Galleon keyboard. Asked once for each handle and then remembered (a handle is a device, not a key).
bool acceptsDevice(RawKeyWindow &window, HANDLE device)
{
    for (const auto &known : window.devices) {
        if (known.first == device)
            return known.second;
    }
    bool accepted = false;
    UINT characters = 0;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &characters) == 0 && characters > 0 && characters <= LongestDeviceName) {
        std::wstring name(characters, L'\0');
        UINT size = characters;
        const UINT copied = GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, name.data(), &size);
        if (copied != static_cast<UINT>(-1) && copied > 0)
            accepted = isGalleonKeyboardDeviceName(QString::fromWCharArray(name.c_str()));
    }
    if (window.devices.size() >= static_cast<std::size_t>(WindowsRawKeyPressSource::MaxKnownDevices))
        window.devices.erase(window.devices.begin());
    window.devices.emplace_back(device, accepted);
    return accepted;
}

// One WM_INPUT message. Anything that Windows gives that is not what is expected is dropped without a word.
void handleRawInput(RawKeyWindow &window, HRAWINPUT input)
{
    // The header first: it says which device sent the message. The key of any other keyboard is not read.
    RAWINPUTHEADER header{};
    UINT headerSize = sizeof(header);
    if (GetRawInputData(input, RID_HEADER, &header, &headerSize, sizeof(RAWINPUTHEADER)) != sizeof(header))
        return;
    if (header.dwType != RIM_TYPEKEYBOARD || header.hDevice == nullptr || !acceptsDevice(window, header.hDevice))
        return;
    RAWINPUT data{};
    UINT dataSize = sizeof(data);
    const UINT got = GetRawInputData(input, RID_INPUT, &data, &dataSize, sizeof(RAWINPUTHEADER));
    if (got != static_cast<UINT>(-1) && got >= sizeof(RAWINPUTHEADER) && data.header.dwType == RIM_TYPEKEYBOARD) {
        const RawKeyboardEvent event{data.data.keyboard.MakeCode, data.data.keyboard.Flags};
        if (const auto usage = usageOfRawKeyDown(event)) {
            window.dispatching = true;
            Q_EMIT window.owner->keyDown(*usage); // within galleond only, and nothing is kept
            window.dispatching = false;
        }
    }
    SecureZeroMemory(&data, sizeof(data)); // nothing of the key stays in memory once it has been used
}

LRESULT CALLBACK windowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message != WM_INPUT)
        return DefWindowProcW(handle, message, wParam, lParam);
    auto *window = reinterpret_cast<RawKeyWindow *>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    if (window != nullptr && window->owner != nullptr && window->owner->watching())
        handleRawInput(*window, reinterpret_cast<HRAWINPUT>(lParam));
    // Windows asks for DefWindowProc after a WM_INPUT, for its own cleanup, whether the message was read or not.
    const LRESULT result = DefWindowProcW(handle, message, wParam, lParam);
    if (window != nullptr && window->destroyAfter && !window->dispatching) {
        window->destroyAfter = false;
        SetWindowLongPtrW(handle, GWLP_USERDATA, 0);
        window->handle = nullptr;
        DestroyWindow(handle);
    }
    return result;
}

bool createWindow(RawKeyWindow &window)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = WindowClass;
    if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;
    // A top-level window that is never shown, as the one of the power events is.
    window.handle = CreateWindowExW(WS_EX_TOOLWINDOW, WindowClass, L"GalleonCore key glow", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window.handle == nullptr)
        return false;
    SetWindowLongPtrW(window.handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&window));
    return true;
}

} // namespace

WindowsRawKeyPressSource::WindowsRawKeyPressSource(QObject *parent) : KeyPressSource(parent), m_window(std::make_unique<RawKeyWindow>())
{
    m_window->owner = this;
}

WindowsRawKeyPressSource::~WindowsRawKeyPressSource()
{
    release();
    if (m_window->handle != nullptr) { // left alive by a stop() inside the signal
        SetWindowLongPtrW(m_window->handle, GWLP_USERDATA, 0);
        DestroyWindow(m_window->handle);
        m_window->handle = nullptr;
    }
}

void WindowsRawKeyPressSource::start()
{
    if (m_watching)
        return;
    m_window->destroyAfter = false;
    if (m_window->handle == nullptr && !createWindow(*m_window)) {
        Q_EMIT unavailable();
        return;
    }
    RAWINPUTDEVICE device{};
    device.usUsagePage = UsagePageGeneric;
    device.usUsage = UsageKeyboard;
    device.dwFlags = RIDEV_INPUTSINK; // the events come while another program has the focus; RIDEV_NOLEGACY is NOT set: the keys still reach that program
    device.hwndTarget = m_window->handle;
    if (RegisterRawInputDevices(&device, 1, sizeof(device)) == FALSE) {
        DestroyWindow(m_window->handle);
        m_window->handle = nullptr;
        Q_EMIT unavailable();
        return;
    }
    m_window->registered = true;
    m_window->devices.clear();
    m_watching = true;
}

void WindowsRawKeyPressSource::stop()
{
    release();
}

void WindowsRawKeyPressSource::release()
{
    if (m_window->registered) { // unregistered before anything else: from here Windows sends nothing to this process
        RAWINPUTDEVICE device{};
        device.usUsagePage = UsagePageGeneric;
        device.usUsage = UsageKeyboard;
        device.dwFlags = RIDEV_REMOVE;
        device.hwndTarget = nullptr;
        RegisterRawInputDevices(&device, 1, sizeof(device));
        m_window->registered = false;
    }
    m_watching = false;
    m_window->devices.clear();
    if (m_window->handle == nullptr)
        return;
    if (m_window->dispatching) { // stopped by a slot of keyDown, in the middle of the message: the window goes once the message is over
        m_window->destroyAfter = true;
        return;
    }
    SetWindowLongPtrW(m_window->handle, GWLP_USERDATA, 0);
    DestroyWindow(m_window->handle);
    m_window->handle = nullptr;
}

void *WindowsRawKeyPressSource::windowForTests() const
{
    return m_window->handle;
}

} // namespace galleond
