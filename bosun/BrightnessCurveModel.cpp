// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BrightnessCurveModel.h"

#include <QFile>
#include <QTime>
#include <QVariantList>

#include <cstddef>

namespace bosun {

using galleond::BrightnessCurve;

BrightnessCurveModel::BrightnessCurveModel(const QString &path, QObject *parent)
    : QObject(parent), m_path(path), m_watcher(path)
{
    connect(&m_watcher, &galleond::ReplacedFileWatcher::maybeChanged, this, &BrightnessCurveModel::reload);
    reload();
}

QString BrightnessCurveModel::mode() const
{
    return m_curve.mode == BrightnessCurve::Mode::Curve ? QStringLiteral("curve") : QStringLiteral("slider");
}

QVariantList BrightnessCurveModel::hours() const
{
    QVariantList list;
    for (const int value : m_curve.hours)
        list.append(value);
    return list;
}

void BrightnessCurveModel::reload()
{
    BrightnessCurve next = galleond::defaultBrightnessCurve();
    QString problem;
    if (QFile::exists(m_path)) {
        if (const auto read = galleond::readBrightnessCurve(m_path))
            next = *read;
        else
            problem = tr("The brightness curve file is damaged and was not used; changing it here replaces it.");
    }
    if (next == m_curve && problem == m_problem)
        return; // our own save, or nothing new
    m_curve = next;
    m_problem = problem;
    Q_EMIT changed();
}

QString BrightnessCurveModel::save()
{
    if (!galleond::writeBrightnessCurve(m_path, m_curve))
        return tr("The brightness curve could not be saved.");
    m_problem.clear();
    return {};
}

QString BrightnessCurveModel::setMode(const QString &mode)
{
    BrightnessCurve::Mode next{};
    if (mode == QStringLiteral("curve"))
        next = BrightnessCurve::Mode::Curve;
    else if (mode == QStringLiteral("slider"))
        next = BrightnessCurve::Mode::Slider;
    else
        return tr("Unknown brightness mode.");
    if (next == m_curve.mode && m_problem.isEmpty())
        return {};
    const BrightnessCurve before = m_curve;
    m_curve.mode = next;
    if (const QString error = save(); !error.isEmpty()) {
        m_curve = before;
        return error;
    }
    Q_EMIT changed();
    return {};
}

QString BrightnessCurveModel::setHour(int hour, double value)
{
    if (hour < 0 || hour >= BrightnessCurve::Hours)
        return tr("There is no such hour on the curve.");
    const int step = galleond::snapBrightness(value);
    if (m_curve.hours.at(static_cast<std::size_t>(hour)) == step && m_problem.isEmpty())
        return {};
    const BrightnessCurve before = m_curve;
    m_curve.hours.at(static_cast<std::size_t>(hour)) = step;
    if (const QString error = save(); !error.isEmpty()) {
        m_curve = before;
        return error;
    }
    Q_EMIT changed();
    return {};
}

double BrightnessCurveModel::smooth(double hour) const
{
    return galleond::smoothBrightness(m_curve.hours, hour);
}

double BrightnessCurveModel::smoothWith(double hour, int overrideHour, double value) const
{
    auto points = m_curve.hours;
    if (overrideHour >= 0 && overrideHour < BrightnessCurve::Hours)
        points.at(static_cast<std::size_t>(overrideHour)) = galleond::snapBrightness(value);
    return galleond::smoothBrightness(points, hour);
}

int BrightnessCurveModel::pointForTime(int hour, int minute)
{
    const QTime time(hour, minute);
    return time.isValid() ? galleond::nearestHourIndex(time) : 0;
}

int BrightnessCurveModel::snap(double value)
{
    return galleond::snapBrightness(value);
}

} // namespace bosun
