// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyGlowStore.h"

#include <utility>

namespace bosun {

KeyGlowStore::KeyGlowStore(QString path, bool supported, QObject *parent) : QObject(parent), m_path(std::move(path)), m_supported(supported)
{
    if (m_path.isEmpty() || !m_supported)
        return;
    m_choice = galleond::keyGlowChoice(m_path); // off and the default colour for a missing or damaged file
}

QString KeyGlowStore::colour() const
{
    return galleond::keyGlowColourText(m_choice.colour);
}

QString KeyGlowStore::save()
{
    if (m_path.isEmpty())
        return {};
    return galleond::writeKeyGlowChoice(m_path, m_choice) ? QString() : tr("The choice could not be saved.");
}

QString KeyGlowStore::setAllow(bool allow)
{
    if (!m_supported)
        return tr("This is not available on this system yet.");
    if (allow == m_choice.allow)
        return {};
    m_choice.allow = allow;
    Q_EMIT changed();
    return save();
}

QString KeyGlowStore::setColour(const QString &colour)
{
    if (!m_supported)
        return tr("This is not available on this system yet.");
    const auto parsed = galleond::parseKeyGlowColour(colour);
    if (!parsed)
        return tr("A colour is written as # and six digits or letters from a to f.");
    if (*parsed == m_choice.colour)
        return {};
    m_choice.colour = *parsed;
    Q_EMIT changed();
    return save();
}

} // namespace bosun
