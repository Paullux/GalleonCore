// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppearanceStore.h"

#include "AppearanceFile.h"

#include "SkySun.h"
#include "WeatherPlace.h"

#include <galleon/keyboard/SkyPalette.h>

#include <QLocale>

#include <algorithm>
#include <cmath>
#include <utility>

namespace bosun {

namespace {

using galleon::keyboard::Appearance;
using galleon::keyboard::BaseType;
using galleon::keyboard::Retouch;

[[nodiscard]] bool isByte(int value)
{
    return value >= 0 && value <= 255;
}

} // namespace

AppearanceStore::AppearanceStore(QString path, QString widgetsPath, Clock clock, QObject *parent)
    : QObject(parent), m_path(std::move(path)), m_widgetsPath(std::move(widgetsPath)), m_clock(std::move(clock))
{
    if (m_path.isEmpty())
        return;
    if (const auto stored = galleond::readAppearance(m_path))
        m_appearance = *stored;
}

QDateTime AppearanceStore::systemClock()
{
    return QDateTime::currentDateTime();
}

QString AppearanceStore::baseType() const
{
    switch (m_appearance.base) {
    case BaseType::Rainbow:
        return QStringLiteral("rainbow");
    case BaseType::Sky:
        return QStringLiteral("sky");
    case BaseType::Uniform:
        break;
    }
    return QStringLiteral("uniform");
}

QColor AppearanceStore::colour() const
{
    return {m_appearance.colour.red, m_appearance.colour.green, m_appearance.colour.blue};
}

QVariantList AppearanceStore::retouches() const
{
    QVariantList list;
    for (const Retouch &retouch : m_appearance.retouches)
        list.append(QVariantMap{{QStringLiteral("led"), retouch.led},
                                {QStringLiteral("colour"), QColor(retouch.colour.red, retouch.colour.green, retouch.colour.blue).name()}});
    return list;
}

QString AppearanceStore::commit(const Appearance &next)
{
    if (!galleon::keyboard::isValid(next))
        return tr("This appearance is not valid.");
    m_appearance = next; // kept in memory first: the drawing follows even if the file cannot be written
    Q_EMIT changed();
    if (m_path.isEmpty())
        return {};
    return galleond::writeAppearance(m_path, next) ? QString() : tr("The appearance could not be saved.");
}

QString AppearanceStore::setUniform(int red, int green, int blue)
{
    if (!isByte(red) || !isByte(green) || !isByte(blue))
        return tr("A colour is made of three numbers from 0 to 255.");
    Appearance next = m_appearance;
    next.base = BaseType::Uniform;
    next.colour = {static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green), static_cast<std::uint8_t>(blue)};
    return commit(next);
}

QString AppearanceStore::setRainbow(int speed)
{
    Appearance next = m_appearance;
    next.base = BaseType::Rainbow;
    next.rainbowSpeed = speed;
    return commit(next);
}

QString AppearanceStore::setSky()
{
    Appearance next = m_appearance;
    next.base = BaseType::Sky;
    return commit(next);
}

QVariantMap AppearanceStore::skyAt(double hours) const
{
    const auto colourName = [](const galleon::keyboard::Rgb &c) { return QColor(c.red, c.green, c.blue).name(); };
    const auto rows = galleon::keyboard::skyRowColours(hours, skyDay());
    QVariantList names;
    for (const auto &row : rows)
        names.append(colourName(row));
    return {{QStringLiteral("rows"), names}, {QStringLiteral("strip"), colourName(galleon::keyboard::skyStripColour(rows.front()))}};
}

int AppearanceStore::skyRowOf(double y)
{
    const auto place = galleon::keyboard::skyPlaceOf(y);
    return place.strip ? -1 : place.row;
}

QString AppearanceStore::setFollowSun(bool follow)
{
    if (m_appearance.base != BaseType::Sky)
        return tr("Choose the sky first: only the sky follows the sun.");
    Appearance next = m_appearance;
    next.followSun = follow;
    return commit(next);
}

QString AppearanceStore::setSkyPlace(const QString &latitude, const QString &longitude)
{
    bool latitudeOk = false;
    bool longitudeOk = false;
    const double lat = QLocale::c().toDouble(latitude.trimmed(), &latitudeOk);
    const double lon = QLocale::c().toDouble(longitude.trimmed(), &longitudeOk);
    if (!latitudeOk || !longitudeOk || !galleon::keyboard::isValidPlace(lat, lon))
        return tr("Enter latitude (-90…90) and longitude (-180…180), using a decimal point.");
    Appearance next = m_appearance;
    next.skyPlace = galleon::keyboard::GeoPlace{lat, lon};
    return commit(next);
}

QString AppearanceStore::clearSkyPlace()
{
    Appearance next = m_appearance;
    next.skyPlace.reset();
    return commit(next);
}

QString AppearanceStore::skyLatitude() const
{
    return m_appearance.skyPlace ? QString::number(m_appearance.skyPlace->latitude, 'g', 12) : QString();
}

QString AppearanceStore::skyLongitude() const
{
    return m_appearance.skyPlace ? QString::number(m_appearance.skyPlace->longitude, 'g', 12) : QString();
}

std::optional<galleon::keyboard::GeoPlace> AppearanceStore::placeInUse() const
{
    if (m_appearance.base != BaseType::Sky || !m_appearance.followSun)
        return std::nullopt; // nothing about a place is read unless the sky follows the sun
    const auto weather = m_widgetsPath.isEmpty() ? std::nullopt : galleond::readWeatherPlace(m_widgetsPath);
    return galleon::keyboard::choosePlace(weather, m_appearance.skyPlace);
}

QString AppearanceStore::sunSource() const
{
    if (m_appearance.base != BaseType::Sky || !m_appearance.followSun)
        return QStringLiteral("off");
    const auto weather = m_widgetsPath.isEmpty() ? std::nullopt : galleond::readWeatherPlace(m_widgetsPath);
    if (weather && galleon::keyboard::isValidPlace(weather->latitude, weather->longitude))
        return QStringLiteral("weather");
    if (m_appearance.skyPlace)
        return QStringLiteral("sky");
    return QStringLiteral("none");
}

galleon::keyboard::SkyDay AppearanceStore::skyDay() const
{
    const QDateTime now = m_clock();
    return galleond::skyDayFor(m_appearance.base == BaseType::Sky && m_appearance.followSun, placeInUse(), now.date(), now.offsetFromUtc() / 3600.0);
}

QString AppearanceStore::retouch(int led, int red, int green, int blue)
{
    if (led < 0 || led >= static_cast<int>(galleon::keyboard::LiveLedCount) || !isByte(red) || !isByte(green) || !isByte(blue))
        return tr("A LED is 0 to 145 and a colour is made of three numbers from 0 to 255.");
    Appearance next = m_appearance;
    const Retouch wanted{static_cast<std::uint16_t>(led), {static_cast<std::uint8_t>(red), static_cast<std::uint8_t>(green), static_cast<std::uint8_t>(blue)}};
    const auto same = std::ranges::find(next.retouches, wanted.led, &Retouch::led);
    if (same != next.retouches.end())
        *same = wanted;
    else
        next.retouches.push_back(wanted);
    return commit(next);
}

QString AppearanceStore::clearRetouches()
{
    Appearance next = m_appearance;
    next.retouches.clear();
    return commit(next);
}

QString AppearanceStore::setLevel(int level)
{
    Appearance next = m_appearance;
    next.level = level;
    return commit(next);
}

QString AppearanceStore::reset()
{
    return commit(Appearance{});
}

} // namespace bosun
