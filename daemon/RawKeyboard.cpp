// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RawKeyboard.h"

#include <QRegularExpression>

namespace galleond {

std::optional<galleon::keyboard::HidUsage> usageOfRawKeyDown(const RawKeyboardEvent &event)
{
    if ((event.flags & RawKeyBreak) != 0 || (event.flags & RawKeyE1) != 0)
        return std::nullopt;
    return galleon::keyboard::hidUsageFromWindowsScanCode(event.makeCode, (event.flags & RawKeyE0) != 0);
}

bool isGalleonKeyboardDeviceName(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral("VID_1B1C&PID_2B0C(?![0-9A-F])"), QRegularExpression::CaseInsensitiveOption);
    return name.size() <= 1024 && pattern.match(name).hasMatch(); // a name longer than any real one is refused
}

} // namespace galleond
