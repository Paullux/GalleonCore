// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeySignalsAccess.h"

#include <QStandardPaths>

namespace galleond {

QString keySignalsAccessPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/key-signals.json");
}

} // namespace galleond
