// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace galleond {

class KeyboardController;

// Writes keyboard-state.json (KeyboardState.h) after every read or confirmed write of the keyboard's brightness
// by a KeyboardController. A failed read or write publishes nothing: the file keeps the last value that was
// really seen, and is never replaced by a guess. This object only listens to the controller's signals; the
// controller does not know it exists, and neither calls into the Stream Deck side of the daemon.
class KeyboardStatePublisher : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<qint64()>; // seconds since 1970

    KeyboardStatePublisher(const KeyboardController &controller, QString path, Clock clock, QObject *parent = nullptr);

public Q_SLOTS:
    void onBrightnessRead(int value, bool ok, const QString &reason);
    void onBrightnessWritten(int value, bool ok, const QString &reason);

private:
    void publish(int value);

    QString m_path;
    Clock m_clock;
};

} // namespace galleond
