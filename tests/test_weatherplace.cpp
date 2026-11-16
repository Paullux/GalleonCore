// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The place of the weather widget, read from widgets.json for the sky that follows the sun: only the two numbers of the place are taken, and
// anything out of shape gives no place. The file is the person's own, so it is untrusted input.

#include "SkySun.h"
#include "WeatherPlace.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;
using galleon::keyboard::GeoPlace;

namespace {

QByteArray widgets(const QByteArray &location)
{
    return QByteArray(R"({"version":1,"layout":[{"id":"clock","enabled":true}],"location":)") + location + R"(,"fps":{}})";
}

} // namespace

class TestWeatherPlace : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theLatitudeAndTheLongitudeAreTakenAndNothingElseOfTheFile()
    {
        const auto place = parseWeatherPlace(widgets(R"({"latitude":48.8566,"longitude":2.3522,"city":"Paris"})"));
        QVERIFY(place.has_value());
        QCOMPARE(place.value_or(GeoPlace{}), (GeoPlace{48.8566, 2.3522}));
    }

    void aFileWithNoPlaceGivesNoPlace()
    {
        QVERIFY(!parseWeatherPlace(R"({"version":1,"layout":[]})").has_value()); // the weather was never used
        QVERIFY(!parseWeatherPlace(widgets("{}")).has_value());
        QVERIFY(!parseWeatherPlace(widgets(R"({"city":"Paris"})")).has_value()); // a label alone is not a place
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":48.8})")).has_value());  // half of a place
        QVERIFY(!parseWeatherPlace(widgets(R"({"longitude":2.3})")).has_value());
    }

    void aPlaceThatIsNotTwoNumbersInRangeIsNotAPlace()
    {
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":"48.8","longitude":"2.3"})")).has_value()); // text, not numbers
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":91,"longitude":2.3})")).has_value());
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":-91,"longitude":2.3})")).has_value());
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":48.8,"longitude":180.1})")).has_value());
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":48.8,"longitude":-181})")).has_value());
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":1e999,"longitude":2.3})")).has_value());
        QVERIFY(!parseWeatherPlace(widgets(R"({"latitude":null,"longitude":true})")).has_value());
        QVERIFY(!parseWeatherPlace(widgets(R"([48.8,2.3])")).has_value()); // not an object
        QVERIFY(parseWeatherPlace(widgets(R"({"latitude":-90,"longitude":180})")).has_value()); // the limits themselves are places
    }

    void aDamagedOrOversizedFileGivesNoPlace()
    {
        QVERIFY(!parseWeatherPlace({}).has_value());
        QVERIFY(!parseWeatherPlace(R"({"location":{"latitude":48.8,"longitude":)").has_value()); // cut short
        QVERIFY(!parseWeatherPlace("not json at all").has_value());
        QVERIFY(!parseWeatherPlace(R"(["location"])").has_value());
        QByteArray big = widgets(R"({"latitude":48.8,"longitude":2.3})");
        big.append(QByteArray(MaxWidgetsBytes, ' ')); // a good file made too large
        QVERIFY(!parseWeatherPlace(big).has_value());
    }

    void theSkyOfADayIsTheRealSunWhenItFollowsTheSunAndHasAPlaceAndTheFixedScheduleOtherwise()
    {
        const GeoPlace paris{48.8566, 2.3522};
        const QDate solstice(2026, 6, 21);
        const auto sun = galleon::keyboard::sunDay(paris.latitude, paris.longitude, 2026, 6, 21, 2.0);
        const auto followed = skyDayFor(true, paris, solstice, 2.0);
        QCOMPARE(followed.sunriseHours, sun.sunriseHours);
        QCOMPARE(followed.dayLengthHours, sun.dayLengthHours);
        const galleon::keyboard::SkyDay fixed; // the schedule of the mockup
        QCOMPARE(skyDayFor(false, paris, solstice, 2.0), fixed);          // the sky does not follow the sun
        QCOMPARE(skyDayFor(true, std::nullopt, solstice, 2.0), fixed);    // no place known
        QCOMPARE(skyDayFor(true, GeoPlace{95.0, 0.0}, solstice, 2.0), fixed); // a place out of range
        QCOMPARE(skyDayFor(true, paris, QDate(), 2.0), fixed);            // no date
        QCOMPARE(skyDayFor(true, GeoPlace{69.6496, 18.956}, QDate(2026, 12, 21), 1.0), fixed); // polar night
        QCOMPARE(skyDayFor(true, GeoPlace{69.6496, 18.956}, solstice, 2.0), fixed);            // midnight sun
    }

    void theFileIsReadFromTheDiskAndAMissingOneGivesNoPlace()
    {
        const QTemporaryDir dir;
        QVERIFY(!readWeatherPlace(dir.filePath("widgets.json")).has_value());
        QFile out(dir.filePath("widgets.json"));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(widgets(R"({"latitude":-33.8688,"longitude":151.2093})"));
        out.close();
        QCOMPARE(readWeatherPlace(dir.filePath("widgets.json")).value_or(GeoPlace{}), (GeoPlace{-33.8688, 151.2093}));
        QVERIFY(widgetsPath().endsWith(QStringLiteral("/galleoncore/widgets.json")));
    }
};

QTEST_GUILESS_MAIN(TestWeatherPlace)
#include "test_weatherplace.moc"
