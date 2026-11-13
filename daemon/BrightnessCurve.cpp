// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BrightnessCurve.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace galleond {
namespace {

constexpr int Version = 1;

std::optional<int> brightnessStep(const QJsonValue &value)
{
    if (!value.isDouble())
        return std::nullopt;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0.0 || number > BrightnessCurve::Max || number != std::floor(number))
        return std::nullopt;
    const int whole = static_cast<int>(number);
    if (whole % BrightnessCurve::Step != 0)
        return std::nullopt;
    return whole;
}

} // namespace

BrightnessCurve defaultBrightnessCurve()
{
    BrightnessCurve curve;
    curve.mode = BrightnessCurve::Mode::Slider;
    // Night 20 %, morning and evening 60 %, day 100 %.
    curve.hours = {200, 200, 200, 200, 200, 200, 400, 600, 800, 1000, 1000, 1000,
                   1000, 1000, 1000, 1000, 1000, 800, 800, 600, 600, 400, 200, 200};
    return curve;
}

QByteArray encodeBrightnessCurve(const BrightnessCurve &curve)
{
    QJsonArray hours;
    for (const int value : curve.hours)
        hours.append(value);
    QJsonObject object;
    object.insert(QStringLiteral("version"), Version);
    object.insert(QStringLiteral("mode"), curve.mode == BrightnessCurve::Mode::Curve ? QStringLiteral("curve") : QStringLiteral("slider"));
    object.insert(QStringLiteral("hours"), hours);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

std::optional<BrightnessCurve> parseBrightnessCurve(const QByteArray &bytes)
{
    if (bytes.isEmpty() || bytes.size() > BrightnessCurve::MaxBytes)
        return std::nullopt;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject object = document.object();
    if (object.size() != 3)
        return std::nullopt;
    const QJsonValue version = object.value(QStringLiteral("version"));
    if (!version.isDouble() || version.toDouble() != Version)
        return std::nullopt;
    BrightnessCurve curve;
    const QJsonValue mode = object.value(QStringLiteral("mode"));
    if (mode.isString() && mode.toString() == QStringLiteral("curve"))
        curve.mode = BrightnessCurve::Mode::Curve;
    else if (mode.isString() && mode.toString() == QStringLiteral("slider"))
        curve.mode = BrightnessCurve::Mode::Slider;
    else
        return std::nullopt;
    const QJsonValue hours = object.value(QStringLiteral("hours"));
    if (!hours.isArray() || hours.toArray().size() != BrightnessCurve::Hours)
        return std::nullopt;
    const QJsonArray array = hours.toArray();
    for (int i = 0; i < BrightnessCurve::Hours; ++i) {
        const auto step = brightnessStep(array.at(i));
        if (!step)
            return std::nullopt;
        curve.hours.at(static_cast<std::size_t>(i)) = *step;
    }
    return curve;
}

int brightnessAt(const BrightnessCurve &curve, QTime time)
{
    const int minutes = time.hour() * 60 + time.minute();
    // Nearest whole hour; 30 minutes rounds up; 23:30 and later wrap to 00:00.
    const int hour = ((minutes + 30) / 60) % BrightnessCurve::Hours;
    return curve.hours.at(static_cast<std::size_t>(hour));
}

double smoothBrightness(const std::array<int, BrightnessCurve::Hours> &points, double hour)
{
    constexpr int N = BrightnessCurve::Hours;
    const double wrapped = std::fmod(std::fmod(hour, N) + N, N);
    const int i1 = static_cast<int>(std::floor(wrapped)) % N;
    const double t = wrapped - std::floor(wrapped);
    const auto at = [&points](int i) { return static_cast<double>(points.at(static_cast<std::size_t>(((i % N) + N) % N))); };
    const double p0 = at(i1 - 1);
    const double p1 = at(i1);
    const double p2 = at(i1 + 1);
    const double p3 = at(i1 + 2);
    // Catmull-Rom: passes through p1 at t = 0 and p2 at t = 1.
    const double value = 0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t
                                + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t);
    return std::clamp(value, 0.0, static_cast<double>(BrightnessCurve::Max));
}

int snapBrightness(double value)
{
    if (!std::isfinite(value))
        return 0;
    const double clamped = std::clamp(value, 0.0, static_cast<double>(BrightnessCurve::Max));
    return static_cast<int>(std::floor(clamped / BrightnessCurve::Step + 0.5)) * BrightnessCurve::Step;
}

QString brightnessCurvePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/brightness-curve.json");
}

bool writeBrightnessCurve(const QString &path, const BrightnessCurve &curve)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(encodeBrightnessCurve(curve));
    return file.commit();
}

std::optional<BrightnessCurve> readBrightnessCurve(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return parseBrightnessCurve(file.read(BrightnessCurve::MaxBytes + 1));
}

} // namespace galleond
