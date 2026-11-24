// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// What is the same on both systems in the "Try a signal" panel: finding the key of a LED and checking the request. The sending is by platform.
#include "DaemonStatus.h"

#include "KeyMapFile.h"
#include "KeySignalRequest.h"

namespace bosun {

QString DaemonStatus::keyNameOfLed(int led) const // NOLINT(readability-convert-member-functions-to-static): called from QML on the object
{
    return bosun::keyNameOfLed(galleond::builtInKeyMap(), led);
}

QString DaemonStatus::lightKey(int led, const QString &colour, bool pulse, int seconds)
{
    if (!running())
        return tr("Start galleond to light a key");
    const QString key = keyNameOfLed(led);
    if (key.isEmpty())
        return tr("Choose a key on the drawing: this one has no measured light");
    QString why;
    const auto request = makeKeySignalRequest(key, colour, pulse, seconds, &why);
    if (!request)
        return tr("The request is not valid: %1").arg(why);
    postKeySignal(*request);
    return {};
}

QString DaemonStatus::clearKey(int led)
{
    if (!running())
        return tr("Start galleond to take a signal away");
    const QString key = keyNameOfLed(led);
    if (key.isEmpty())
        return tr("Choose a key on the drawing: this one has no measured light");
    postKeySignalClear(key);
    return {};
}

QString DaemonStatus::clearKeys()
{
    if (!running())
        return tr("Start galleond to take a signal away");
    postKeySignalsClear();
    return {};
}

} // namespace bosun
