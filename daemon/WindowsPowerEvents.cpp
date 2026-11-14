// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WindowsPowerEvents.h"

#include "PowerMessages.h"

#include <windows.h>

namespace galleond {

struct PowerWindow
{
    HWND handle = nullptr;
    HPOWERNOTIFY registration = nullptr;
    WindowsPowerEvents *owner = nullptr;
};

namespace {

constexpr wchar_t WindowClass[] = L"GalleonCorePowerEvents";

LRESULT CALLBACK windowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *window = reinterpret_cast<PowerWindow *>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    if (window && window->owner) {
        switch (powerChangeFor(message, wParam)) {
        case PowerChange::GoingToSleep: Q_EMIT window->owner->aboutToSleep(); break; // answered before this returns
        case PowerChange::Resumed: Q_EMIT window->owner->resumed(); break;
        case PowerChange::None: break;
        }
    }
    if (message == WM_POWERBROADCAST || message == WM_QUERYENDSESSION)
        return TRUE; // nothing here refuses a suspension or the end of the session
    if (message == WM_ENDSESSION)
        return 0;
    return DefWindowProcW(handle, message, wParam, lParam);
}

} // namespace

WindowsPowerEvents::WindowsPowerEvents(QObject *parent) : PowerEvents(parent), m_window(std::make_unique<PowerWindow>())
{
    m_window->owner = this;
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = WindowClass;
    if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return;
    // A top-level window that is never shown, not a message-only one: only top-level windows are sent the broadcasts.
    m_window->handle = CreateWindowExW(WS_EX_TOOLWINDOW, WindowClass, L"GalleonCore power events", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                                       GetModuleHandleW(nullptr), nullptr);
    if (!m_window->handle)
        return;
    SetWindowLongPtrW(m_window->handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_window.get()));
    // Also asked for explicitly: this is what delivers the suspend and resume of a machine that goes to a low-power state.
    // The two ways can tell the same thing twice: the lighting takes the second for nothing.
    m_window->registration = RegisterSuspendResumeNotification(m_window->handle, DEVICE_NOTIFY_WINDOW_HANDLE);
}

WindowsPowerEvents::~WindowsPowerEvents()
{
    if (m_window->registration)
        UnregisterSuspendResumeNotification(m_window->registration);
    if (m_window->handle)
        DestroyWindow(m_window->handle);
}

bool WindowsPowerEvents::available(QString *why) const
{
    if (m_window->handle)
        return true;
    *why = tr("galleond could not open the window through which Windows tells about sleep: the lighting stays off, because the keys could not be given back before sleeping.");
    return false;
}

} // namespace galleond
