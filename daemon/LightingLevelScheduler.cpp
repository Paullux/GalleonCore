// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingLevelScheduler.h"

#include "LightingController.h"

#include <QStandardPaths>

#include <utility>

namespace galleond {

LightingLevelScheduler::LightingLevelScheduler(LightingController &controller, const QString &curvePath, Clock clock, int tickMs, QObject *parent)
    : QObject(parent), m_controller(controller), m_path(curvePath), m_clock(std::move(clock)), m_watcher(curvePath)
{
    connect(&m_watcher, &ReplacedFileWatcher::maybeChanged, this, &LightingLevelScheduler::reload);
    m_tick.setInterval(tickMs);
    connect(&m_tick, &QTimer::timeout, this, &LightingLevelScheduler::evaluate);
    m_tick.start();
    reload();
}

void LightingLevelScheduler::reload()
{
    if (const auto fresh = readBrightnessCurve(m_path))
        m_curve = fresh; // an invalid or missing file leaves the curve in force (a half-written file changes nothing)
    evaluate();
}

void LightingLevelScheduler::evaluate()
{
    if (m_curve && m_curve->mode == BrightnessCurve::Mode::Curve)
        static_cast<void>(m_controller.setLevelOverride(brightnessAt(*m_curve, m_clock())));
    else
        static_cast<void>(m_controller.setLevelOverride(std::nullopt));
}

QString lightingLevelCurvePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/lighting-level-curve.json");
}

} // namespace galleond
