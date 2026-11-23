// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SessionEnding.h"

#include <QtGlobal> // Q_OS_WIN comes from here: without it the Windows branch would silently not be built

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace galleond {

bool sessionIsEnding()
{
#ifdef Q_OS_WIN
    return GetSystemMetrics(SM_SHUTTINGDOWN) != 0;
#else
    return false;
#endif
}

} // namespace galleond
