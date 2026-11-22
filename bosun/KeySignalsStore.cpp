// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeySignalsStore.h"

#include <KeySignalsAccess.h>

#include <utility>

namespace bosun {

KeySignalsStore::KeySignalsStore(QString path, QObject *parent) : QObject(parent), m_path(std::move(path))
{
    if (m_path.isEmpty())
        return;
    m_allow = galleond::keySignalsAllowed(m_path);
}

QString KeySignalsStore::setAllow(bool allow)
{
    if (allow == m_allow)
        return {};
    m_allow = allow;
    Q_EMIT changed();
    if (m_path.isEmpty())
        return {};
    return galleond::writeKeySignalsAccess(m_path, allow) ? QString() : tr("The choice could not be saved.");
}

} // namespace bosun
