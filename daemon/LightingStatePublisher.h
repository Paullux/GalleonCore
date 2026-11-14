// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace galleond {

class LightingController;

// Writes lighting-state.json (LightingState.h) at every change of state of the live lighting. It only listens to the
// controller's signal; the controller does not know it exists. A failed write is logged and nothing else: the file keeps
// the last state that was written, and Bosun also knows when galleond is gone.
class LightingStatePublisher : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<qint64()>; // seconds since 1970

    LightingStatePublisher(const LightingController &controller, QString path, Clock clock, QObject *parent = nullptr);

public Q_SLOTS:
    void onStateChanged(const QString &state, const QString &detail);

private:
    QString m_path;
    Clock m_clock;
};

} // namespace galleond
