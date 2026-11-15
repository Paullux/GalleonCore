// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BrightnessCurve.h"
#include "ReplacedFileWatcher.h"

#include <QObject>
#include <QString>
#include <QTime>
#include <QTimer>

#include <functional>
#include <optional>

namespace galleond {

class LightingController;

// The second curve: the LEVEL of the colours of the live lighting through the day (lighting-level-curve.json, the same format as the brightness
// curve: 24 points, steps of 100, a mode "slider" or "curve"). It is a software factor on the colours; it never writes to the keyboard, so it can
// change every hour without wearing anything. The brightness curve of the keyboard itself (BrightnessScheduler) is a different file and a different
// thing.
//
// In curve mode the level in force is the point of the curve for the time of day (the one rule of brightnessAt), given to the controller as a level
// that takes the place of the appearance's own; in slider mode, or with no file, the level of the appearance file is in force. The file is untrusted:
// an invalid or missing one is ignored whole and what is in force stays, like the brightness curve. The time comes from an injected clock, checked
// every `tickMs`. It only calls LightingController::setLevelOverride.
class LightingLevelScheduler : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<QTime()>;

    LightingLevelScheduler(LightingController &controller, const QString &curvePath, Clock clock, int tickMs = 30000, QObject *parent = nullptr);

    void reload();   // reads the file again and decides; called when the file changes
    void evaluate(); // decides now; called by the timer

    [[nodiscard]] std::optional<BrightnessCurve> curve() const { return m_curve; }

private:
    LightingController &m_controller;
    QString m_path;
    Clock m_clock;
    ReplacedFileWatcher m_watcher;
    QTimer m_tick;
    std::optional<BrightnessCurve> m_curve;
};

// ~/.config/galleoncore/lighting-level-curve.json, next to brightness-curve.json.
[[nodiscard]] QString lightingLevelCurvePath();

} // namespace galleond
