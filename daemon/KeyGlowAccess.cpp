// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyGlowAccess.h"

#include <QStandardPaths>

namespace galleond {

QString keyGlowAccessPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/galleoncore/key-glow.json");
}

} // namespace galleond
