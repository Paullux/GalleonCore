// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QTime>

#include <array>
#include <optional>

namespace galleond {

// The keyboard's brightness through the day: one point per hour, each a step of 100 from 0 to 1000 (the unit
// of the keyboard, property 0xEA; Bosun shows it as 0 to 100 %). The settings window edits it as a smooth curve
// and saves the nearest points; the daemon applies the point nearest to the time of day. A second field says
// whether the curve is in charge ("curve") or the manual slider is ("slider"), so the daemon never writes a
// value the user did not choose.
//
//   {"version":1,"mode":"curve","hours":[200,200, ... 24 numbers ... ,300]}
//
// The file is the user's and may be edited by hand or damaged: it is untrusted input. Exactly these three keys,
// a known mode, exactly 24 whole numbers that are steps of 100 within 0..1000, at most 4 KiB; anything else
// is refused whole (the previous curve stays in force), never partly read.
struct BrightnessCurve
{
    enum class Mode : unsigned char { Slider, Curve };

    static constexpr int Hours = 24;
    static constexpr int Step = 100;
    static constexpr int Max = 1000;
    static constexpr qint64 MaxBytes = 4096;

    Mode mode = Mode::Slider;
    std::array<int, Hours> hours{};

    friend bool operator==(const BrightnessCurve &, const BrightnessCurve &) = default;
};

// Slider mode and a gentle day shape (dim at night, full by day): what a user who never opened the editor gets.
[[nodiscard]] BrightnessCurve defaultBrightnessCurve();

[[nodiscard]] QByteArray encodeBrightnessCurve(const BrightnessCurve &curve);
[[nodiscard]] std::optional<BrightnessCurve> parseBrightnessCurve(const QByteArray &bytes);

// The brightness to write at this time of day: the point nearest to it, so 07:29 takes the 07:00 point and 07:30
// the 08:00 one (a tie goes to the later hour); 23:30 and later take the 00:00 point, the curve being a circle.
// One value per hour at most, so the keyboard's own memory is written at most 24 times a day.
[[nodiscard]] int brightnessAt(const BrightnessCurve &curve, QTime time);

// For the editor: a smooth, closed curve through the 24 points (a cubic Catmull-Rom spline that wraps from 23:00
// to 00:00), at `hour` in [0, 24). Clamped to 0..1000. It passes through each point at its hour.
[[nodiscard]] double smoothBrightness(const std::array<int, BrightnessCurve::Hours> &points, double hour);

// The editor drags a free value; what is saved is the nearest step of 100 within 0..1000 (a tie goes up).
[[nodiscard]] int snapBrightness(double value);

// ~/.config/galleoncore/brightness-curve.json, next to pages.json.
[[nodiscard]] QString brightnessCurvePath();
// Atomic. Creates the folder. False on failure.
[[nodiscard]] bool writeBrightnessCurve(const QString &path, const BrightnessCurve &curve);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<BrightnessCurve> readBrightnessCurve(const QString &path);

} // namespace galleond
