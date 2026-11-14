// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingStatePublisher.h"

#include "LightingController.h"
#include "LightingState.h"

#include <QDebug>

namespace galleond {

LightingStatePublisher::LightingStatePublisher(const LightingController &controller, QString path, Clock clock, QObject *parent)
    : QObject(parent), m_path(std::move(path)), m_clock(std::move(clock))
{
    connect(&controller, &LightingController::stateChanged, this, &LightingStatePublisher::onStateChanged);
}

void LightingStatePublisher::onStateChanged(const QString &state, const QString &detail)
{
    if (!isKnownLightingState(state))
        return; // never publish a state the reader would refuse
    if (!writeLightingState(m_path, {.state = state, .detail = detail.left(MaxLightingDetail), .time = m_clock()}))
        qWarning() << "galleond: cannot write" << m_path;
}

} // namespace galleond
