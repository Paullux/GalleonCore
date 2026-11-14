// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingEnvironment.h"

#include <memory>

namespace galleond {

struct PowerWindow; // the native window: kept out of this header so that it does not pull <windows.h>

// Sleep, wake and the end of the session on Windows, for the live lighting (see PowerMessages.h for what each message means).
// A window that is never shown receives them: a message-only window would not, since the broadcasts of Windows go to the
// top-level windows only. It must live on the thread of the event loop: the messages arrive as that loop runs, and the
// controller's slots run on that thread. Windows gives a program only a short time to answer a suspension, and cannot be asked
// to wait (holdSleep and allowSleep do nothing here): the keys are given back inside the notification itself, which for a
// keyboard that answers takes a fraction of a second. Without the window, lighting is refused.
class WindowsPowerEvents final : public PowerEvents
{
    Q_OBJECT

public:
    explicit WindowsPowerEvents(QObject *parent = nullptr);
    ~WindowsPowerEvents() override;

    [[nodiscard]] bool available(QString *why) const override;
    void holdSleep() override {}
    void allowSleep() override {}

private:
    std::unique_ptr<PowerWindow> m_window;
};

} // namespace galleond
