// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingRestoreStore.h"

#include <LightingRestore.h>

#include <utility>

namespace bosun {

LightingRestoreStore::LightingRestoreStore(QString path, QObject *parent) : QObject(parent), m_path(std::move(path))
{
    if (m_path.isEmpty())
        return;
    m_restore = galleond::readLightingRestore(m_path).value_or(false);
}

QString LightingRestoreStore::setRestore(bool restore)
{
    if (restore == m_restore)
        return {};
    m_restore = restore;
    Q_EMIT changed();
    if (m_path.isEmpty())
        return {};
    return galleond::writeLightingRestore(m_path, restore) ? QString() : tr("The choice could not be saved.");
}

} // namespace bosun
