// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>

namespace galleond {

// The settings of the two rings around the dials of the module (dial-rings.json), written by the settings window and applied by galleond:
//
//   {"version":1,"lit":true,"level":1000}
//
// `lit` false puts the rings out; `level` is 0 to 1000 (the unit of the other levels of the project), a factor on the colours they show. These are
// the settings of every colour of the rings: the ambient ones (the theme on the left, white on the right), what a ring shows to answer an action of the person
// (magenta while the wallpaper is chosen), and the green rain. Put out, or at level 0, the rings show nothing in normal use, whatever the effect: nothing
// relights them (answerRingColour). The green rain is the one exception (floorRingColour). A missing file means "lit, full level", as before this file existed.
// Like every file read from disk it is untrusted: a bounded size, exactly the keys above, a boolean, a whole number in range, or nothing at all.
// The file says nothing about the keyboard (the two USB parts stay separate) and nothing about key presses.
inline constexpr int RingLevelFull = 1000;
// The least that a ring is shown at (30 %) when an effect or an answer shows on it at all: so that it stays readable when the rings are set low.
inline constexpr int RingShownMinLevel = 300;
inline constexpr qint64 MaxRingSettingsBytes = 1024;

struct RingSettings
{
    bool lit = true;
    int level = RingLevelFull;

    friend bool operator==(const RingSettings &, const RingSettings &) = default;
};

[[nodiscard]] QByteArray encodeRingSettings(const RingSettings &settings);
// nullopt for anything out of shape; `why`, when given, receives a short reason for the log (never the file's content).
[[nodiscard]] std::optional<RingSettings> parseRingSettings(const QByteArray &bytes, QString *why = nullptr);

// ~/.config/galleoncore/dial-rings.json (the config folder of the platform).
[[nodiscard]] QString ringSettingsPath();
// Atomic (written aside, then renamed). False on failure, and nothing is written for settings out of range.
[[nodiscard]] bool writeRingSettings(const QString &path, const RingSettings &settings);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<RingSettings> readRingSettings(const QString &path, QString *why = nullptr);

// The colour (0xRRGGBB) that an ambient ring shows: dark when the rings are out, else each channel scaled by the level (rounded to the nearest). Pure.
[[nodiscard]] std::uint32_t ambientRingColour(std::uint32_t rgb, const RingSettings &settings);

// The colour (0xRRGGBB) of a ring that answers an action of the person (the magenta of the wallpaper choice), in normal use: with the rings put out, or at
// level 0, nothing: the answer does not relight them. Otherwise the level of the settings, raised to RingShownMinLevel so that a low level stays readable. Pure.
[[nodiscard]] std::uint32_t answerRingColour(std::uint32_t rgb, const RingSettings &settings);

// The colour (0xRRGGBB) of a ring in the green rain, which is started on purpose and is the one exception: at least RingShownMinLevel, even with the rings
// put out or at level 0 (the rain relights them), and the level of the settings above that. Pure.
[[nodiscard]] std::uint32_t floorRingColour(std::uint32_t rgb, const RingSettings &settings);

} // namespace galleond
