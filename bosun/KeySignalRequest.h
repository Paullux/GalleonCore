// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/KeyMap.h>

#include <QString>

#include <optional>

namespace bosun {

// What the "Try a signal" panel of the settings window asks galleond for: the same request another program makes (KeySignals.h, docs/KEY_SIGNALS.md), so that a person
// can see what a program would light. Pure: it builds and checks the request, nothing is sent here and no key press is read (the key is the one chosen on the drawing).
struct KeySignalRequest
{
    QString key;     // the name of the physical key as KeyIdentity.h names it ("F3"): the place on a QWERTY keyboard
    int red = 0;
    int green = 0;
    int blue = 0;
    bool pulse = false;
    int seconds = 0; // 1 to 86400

    friend bool operator==(const KeySignalRequest &, const KeySignalRequest &) = default;
};

// The longest a signal can last, the same bound as galleond's (MaxSignalSeconds).
inline constexpr int LongestSignalSeconds = 24 * 60 * 60;

// The name of the key whose light is this LED, from the map of measured lights; "" for a LED that lights a section of the strip, lights nothing that was seen, is not
// in the map or is out of the frame. A key whose light has not been measured has no name here: it cannot be asked for.
[[nodiscard]] QString keyNameOfLed(const galleon::keyboard::KeyMap &map, int led);

// nullopt, with a short reason in `why`, for a request that is not whole: no key, a colour that is not "#rrggbb", a time out of 1 to 86400 seconds.
[[nodiscard]] std::optional<KeySignalRequest> makeKeySignalRequest(const QString &key, const QString &colour, bool pulse, int seconds, QString *why = nullptr);

} // namespace bosun
