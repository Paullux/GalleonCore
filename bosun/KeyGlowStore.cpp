// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyGlowStore.h"

#include <KeyGlowAccess.h>

#include <utility>

namespace bosun {

KeyGlowStore::KeyGlowStore(QString path, bool supported, QObject *parent) : QObject(parent), m_path(std::move(path)), m_supported(supported)
{
    if (m_path.isEmpty() || !m_supported)
        return;
    m_allow = galleond::keyGlowAllowed(m_path);
}

QString KeyGlowStore::setAllow(bool allow)
{
    if (!m_supported)
        return tr("This is not available on this system yet.");
    if (allow == m_allow)
        return {};
    m_allow = allow;
    Q_EMIT changed();
    if (m_path.isEmpty())
        return {};
    return galleond::writeKeyGlowAccess(m_path, allow) ? QString() : tr("The choice could not be saved.");
}

} // namespace bosun
