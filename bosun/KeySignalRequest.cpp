// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeySignalRequest.h"

#include <galleon/keyboard/KeyIdentity.h>

#include <QRegularExpression>

namespace bosun {

QString keyNameOfLed(const galleon::keyboard::KeyMap &map, int led)
{
    if (led < 0 || led >= static_cast<int>(galleon::keyboard::LiveLedCount))
        return {};
    const auto assignment = map.assignmentFor(static_cast<std::uint16_t>(led));
    if (!assignment || assignment->kind != galleon::keyboard::LedKind::Key)
        return {};
    const auto name = galleon::keyboard::keyName(assignment->key);
    return name ? QString::fromLatin1(*name) : QString();
}

std::optional<KeySignalRequest> makeKeySignalRequest(const QString &key, const QString &colour, bool pulse, int seconds, QString *why)
{
    const auto refuse = [why](const QString &reason) -> std::optional<KeySignalRequest> {
        if (why != nullptr)
            *why = reason;
        return std::nullopt;
    };
    if (key.isEmpty() || key.size() > 32)
        return refuse(QStringLiteral("no key"));
    static const QRegularExpression hex(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    if (!hex.match(colour).hasMatch())
        return refuse(QStringLiteral("the colour is not #rrggbb"));
    if (seconds < 1 || seconds > LongestSignalSeconds)
        return refuse(QStringLiteral("the time is out of 1 to 86400 seconds"));
    KeySignalRequest request;
    request.key = key;
    request.red = colour.mid(1, 2).toInt(nullptr, 16);
    request.green = colour.mid(3, 2).toInt(nullptr, 16);
    request.blue = colour.mid(5, 2).toInt(nullptr, 16);
    request.pulse = pulse;
    request.seconds = seconds;
    return request;
}

} // namespace bosun
