// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyboardStatePublisher.h"

#include "KeyboardController.h"
#include "KeyboardState.h"

#include <QDebug>

namespace galleond {

KeyboardStatePublisher::KeyboardStatePublisher(const KeyboardController &controller, QString path, Clock clock, QObject *parent)
    : QObject(parent), m_path(std::move(path)), m_clock(std::move(clock))
{
    connect(&controller, &KeyboardController::brightnessRead, this, &KeyboardStatePublisher::onBrightnessRead);
    connect(&controller, &KeyboardController::brightnessWritten, this, &KeyboardStatePublisher::onBrightnessWritten);
}

void KeyboardStatePublisher::onBrightnessRead(int value, bool ok, const QString & /*reason*/)
{
    if (ok)
        publish(value);
}

void KeyboardStatePublisher::onBrightnessWritten(int value, bool ok, const QString & /*reason*/)
{
    if (ok)
        publish(value);
}

void KeyboardStatePublisher::publish(int value)
{
    if (!writeKeyboardState(m_path, {.brightness = value, .time = m_clock()}))
        qWarning() << "galleond: cannot write" << m_path;
}

} // namespace galleond
