// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NightController.h"

#include "NightMode.h"

#include <galleon/Protocol.h>
#include <galleon/StreamDeck.h>

#include <QColor>
#include <QDebug>
#include <QFile>
#include <QImage>

#include <utility>

namespace galleond {
namespace {

constexpr int DefaultTickMs = 15000;

QImage black(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::black);
    return image;
}

} // namespace

NightController::NightController(galleon::StreamDeck *deck, QString path, Clock now, QObject *parent)
    : QObject(parent), m_deck(deck), m_path(std::move(path)), m_now(now ? std::move(now) : Clock([] { return QDateTime::currentDateTime(); }))
{
    m_tick.setInterval(DefaultTickMs);
    connect(&m_tick, &QTimer::timeout, this, &NightController::tick);
}

qint64 NightController::nowSeconds() const
{
    return m_now().toSecsSinceEpoch();
}

bool NightController::start(int hour, int minute)
{
    const QDateTime end = nightEnd(m_now(), hour, minute);
    if (!end.isValid())
        return false;
    begin(end.toSecsSinceEpoch());
    return true;
}

void NightController::begin(qint64 until)
{
    const bool wasActive = isActive();
    m_until = until;
    if (!m_path.isEmpty() && !writeNightUntil(m_path, until))
        qWarning() << "galleond: the end of the night could not be saved: a restart of galleond would end it";
    m_tick.start();
    if (m_deck->isOpen())
        m_deck->setBrightness(0); // dark at once: what the others do when they are put to sleep (below) is not seen
    qInfo() << "galleond: lights off until" << QDateTime::fromSecsSinceEpoch(until).toString(Qt::ISODate);
    if (!wasActive)
        Q_EMIT activeChanged(true); // the others go quiet first (a screensaver that wakes redraws), then the black is sent over whatever they drew
    apply();
}

bool NightController::resume()
{
    if (m_path.isEmpty())
        return false;
    QString why;
    const std::optional<qint64> until = readNightUntil(m_path, nowSeconds(), &why);
    if (!until) {
        if (QFile::exists(m_path)) {
            qInfo() << "galleond: the night file is not used (" << why << "): removed";
            static_cast<void>(removeNightFile(m_path)); // so that Bosun does not show a night that is over
        }
        return false;
    }
    begin(*until);
    return true;
}

void NightController::end()
{
    if (!isActive())
        return;
    m_tick.stop();
    m_until = 0;
    if (!m_path.isEmpty() && !removeNightFile(m_path))
        qWarning() << "galleond: the night file could not be removed";
    // The brightness first, so that the pages drawn when the others wake up are seen.
    if (m_deck->isOpen() && !m_deck->setBrightness(NightDayBrightness))
        qWarning() << "galleond: could not give the brightness of the module back";
    qInfo() << "galleond: the night is over";
    Q_EMIT activeChanged(false);
}

void NightController::reapply()
{
    if (isActive())
        apply();
}

void NightController::apply()
{
    if (!m_deck->isOpen())
        return; // sent again when the module is back (reapply)
    // Dark first, then the pictures: the brightness already hides the change.
    bool ok = m_deck->setBrightness(0);
    ok = m_deck->setScreenImage(black(galleon::protocol::ScreenWidth, galleon::protocol::ScreenHeight)) && ok;
    for (int key = 0; key < galleon::protocol::KeyCount; ++key)
        ok = m_deck->setKeyImage(key, black(galleon::protocol::KeyImageSize, galleon::protocol::KeyImageSize)) && ok;
    for (int dial = 0; dial < galleon::protocol::DialCount; ++dial)
        ok = m_deck->setDialRing(dial, 0, 0, 0) && ok;
    if (!ok)
        qWarning() << "galleond: the module did not take all of the night: it is tried again when it comes back";
}

void NightController::tick()
{
    if (isActive() && nowSeconds() >= m_until)
        end();
}

} // namespace galleond
