// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The brightness curve of the day: the strict file format, the value for a time of day, the smooth curve of the
// editor and the snapping to steps of 100. Pure code and a temporary file; no keyboard is involved.

#include "BrightnessCurve.h"

#include <QTemporaryDir>
#include <QTest>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QTime>

using namespace galleond;

namespace {

BrightnessCurve ramp()
{
    BrightnessCurve curve;
    curve.mode = BrightnessCurve::Mode::Curve;
    for (int hour = 0; hour < BrightnessCurve::Hours; ++hour)
        curve.hours.at(static_cast<std::size_t>(hour)) = (hour % 11) * 100; // 0, 100 ... 1000, then 0 again
    return curve;
}

bool refused(const QByteArray &json) { return !parseBrightnessCurve(json).has_value(); }

QByteArray withHours(const QString &hours, const QString &mode = QStringLiteral("curve"))
{
    return QStringLiteral(R"({"version":1,"mode":"%1","hours":[%2]})").arg(mode, hours).toUtf8();
}

QString twentyFour(const QString &value)
{
    return QStringList(24, value).join(',');
}

} // namespace

class TestBrightnessCurve : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- the file ------------------------------------------------------------------------------------
    void aCurveSurvivesBeingEncodedAndParsed()
    {
        const BrightnessCurve curve = ramp();
        QCOMPARE(parseBrightnessCurve(encodeBrightnessCurve(curve)), std::optional<BrightnessCurve>(curve));
        const BrightnessCurve defaults = defaultBrightnessCurve();
        QCOMPARE(parseBrightnessCurve(encodeBrightnessCurve(defaults)), std::optional<BrightnessCurve>(defaults));
    }

    void theDefaultIsTheSliderWithAPointForEveryHourInSteps()
    {
        const BrightnessCurve curve = defaultBrightnessCurve();
        QCOMPARE(curve.mode, BrightnessCurve::Mode::Slider); // nothing is written by the curve until the user chooses it
        for (const int value : curve.hours) {
            QVERIFY(value >= 0 && value <= 1000);
            QCOMPARE(value % 100, 0);
        }
    }

    void bothModesAreKnown()
    {
        QVERIFY(parseBrightnessCurve(withHours(twentyFour("500"), "curve")));
        QVERIFY(parseBrightnessCurve(withHours(twentyFour("500"), "slider")));
        QVERIFY(refused(withHours(twentyFour("500"), "auto")));
        QVERIFY(refused(withHours(twentyFour("500"), "")));
        QVERIFY(refused(withHours(twentyFour("500"), "Curve")));
    }

    void exactlyTwentyFourPointsAreRequired()
    {
        QVERIFY(!refused(withHours(twentyFour("500"))));
        QVERIFY(refused(withHours(QStringList(23, "500").join(','))));
        QVERIFY(refused(withHours(QStringList(25, "500").join(','))));
        QVERIFY(refused(withHours("")));
    }

    void everyPointMustBeAWholeStepOfOneHundredFromZeroToAThousand()
    {
        for (const char *bad : {"-100", "1100", "1000.5", "450", "50", "1", "\"500\"", "null", "true", "[]", "{}"})
            QVERIFY2(refused(withHours(QStringList({QString::fromLatin1(bad), twentyFour("500").mid(0, 23 * 4 - 1)}).join(','))), bad);
        // One bad point refuses the whole file: nothing is partly read.
        QVERIFY(refused(withHours(twentyFour("500").replace(0, 3, "450"))));
    }

    void onlyTheThreeKeysOfTheFormatAreAccepted()
    {
        QVERIFY(refused(QByteArray(R"({"version":1,"mode":"curve"})")));
        QVERIFY(refused(QStringLiteral(R"({"version":1,"mode":"curve","hours":[%1],"extra":1})").arg(twentyFour("500")).toUtf8()));
        QVERIFY(refused(QStringLiteral(R"({"version":2,"mode":"curve","hours":[%1]})").arg(twentyFour("500")).toUtf8()));
        QVERIFY(refused(QStringLiteral(R"({"version":"1","mode":"curve","hours":[%1]})").arg(twentyFour("500")).toUtf8()));
        QVERIFY(refused("[]"));
        QVERIFY(refused("not json"));
        QVERIFY(refused(""));
    }

    void aFileOverTheSizeLimitIsRefused()
    {
        QByteArray big = encodeBrightnessCurve(ramp());
        big.append(QByteArray(static_cast<int>(BrightnessCurve::MaxBytes), ' '));
        QVERIFY(refused(big));
    }

    void theFileIsWrittenAtomicallyInANewFolderAndReadBack()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("new/brightness-curve.json");
        QVERIFY(writeBrightnessCurve(path, ramp()));
        QCOMPARE(readBrightnessCurve(path), std::optional<BrightnessCurve>(ramp()));
        BrightnessCurve other = ramp();
        other.mode = BrightnessCurve::Mode::Slider;
        QVERIFY(writeBrightnessCurve(path, other));
        QCOMPARE(readBrightnessCurve(path), std::optional<BrightnessCurve>(other));
        QCOMPARE(QDir(QFileInfo(path).absolutePath()).entryList(QDir::Files).size(), 1);
    }

    void aMissingFileReadsAsNothingAndThePathIsNextToThePagesFile()
    {
        const QTemporaryDir dir;
        QVERIFY(!readBrightnessCurve(dir.filePath("absent.json")));
        QVERIFY(brightnessCurvePath().endsWith(QStringLiteral("/galleoncore/brightness-curve.json")));
    }

    // ---- the value for a time of day ------------------------------------------------------------------
    void theValueIsThePointNearestToTheTime()
    {
        const BrightnessCurve curve = ramp(); // hour h holds (h % 11) * 100
        QCOMPARE(brightnessAt(curve, QTime(7, 0)), 700);
        QCOMPARE(brightnessAt(curve, QTime(7, 29)), 700);
        QCOMPARE(brightnessAt(curve, QTime(7, 30)), 800); // a tie goes to the later hour
        QCOMPARE(brightnessAt(curve, QTime(7, 59)), 800);
        QCOMPARE(brightnessAt(curve, QTime(0, 0)), 0);
        QCOMPARE(brightnessAt(curve, QTime(12, 10)), 100);
    }

    void theDayIsACircleSoJustBeforeMidnightTakesTheMidnightPoint()
    {
        BrightnessCurve curve = ramp();
        curve.hours.at(0) = 300;
        QCOMPARE(brightnessAt(curve, QTime(23, 29)), curve.hours.at(23));
        QCOMPARE(brightnessAt(curve, QTime(23, 30)), 300);
        QCOMPARE(brightnessAt(curve, QTime(23, 59, 59)), 300);
    }

    void theValueChangesAtMostOncePerHourSoTheKeyboardMemoryIsWrittenAtMostTwentyFourTimesADay()
    {
        BrightnessCurve curve;
        for (int hour = 0; hour < 24; ++hour)
            curve.hours.at(static_cast<std::size_t>(hour)) = (hour % 2) * 1000; // the worst case: every hour differs
        int changes = 0;
        int previous = brightnessAt(curve, QTime(0, 0));
        for (int minute = 1; minute < 24 * 60; ++minute) {
            const int value = brightnessAt(curve, QTime(minute / 60, minute % 60));
            changes += value != previous ? 1 : 0;
            previous = value;
        }
        QVERIFY2(changes <= 24, qPrintable(QString::number(changes)));
    }

    void everyMinuteOfTheDayGivesAValidStep()
    {
        const BrightnessCurve curve = defaultBrightnessCurve();
        for (int minute = 0; minute < 24 * 60; ++minute) {
            const int value = brightnessAt(curve, QTime(minute / 60, minute % 60));
            QVERIFY(value >= 0 && value <= 1000 && value % 100 == 0);
        }
    }

    // ---- the smooth curve of the editor ---------------------------------------------------------------
    void theSmoothCurvePassesThroughEveryPointAtItsHour()
    {
        const auto points = ramp().hours;
        for (int hour = 0; hour < 24; ++hour)
            QCOMPARE(smoothBrightness(points, hour), static_cast<double>(points.at(static_cast<std::size_t>(hour))));
    }

    void theSmoothCurveWrapsAroundMidnightWithoutAJump()
    {
        auto points = ramp().hours;
        points.at(23) = 400;
        points.at(0) = 400;
        // Just before and just after midnight the curve is continuous.
        QVERIFY(std::abs(smoothBrightness(points, 23.999) - smoothBrightness(points, 0.0)) < 5.0);
        QVERIFY(std::abs(smoothBrightness(points, 24.0) - smoothBrightness(points, 0.0)) < 1e-9);
        QVERIFY(std::abs(smoothBrightness(points, -0.001) - smoothBrightness(points, 23.999)) < 1e-9);
    }

    void theSmoothCurveNeverLeavesTheScaleEvenWhereASplineWouldOvershoot()
    {
        std::array<int, 24> points{};
        points.at(12) = 1000; // a spike: the spline dips below 0 next to it unless clamped
        for (int tick = 0; tick < 480; ++tick) { // every 0.05 hour
            const double value = smoothBrightness(points, tick * 0.05);
            QVERIFY(value >= 0.0 && value <= 1000.0);
        }
    }

    void aFlatCurveStaysFlat()
    {
        std::array<int, 24> points{};
        points.fill(600);
        for (int tick = 0; tick < 96; ++tick) // every quarter of an hour
            QCOMPARE(smoothBrightness(points, tick * 0.25), 600.0);
    }

    // ---- snapping what the editor drags ---------------------------------------------------------------
    void aDraggedValueIsSnappedToTheNearestStepOfOneHundred()
    {
        QCOMPARE(snapBrightness(0.0), 0);
        QCOMPARE(snapBrightness(49.9), 0);
        QCOMPARE(snapBrightness(50.0), 100); // a tie goes up
        QCOMPARE(snapBrightness(449.0), 400);
        QCOMPARE(snapBrightness(450.0), 500);
        QCOMPARE(snapBrightness(1000.0), 1000);
    }

    void aDraggedValueOutsideTheScaleOrNotANumberIsBroughtBackInside()
    {
        QCOMPARE(snapBrightness(-300.0), 0);
        QCOMPARE(snapBrightness(5000.0), 1000);
        QCOMPARE(snapBrightness(std::nan("")), 0);
        QCOMPARE(snapBrightness(std::numeric_limits<double>::infinity()), 0);
    }

    void aSnappedCurveIsAlwaysAFileThatParses()
    {
        BrightnessCurve curve;
        curve.mode = BrightnessCurve::Mode::Curve;
        const auto points = defaultBrightnessCurve().hours;
        for (int hour = 0; hour < 24; ++hour)
            curve.hours.at(static_cast<std::size_t>(hour)) = snapBrightness(smoothBrightness(points, hour + 0.37));
        QVERIFY(parseBrightnessCurve(encodeBrightnessCurve(curve)).has_value());
    }
};

QTEST_GUILESS_MAIN(TestBrightnessCurve)
#include "test_brightnesscurve.moc"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
