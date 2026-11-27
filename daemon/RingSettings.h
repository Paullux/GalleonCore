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
//   {"version":2,"lit":true,"level":1000,"left":"default","right":"#ffffff"}
//
// `lit` false puts the rings out; `level` is 0 to 1000 (the unit of the other levels of the project), a factor on the colours they show. These are
// the settings of every colour of the rings: the ambient ones, what a ring shows to answer an action of the person (magenta while the wallpaper is
// chosen), and the green rain. Put out, or at level 0, the rings show nothing in normal use, whatever the effect: nothing relights them
// (answerRingColour). The green rain is the one exception (floorRingColour). A missing file means "lit, full level, the colours of before", as before
// this file existed.
// `left` and `right` are the ambient colour of each ring: "default" (the colour of the theme on the left, white on the right, as always) or "#rrggbb".
// The colour is a choice of the person, a colour like the others: the level and the "out" setting still apply to it, and it changes neither the magenta
// of the wallpaper choice nor the green rain. A file of version 1 (the three keys `version`, `lit` and `level`, as written before the colours existed)
// is still read, and means "default" for both rings.
// Like every file read from disk it is untrusted: a bounded size, exactly the keys of its version, a boolean, a whole number in range, a colour written as
// above, or nothing at all.
// The file says nothing about the keyboard (the two USB parts stay separate) and nothing about key presses.
inline constexpr int RingLevelFull = 1000;
// The least that a ring is shown at (30 %) when an effect or an answer shows on it at all: so that it stays readable when the rings are set low.
inline constexpr int RingShownMinLevel = 300;
inline constexpr qint64 MaxRingSettingsBytes = 1024;

struct RingSettings
{
    bool lit = true;
    int level = RingLevelFull;
    // NOLINTBEGIN(readability-redundant-member-init): designated initializers elsewhere leave the colours out, and -Wmissing-field-initializers wants them to have one.
    std::optional<std::uint32_t> left{};  // 0xRRGGBB, or nothing for "default": the colour of the theme
    std::optional<std::uint32_t> right{}; // 0xRRGGBB, or nothing for "default": white
    // NOLINTEND(readability-redundant-member-init)

    friend bool operator==(const RingSettings &, const RingSettings &) = default;
};

// The text of a colour choice, as the file holds it and as the settings window passes it: "default", or "#rrggbb" in lower case. Pure.
[[nodiscard]] QString ringColourText(const std::optional<std::uint32_t> &choice);
// false for anything that is not exactly "default" or "#rrggbb" (upper or lower case digits; a short form, a name or a space is refused). `choice` is
// untouched then; for "default" it is set to nothing. Pure.
[[nodiscard]] bool parseRingColour(const QString &text, std::optional<std::uint32_t> *choice);

[[nodiscard]] QByteArray encodeRingSettings(const RingSettings &settings);
// nullopt for anything out of shape; `why`, when given, receives a short reason for the log (never the file's content).
[[nodiscard]] std::optional<RingSettings> parseRingSettings(const QByteArray &bytes, QString *why = nullptr);

// ~/.config/galleoncore/dial-rings.json (the config folder of the platform).
[[nodiscard]] QString ringSettingsPath();
// Atomic (written aside, then renamed). False on failure, and nothing is written for settings out of range.
[[nodiscard]] bool writeRingSettings(const QString &path, const RingSettings &settings);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<RingSettings> readRingSettings(const QString &path, QString *why = nullptr);

// The colour (0xRRGGBB) that each ambient ring starts from, before the level: the one chosen, else the theme's (left) or white (right). Pure.
[[nodiscard]] std::uint32_t leftRingBase(std::uint32_t themeRgb, const RingSettings &settings);
[[nodiscard]] std::uint32_t rightRingBase(const RingSettings &settings);

// The colour (0xRRGGBB) that an ambient ring shows: dark when the rings are out, else each channel scaled by the level (rounded to the nearest). Pure.
[[nodiscard]] std::uint32_t ambientRingColour(std::uint32_t rgb, const RingSettings &settings);

// The colour (0xRRGGBB) of a ring that answers an action of the person (the magenta of the wallpaper choice), in normal use: with the rings put out, or at
// level 0, nothing: the answer does not relight them. Otherwise the level of the settings, raised to RingShownMinLevel so that a low level stays readable. Pure.
[[nodiscard]] std::uint32_t answerRingColour(std::uint32_t rgb, const RingSettings &settings);

// The colour (0xRRGGBB) of a ring in the green rain, which is started on purpose and is the one exception: at least RingShownMinLevel, even with the rings
// put out or at level 0 (the rain relights them), and the level of the settings above that. Pure.
[[nodiscard]] std::uint32_t floorRingColour(std::uint32_t rgb, const RingSettings &settings);

} // namespace galleond
