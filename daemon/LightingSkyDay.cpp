// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingSkyDay.h"

#include "LightingController.h"
#include "SkySun.h"
#include "WeatherPlace.h"

#include <utility>

namespace galleond {

struct LightingSkyDay::State
{
    Clock clock;
    mutable std::mutex mutex;
    bool follow = false;
    std::optional<galleon::keyboard::GeoPlace> skyPlace;
    std::optional<galleon::keyboard::GeoPlace> weatherPlace;
    mutable QDate cachedDate;
    mutable std::optional<galleon::keyboard::GeoPlace> cachedPlace;
    mutable bool cacheValid = false;
    mutable galleon::keyboard::SkyDay cachedDay;

    [[nodiscard]] galleon::keyboard::SkyDay day() const;
};

LightingSkyDay::LightingSkyDay(LightingController &controller, QString widgetsPath, Clock clock, QObject *parent)
    : QObject(parent), m_widgetsPath(std::move(widgetsPath)), m_watcher(m_widgetsPath), m_state(std::make_shared<State>())
{
    m_state->clock = std::move(clock);
    connect(&controller.announcer(), &AppearanceAnnouncer::appearanceSet, this, &LightingSkyDay::onAppearanceSet);
    // The file is only watched (its being replaced), not read, until the sky follows the sun.
    connect(&m_watcher, &ReplacedFileWatcher::maybeChanged, this, [this] {
        bool follow = false;
        {
            const std::scoped_lock lock(m_state->mutex);
            follow = m_state->follow;
        }
        if (follow)
            readWeatherPlace();
    });
    // The function holds the state, not this object: see State.
    controller.setSkyDay([state = m_state] { return state->day(); });
}

QDateTime LightingSkyDay::systemClock()
{
    return QDateTime::currentDateTime();
}

void LightingSkyDay::onAppearanceSet(const galleon::keyboard::Appearance &appearance)
{
    const bool follow = appearance.base == galleon::keyboard::BaseType::Sky && appearance.followSun;
    {
        const std::scoped_lock lock(m_state->mutex);
        m_state->follow = follow;
        m_state->skyPlace = follow ? appearance.skyPlace : std::nullopt; // the place of the sky is kept only while it is used
        if (!follow)
            m_state->weatherPlace.reset(); // nothing about the place is kept when it is not used
        m_state->cacheValid = false;
    }
    if (follow)
        readWeatherPlace();
}

void LightingSkyDay::readWeatherPlace()
{
    const auto place = galleond::readWeatherPlace(m_widgetsPath);
    ++m_weatherReads;
    const std::scoped_lock lock(m_state->mutex);
    m_state->weatherPlace = place;
    m_state->cacheValid = false;
}

galleon::keyboard::SkyDay LightingSkyDay::day() const
{
    return m_state->day();
}

galleon::keyboard::SkyDay LightingSkyDay::State::day() const
{
    const QDateTime now = clock();
    const std::scoped_lock lock(mutex);
    if (!follow)
        return {};
    const auto place = galleon::keyboard::choosePlace(weatherPlace, skyPlace);
    if (!cacheValid || cachedDate != now.date() || cachedPlace != place) {
        cachedDay = skyDayFor(true, place, now.date(), now.offsetFromUtc() / 3600.0);
        cachedDate = now.date();
        cachedPlace = place;
        cacheValid = true;
    }
    return cachedDay;
}

} // namespace galleond
