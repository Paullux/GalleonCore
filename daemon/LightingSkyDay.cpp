// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingSkyDay.h"

#include "LightingController.h"
#include "SkySun.h"
#include "WeatherPlace.h"

#include <utility>

namespace galleond {

LightingSkyDay::LightingSkyDay(LightingController &controller, QString widgetsPath, Clock clock, QObject *parent)
    : QObject(parent), m_widgetsPath(std::move(widgetsPath)), m_clock(std::move(clock)), m_watcher(m_widgetsPath)
{
    connect(&controller, &LightingController::appearanceSet, this, &LightingSkyDay::onAppearanceSet);
    // The file is only watched (its being replaced), not read, until the sky follows the sun.
    connect(&m_watcher, &ReplacedFileWatcher::maybeChanged, this, [this] {
        bool follow = false;
        {
            const std::scoped_lock lock(m_mutex);
            follow = m_follow;
        }
        if (follow)
            readWeatherPlace();
    });
    controller.setSkyDay([this] { return day(); });
}

QDateTime LightingSkyDay::systemClock()
{
    return QDateTime::currentDateTime();
}

void LightingSkyDay::onAppearanceSet(const galleon::keyboard::Appearance &appearance)
{
    const bool follow = appearance.base == galleon::keyboard::BaseType::Sky && appearance.followSun;
    {
        const std::scoped_lock lock(m_mutex);
        m_follow = follow;
        m_skyPlace = follow ? appearance.skyPlace : std::nullopt; // the place of the sky is kept only while it is used
        if (!follow)
            m_weatherPlace.reset(); // nothing about the place is kept when it is not used
        m_cacheValid = false;
    }
    if (follow)
        readWeatherPlace();
}

void LightingSkyDay::readWeatherPlace()
{
    const auto place = galleond::readWeatherPlace(m_widgetsPath);
    ++m_weatherReads;
    const std::scoped_lock lock(m_mutex);
    m_weatherPlace = place;
    m_cacheValid = false;
}

galleon::keyboard::SkyDay LightingSkyDay::day() const
{
    const QDateTime now = m_clock();
    const std::scoped_lock lock(m_mutex);
    if (!m_follow)
        return {};
    const auto place = galleon::keyboard::choosePlace(m_weatherPlace, m_skyPlace);
    if (!m_cacheValid || m_cachedDate != now.date() || m_cachedPlace != place) {
        m_cachedDay = skyDayFor(true, place, now.date(), now.offsetFromUtc() / 3600.0);
        m_cachedDate = now.date();
        m_cachedPlace = place;
        m_cacheValid = true;
    }
    return m_cachedDay;
}

} // namespace galleond
