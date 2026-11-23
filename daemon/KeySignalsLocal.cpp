// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeySignalsLocal.h"

#include "KeySignalsService.h"
#include "LocalControl.h"

namespace galleond {

void connectKeySignals(LocalControl &control, KeySignalsService &service)
{
    QObject::connect(&control, &LocalControl::keySignalRequested, &service,
                     [&service](const QString &client, const QString &key, int red, int green, int blue, const QString &effect, double seconds) {
                         static_cast<void>(service.set(client, key, red, green, blue, effect, seconds)); // the reason of a refusal has nowhere to go: no reply here
                     });
    QObject::connect(&control, &LocalControl::keySignalClearRequested, &service,
                     [&service](const QString &client, const QString &key) { static_cast<void>(service.clear(client, key)); });
    QObject::connect(&control, &LocalControl::keySignalClearAllRequested, &service, [&service](const QString &client) { static_cast<void>(service.clearAll(client)); });
}

} // namespace galleond
