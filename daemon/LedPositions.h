// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <galleon/keyboard/Appearance.h>

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

namespace galleond {

// Where each LED of the live frame is on the keyboard, in millimetres: data/devices/galleon-100-sd-led-positions.json, written by
// tools/keyboard-map/build_map.py. The file is compiled into galleond as a Qt resource, so there is no install path to find on any system (DEB, RPM,
// AppImage, Windows). It is read like any file: strictly enough that a damaged one gives nothing rather than a wrong place.
//
// Only what the effects need is read (the LED, whether it is a key or a section of the strip, and its x and y); the other keys of an entry are ignored.
inline constexpr qint64 MaxLedPositionsBytes = 65536;

// nullopt for a file that is not a version 1 file in millimetres, with no LED, with a LED of the frame twice or beyond it, or a place outside the keyboard
// (more than a metre from the origin). `why`, when given, receives a short reason (never the file's content).
[[nodiscard]] std::optional<std::vector<galleon::keyboard::LedPosition>> parseLedPositions(const QByteArray &bytes, QString *why = nullptr);

// The positions compiled into the program. Read once; empty if the resource could not be read (which a test catches).
[[nodiscard]] const std::vector<galleon::keyboard::LedPosition> &builtInLedPositions();

} // namespace galleond
