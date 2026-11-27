// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RingStore.h"

#include <utility>

namespace bosun {

RingStore::RingStore(QString path, QObject *parent) : QObject(parent), m_path(std::move(path))
{
    if (m_path.isEmpty())
        return;
    if (const auto stored = galleond::readRingSettings(m_path))
        m_settings = *stored;
}

QString RingStore::commit(const galleond::RingSettings &next)
{
    if (next == m_settings)
        return {};
    m_settings = next;
    Q_EMIT changed();
    if (m_path.isEmpty())
        return {};
    return galleond::writeRingSettings(m_path, next) ? QString() : tr("The settings of the rings could not be saved.");
}

QString RingStore::setLit(bool lit)
{
    galleond::RingSettings next = m_settings;
    next.lit = lit;
    return commit(next);
}

QString RingStore::setLevel(int level)
{
    if (level < 0 || level > galleond::RingLevelFull)
        return tr("The level of the rings is a number from 0 to 1000.");
    galleond::RingSettings next = m_settings;
    next.level = level;
    return commit(next);
}

QString RingStore::setLeftColour(const QString &colour)
{
    galleond::RingSettings next = m_settings;
    if (!galleond::parseRingColour(colour, &next.left))
        return tr("A ring colour is \"default\" or written as #rrggbb.");
    return commit(next);
}

QString RingStore::setRightColour(const QString &colour)
{
    galleond::RingSettings next = m_settings;
    if (!galleond::parseRingColour(colour, &next.right))
        return tr("A ring colour is \"default\" or written as #rrggbb.");
    return commit(next);
}

} // namespace bosun
