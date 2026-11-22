// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NightStateStore.h"

#include <NightMode.h>

#include <QDateTime>
#include <QLocale>

#include <utility>

namespace bosun {

NightStateStore::NightStateStore(QString path, Clock now, QObject *parent)
    : QObject(parent), m_path(std::move(path)), m_now(now ? std::move(now) : Clock([] { return QDateTime::currentSecsSinceEpoch(); })), m_watcher(m_path)
{
    if (m_path.isEmpty())
        return;
    connect(&m_watcher, &galleond::ReplacedFileWatcher::maybeChanged, this, &NightStateStore::refresh);
    m_look.setInterval(30000);
    connect(&m_look, &QTimer::timeout, this, &NightStateStore::refresh);
    m_look.start();
    refresh();
}

QString NightStateStore::endTime() const
{
    if (m_until <= 0)
        return {};
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(m_until).time(), QLocale::ShortFormat);
}

void NightStateStore::refresh()
{
    const qint64 until = m_path.isEmpty() ? 0 : galleond::readNightUntil(m_path, m_now()).value_or(0);
    if (until == m_until)
        return;
    m_until = until;
    Q_EMIT changed();
}

} // namespace bosun
