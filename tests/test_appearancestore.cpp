// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The appearance as Bosun holds it: every change is written to lighting-appearance.json (what galleond reads), and the file is read again
// when the window starts. No keyboard and no daemon are involved.

#include "AppearanceStore.h"

#include <AppearanceFile.h>

#include <QSignalSpy>
#include <QDateTime>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

using namespace bosun;
using galleon::keyboard::BaseType;

class TestAppearanceStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheKeysAreDark()
    {
        const QTemporaryDir dir;
        const AppearanceStore store(dir.filePath("lighting-appearance.json"));
        QCOMPARE(store.baseType(), QStringLiteral("uniform"));
        QCOMPARE(store.colour(), QColor(0, 0, 0));
        QVERIFY(store.retouches().isEmpty());
        QCOMPARE(store.level(), 1000);
    }

    void aColourIsWrittenToTheFileAtOnce()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        QVERIFY(store.setUniform(10, 20, 30).isEmpty());
        const auto written = galleond::readAppearance(path);
        QVERIFY(written.has_value());
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).base, BaseType::Uniform);
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).colour.green, 20);
    }

    void theRainbowIsWrittenWithItsSpeedAndKeepsTheRetouches()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        QVERIFY(store.retouch(58, 255, 255, 255).isEmpty());
        QVERIFY(store.setRainbow(100).isEmpty());
        const auto written = galleond::readAppearance(path);
        QVERIFY(written.has_value());
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).base, BaseType::Rainbow);
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).rainbowSpeed, 100);
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).retouches.size(), std::size_t{1});
    }

    void theSkyIsWrittenAndKeepsTheRetouchesAndGivesTheColoursOfTheHourForTheDrawing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        QVERIFY(store.retouch(58, 255, 255, 255).isEmpty());
        QVERIFY(store.setSky().isEmpty());
        QCOMPARE(store.baseType(), QStringLiteral("sky"));
        const auto written = galleond::readAppearance(path);
        QVERIFY(written.has_value());
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).base, BaseType::Sky);
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).retouches.size(), std::size_t{1});

        // The colours of noon, as the keyboard gets them (the day palette of the mockup), and the strip dimmed.
        const QVariantMap noon = store.skyAt(12.0);
        const QVariantList rows = noon.value(QStringLiteral("rows")).toList();
        QCOMPARE(rows.size(), 6);
        QCOMPARE(rows.constFirst().toString(), QStringLiteral("#1a46aa")); // 26, 70, 170
        QCOMPARE(rows.constLast().toString(), QStringLiteral("#3c87e6"));  // 60, 135, 230
        QCOMPARE(noon.value(QStringLiteral("strip")).toString(), QStringLiteral("#102a66")); // 16, 42, 102
        QCOMPARE(AppearanceStore::skyRowOf(43.0), 0);
        QCOMPARE(AppearanceStore::skyRowOf(146.0), 5);
        QCOMPARE(AppearanceStore::skyRowOf(16.8), -1); // the strip
    }

    void followingTheSunIsOffByDefaultAndOnlyTheSkyCanFollowIt()
    {
        const QTemporaryDir dir;
        AppearanceStore store(dir.filePath("lighting-appearance.json"));
        QVERIFY(!store.followSun());
        QVERIFY(!store.setFollowSun(true).isEmpty()); // the base is one colour: refused, with a reason
        QVERIFY(!store.followSun());
        QVERIFY(store.setSky().isEmpty());
        QVERIFY(store.setFollowSun(true).isEmpty());
        QVERIFY(store.followSun());
        const auto written = galleond::readAppearance(dir.filePath("lighting-appearance.json")).value_or(galleon::keyboard::Appearance{});
        QVERIFY(written.followSun);
        QVERIFY(!written.skyPlace.has_value());
        QVERIFY(store.setFollowSun(false).isEmpty());
        QVERIFY(!galleond::readAppearance(dir.filePath("lighting-appearance.json")).value_or(galleon::keyboard::Appearance{}).followSun);
    }

    void thePlaceOfTheSkyIsTypedWithADecimalPointAndSavedOnlyWhenItIsAPlace()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        QVERIFY(store.setSky().isEmpty());
        QVERIFY(store.setFollowSun(true).isEmpty());
        QVERIFY(store.setSkyPlace(" 48.8566 ", "2.3522").isEmpty());
        QVERIFY(store.hasSkyPlace());
        QCOMPARE(store.skyLatitude(), QStringLiteral("48.8566"));
        QCOMPARE(store.skyLongitude(), QStringLiteral("2.3522"));
        const auto written = galleond::readAppearance(path).value_or(galleon::keyboard::Appearance{});
        QCOMPARE(written.skyPlace.value_or(galleon::keyboard::GeoPlace{}), (galleon::keyboard::GeoPlace{48.8566, 2.3522}));
        // What is not a place changes nothing.
        for (const auto &bad : {std::pair{"91", "2"}, std::pair{"48", "181"}, std::pair{"abc", "2"}, std::pair{"", ""}, std::pair{"48,85", "2.35"}, std::pair{"nan", "2"}, std::pair{"48", "inf"}})
            QVERIFY2(!store.setSkyPlace(bad.first, bad.second).isEmpty(), bad.first);
        QCOMPARE(store.skyLatitude(), QStringLiteral("48.8566"));
        // Forgotten: it leaves the file.
        QVERIFY(store.clearSkyPlace().isEmpty());
        QVERIFY(!store.hasSkyPlace());
        QVERIFY(!galleond::readAppearance(path).value_or(galleon::keyboard::Appearance{}).skyPlace.has_value());
        // And a place typed while the base is another one is not in the file at all (only the sky writes a place).
        QVERIFY(store.setSkyPlace("48.8566", "2.3522").isEmpty());
        QVERIFY(store.setUniform(1, 2, 3).isEmpty());
        QVERIFY(!QFile(path).readAll().contains("latitude"));
        QVERIFY(!galleond::readAppearance(path).value_or(galleon::keyboard::Appearance{}).skyPlace.has_value());
    }

    void theSourceOfTheSunriseIsTheWeatherPlaceThenTheOneOfTheSkyThenNoneAndNothingIsReadWhenOff()
    {
        const QTemporaryDir dir;
        const QString widgets = dir.filePath("widgets.json");
        const auto writeWidgets = [&widgets](const QByteArray &json) {
            QFile out(widgets);
            QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
            out.write(json);
        };
        AppearanceStore store(dir.filePath("lighting-appearance.json"), widgets);
        QCOMPARE(store.sunSource(), QStringLiteral("off")); // one colour
        QVERIFY(store.setSky().isEmpty());
        QCOMPARE(store.sunSource(), QStringLiteral("off")); // the sky, not following the sun
        QVERIFY(store.setFollowSun(true).isEmpty());
        QCOMPARE(store.sunSource(), QStringLiteral("none")); // following, no place anywhere
        QVERIFY(store.setSkyPlace("35.6762", "139.6503").isEmpty());
        QCOMPARE(store.sunSource(), QStringLiteral("sky"));
        writeWidgets(R"({"location":{"latitude":48.8566,"longitude":2.3522}})");
        QCOMPARE(store.sunSource(), QStringLiteral("weather")); // the weather widget comes first
        writeWidgets(R"({"location":{"latitude":999,"longitude":2.3522}})");
        QCOMPARE(store.sunSource(), QStringLiteral("sky")); // a damaged place is no place
        QVERIFY(store.setFollowSun(false).isEmpty());
        writeWidgets(R"({"location":{"latitude":48.8566,"longitude":2.3522}})");
        QCOMPARE(store.sunSource(), QStringLiteral("off"));
    }

    void theSkyOfThePreviewFollowsTheSunOfTheDayWhenAsked()
    {
        const QTemporaryDir dir;
        const auto paris = [] {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            const QTimeZone summerTime = QTimeZone::fromSecondsAheadOfUtc(7200);
#else
            const QTimeZone summerTime(7200);
#endif
            return QDateTime(QDate(2026, 6, 21), QTime(12, 0), summerTime);
        };
        AppearanceStore store(dir.filePath("lighting-appearance.json"), {}, paris);
        QVERIFY(store.setSky().isEmpty());
        const double sunset = galleon::keyboard::sunDay(48.8566, 2.3522, 2026, 6, 21, 2.0).sunsetHours;
        // Not following the sun: at the sunset of Paris the fixed hours are far into the night.
        QCOMPARE(store.skyAt(sunset).value(QStringLiteral("rows")).toList().constFirst().toString(), QStringLiteral("#0a0e26")); // 10, 14, 38
        QVERIFY(store.setFollowSun(true).isEmpty());
        QVERIFY(store.setSkyPlace("48.8566", "2.3522").isEmpty());
        QCOMPARE(store.skyAt(sunset).value(QStringLiteral("rows")).toList().constFirst().toString(), QStringLiteral("#1e165a")); // the dusk: 30, 22, 90
    }

    void aSecondRetouchOfTheSameLedReplacesTheFirst()
    {
        const QTemporaryDir dir;
        AppearanceStore store(dir.filePath("lighting-appearance.json"));
        QVERIFY(store.retouch(4, 255, 0, 0).isEmpty());
        QVERIFY(store.retouch(4, 0, 0, 255).isEmpty());
        QCOMPARE(store.retouches().size(), 1);
        QCOMPARE(store.retouches().constFirst().toMap().value("colour").toString(), QStringLiteral("#0000ff"));
    }

    void clearingTheRetouchesKeepsTheBase()
    {
        const QTemporaryDir dir;
        AppearanceStore store(dir.filePath("lighting-appearance.json"));
        QVERIFY(store.setRainbow(0).isEmpty());
        QVERIFY(store.retouch(4, 255, 0, 0).isEmpty());
        QVERIFY(store.clearRetouches().isEmpty());
        QVERIFY(store.retouches().isEmpty());
        QCOMPARE(store.baseType(), QStringLiteral("rainbow"));
    }

    void whatWasWrittenIsReadAgainWhenTheWindowStarts()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        {
            AppearanceStore store(path);
            QVERIFY(store.setRainbow(250).isEmpty());
            QVERIFY(store.retouch(58, 255, 255, 255).isEmpty());
            QVERIFY(store.setLevel(500).isEmpty());
        }
        const AppearanceStore again(path);
        QCOMPARE(again.baseType(), QStringLiteral("rainbow"));
        QCOMPARE(again.rainbowSpeed(), 250);
        QCOMPARE(again.level(), 500);
        QCOMPARE(again.retouches().size(), 1);
    }

    void aDamagedFileGivesDarkKeysInsteadOfAnError()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":1,"base":)");
        file.close();
        const AppearanceStore store(path);
        QCOMPARE(store.baseType(), QStringLiteral("uniform"));
        QCOMPARE(store.colour(), QColor(0, 0, 0));
    }

    void aValueOutOfRangeIsRefusedAndNothingChanges()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        const QSignalSpy changed(&store, &AppearanceStore::changed);
        QVERIFY(!store.setUniform(256, 0, 0).isEmpty());
        QVERIFY(!store.retouch(146, 0, 0, 0).isEmpty());
        QVERIFY(!store.retouch(-1, 0, 0, 0).isEmpty());
        QVERIFY(!store.setLevel(1001).isEmpty());
        QVERIFY(!store.setRainbow(1001).isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void aFileThatCannotBeWrittenStillChangesTheDrawing()
    {
        AppearanceStore store("/proc/nothing/lighting-appearance.json");
        QVERIFY(!store.setUniform(1, 2, 3).isEmpty()); // the reason is given
        QCOMPARE(store.colour(), QColor(1, 2, 3));     // and the drawing follows
    }

    void withoutAPathNothingIsWritten()
    {
        const QTemporaryDir dir;
        AppearanceStore store{QString()};
        QVERIFY(store.setUniform(1, 2, 3).isEmpty());
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }

    void everyChangeSaysSo()
    {
        const QTemporaryDir dir;
        AppearanceStore store(dir.filePath("lighting-appearance.json"));
        const QSignalSpy changed(&store, &AppearanceStore::changed);
        QVERIFY(store.setUniform(1, 2, 3).isEmpty());
        QVERIFY(store.retouch(5, 1, 1, 1).isEmpty());
        QVERIFY(store.reset().isEmpty());
        QCOMPARE(changed.count(), 3);
        QVERIFY(store.retouches().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAppearanceStore)
#include "test_appearancestore.moc"
