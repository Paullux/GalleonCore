// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <BrightnessCurve.h>
#include <ReplacedFileWatcher.h>

#include <QObject>
#include <QString>
#include <QVariantList>

namespace bosun {

// What the settings window shows and edits of the brightness curve of the day (galleond::BrightnessCurve): the
// mode (the manual slider or the curve) and the 24 points. It reads and writes brightness-curve.json, next to the
// pages file; galleond watches that file and applies it, so this window never talks to the keyboard.
//
// The editor works in the keyboard's unit (0 to 1000, steps of 100); the QML shows it as 0 to 100 %. Whatever the
// editor drags is brought to the nearest step by the model, so the file always parses. A file that is damaged
// (or edited by hand into something invalid) is not trusted: the window shows the default and `problem` says so;
// the next edit replaces the file. The file is also read again when it changes, which an edit of this model
// triggers too (an unchanged curve does not signal).
class BrightnessCurveModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY changed)         // "slider" or "curve"
    Q_PROPERTY(QVariantList hours READ hours NOTIFY changed)  // 24 whole numbers, 0 to 1000, steps of 100
    Q_PROPERTY(QString problem READ problem NOTIFY changed)   // empty, or why the file on disk was not used

public:
    explicit BrightnessCurveModel(const QString &path, QObject *parent = nullptr);

    [[nodiscard]] QString mode() const;
    [[nodiscard]] QVariantList hours() const;
    [[nodiscard]] QString problem() const { return m_problem; }
    [[nodiscard]] galleond::BrightnessCurve curve() const { return m_curve; }

    // "" when saved, else why not. `mode` is "slider" or "curve".
    Q_INVOKABLE [[nodiscard]] QString setMode(const QString &mode);
    // Sets one point to the nearest step of 100 (the value is in the keyboard's unit, 0 to 1000) and saves.
    // "" when saved, else why not (an hour outside 0..23 is refused).
    Q_INVOKABLE [[nodiscard]] QString setHour(int hour, double value);
    // The smooth curve through the points, for drawing: `hour` is 0 to 24 (wraps), the result 0 to 1000.
    Q_INVOKABLE [[nodiscard]] double smooth(double hour) const;
    // The same curve as it would be with this hour's point set to `value` (snapped), for drawing while a point is
    // dragged; nothing is changed or saved. The one implementation of the spline, in galleond::smoothBrightness.
    Q_INVOKABLE [[nodiscard]] double smoothWith(double hour, int overrideHour, double value) const;
    // The point the daemon uses at this time of day (0 to 23), so the editor can mark it.
    Q_INVOKABLE [[nodiscard]] static int pointForTime(int hour, int minute);
    // A free value brought to the nearest step of 100 within 0..1000, as the file will hold it.
    Q_INVOKABLE [[nodiscard]] static int snap(double value);

    // Reads the file again. Public for tests; called when the file changes.
    void reload();

Q_SIGNALS:
    void changed();

private:
    [[nodiscard]] QString save();

    QString m_path;
    galleond::BrightnessCurve m_curve = galleond::defaultBrightnessCurve();
    QString m_problem;
    galleond::ReplacedFileWatcher m_watcher;
};

} // namespace bosun
