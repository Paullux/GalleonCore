// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ReplacedFileWatcher.h"

#include <galleon/keyboard/Appearance.h>
#include <galleon/keyboard/SkyPalette.h>
#include <galleon/keyboard/SunTimes.h>

#include <QDate>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>

namespace galleond {

class LightingController;

// Tells the lighting when the sky changes: the real sunrise and sunset of the place when the sky follows the sun, the fixed schedule of the mockup
// otherwise. It is the link between three things that do not know each other: the appearance (whether the sky follows the sun, and a place typed in
// its settings), the weather widget's place (widgets.json, read by WeatherPlace without going through HomeWidgets) and the lighting.
//
// Privacy, as decided by the owner: nothing about the place is read, kept or computed unless the sky is the base AND it follows the sun (off by default).
// widgets.json is read only then (at that moment, and again when the file changes); the place is never logged, never sent, never published (not in the
// lighting state, not in the trace). The place of the weather widget comes first, then the one of the sky settings, then the fixed hours.
//
// Threads: `day()` is called from the thread of the lighting, every few tens of milliseconds, so it is cheap and locked; everything else runs on the
// thread of the controller (the main thread), where the appearance is set and the file watcher fires.
class LightingSkyDay : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<QDateTime()>;

    // `widgetsPath` is the file of the weather widget; `clock` is the date and time in the zone of the computer (a test gives another).
    LightingSkyDay(LightingController &controller, QString widgetsPath, Clock clock = &LightingSkyDay::systemClock, QObject *parent = nullptr);

    // The date and time of the computer, in its zone (summer time included).
    [[nodiscard]] static QDateTime systemClock();

    // When the sky changes today. Safe to call from any thread.
    [[nodiscard]] galleon::keyboard::SkyDay day() const;

    // For the tests: how many times the weather file was read (it must stay 0 while the sky does not follow the sun).
    [[nodiscard]] int weatherReads() const { return m_weatherReads.load(); }

private:
    void onAppearanceSet(const galleon::keyboard::Appearance &appearance);
    void readWeatherPlace();

    QString m_widgetsPath;
    Clock m_clock;
    ReplacedFileWatcher m_watcher;

    mutable std::mutex m_mutex;
    bool m_follow = false;
    std::optional<galleon::keyboard::GeoPlace> m_skyPlace;
    std::optional<galleon::keyboard::GeoPlace> m_weatherPlace;
    mutable QDate m_cachedDate;
    mutable std::optional<galleon::keyboard::GeoPlace> m_cachedPlace;
    mutable bool m_cacheValid = false;
    mutable galleon::keyboard::SkyDay m_cachedDay;
    std::atomic<int> m_weatherReads{0};
};

} // namespace galleond
