// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// What Bosun shows and edits of the brightness curve: the mode, the 24 points, the file next to the pages file.
// A temporary folder holds the file; no keyboard is involved.

#include "BrightnessCurveModel.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using bosun::BrightnessCurveModel;
using namespace galleond;

namespace {

BrightnessCurve curveOf(BrightnessCurve::Mode mode, int value)
{
    BrightnessCurve curve;
    curve.mode = mode;
    curve.hours.fill(value);
    return curve;
}

} // namespace

class TestBrightnessCurveModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileItShowsTheDefaultInSliderModeAndWritesNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        const BrightnessCurveModel model(path);
        QCOMPARE(model.mode(), QStringLiteral("slider"));
        QCOMPARE(model.hours().size(), 24);
        QVERIFY(model.problem().isEmpty());
        QVERIFY(!QFile::exists(path)); // opening the window changes nothing on disk
    }

    void aSavedCurveIsShownWithItsModeAndPoints()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        QVERIFY(writeBrightnessCurve(path, curveOf(BrightnessCurve::Mode::Curve, 300)));
        const BrightnessCurveModel model(path);
        QCOMPARE(model.mode(), QStringLiteral("curve"));
        QCOMPARE(model.hours().at(5).toInt(), 300);
    }

    void changingTheModeSavesIt()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        BrightnessCurveModel model(path);
        QVERIFY(model.setMode("curve").isEmpty());
        QCOMPARE(readBrightnessCurve(path).value_or(BrightnessCurve{}).mode, BrightnessCurve::Mode::Curve);
        QVERIFY(model.setMode("slider").isEmpty());
        QCOMPARE(readBrightnessCurve(path).value_or(BrightnessCurve{}).mode, BrightnessCurve::Mode::Slider);
    }

    void anUnknownModeIsRefusedAndNothingChanges()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        BrightnessCurveModel model(path);
        QVERIFY(!model.setMode("auto").isEmpty());
        QVERIFY(!model.setMode("").isEmpty());
        QCOMPARE(model.mode(), QStringLiteral("slider"));
        QVERIFY(!QFile::exists(path));
    }

    void aPointIsBroughtToTheNearestStepOfOneHundredAndSaved()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        BrightnessCurveModel model(path);
        QVERIFY(model.setHour(7, 237.0).isEmpty()); // the default holds 600 there
        QCOMPARE(model.hours().at(7).toInt(), 200);
        QCOMPARE(readBrightnessCurve(path).value_or(BrightnessCurve{}).hours.at(7), 200);
        QVERIFY(model.setHour(7, 250.0).isEmpty());
        QCOMPARE(model.hours().at(7).toInt(), 300); // a tie goes up
    }

    void aPointOutsideTheScaleIsBroughtBackInsideAndAnHourThatDoesNotExistIsRefused()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        BrightnessCurveModel model(path);
        QVERIFY(model.setHour(0, -400.0).isEmpty());
        QVERIFY(model.setHour(1, 99999.0).isEmpty());
        QCOMPARE(model.hours().at(0).toInt(), 0);
        QCOMPARE(model.hours().at(1).toInt(), 1000);
        QVERIFY(!model.setHour(-1, 500.0).isEmpty());
        QVERIFY(!model.setHour(24, 500.0).isEmpty());
        QVERIFY(readBrightnessCurve(path).has_value());
    }

    void everythingTheEditorSavesIsAFileThatParses()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        BrightnessCurveModel model(path);
        for (int hour = 0; hour < 24; ++hour)
            QVERIFY(model.setHour(hour, hour * 41.3).isEmpty());
        QVERIFY(model.setMode("curve").isEmpty());
        QVERIFY(readBrightnessCurve(path).has_value());
    }

    void settingTheSameValueAgainWritesAndSignalsNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        BrightnessCurveModel model(path);
        QVERIFY(model.setHour(3, 400.0).isEmpty());
        const QSignalSpy changed(&model, &BrightnessCurveModel::changed);
        QVERIFY(model.setHour(3, 420.0).isEmpty()); // snaps to the same 400
        QVERIFY(model.setMode("slider").isEmpty());  // already the mode
        QCOMPARE(changed.size(), 0);
    }

    void aDamagedFileIsNotUsedItIsSaidAndTheNextEditReplacesIt()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        QFile damaged(path);
        QVERIFY(damaged.open(QIODevice::WriteOnly));
        damaged.write(R"({"version":1,"mode":"curve","hours":[450]})");
        damaged.close();
        BrightnessCurveModel model(path);
        QVERIFY(!model.problem().isEmpty());
        QCOMPARE(model.mode(), QStringLiteral("slider")); // never a half-read curve
        QVERIFY(model.setHour(2, 500.0).isEmpty());
        QVERIFY(model.problem().isEmpty());
        QVERIFY(readBrightnessCurve(path).has_value());
    }

    void aChangeOfTheFileBehindTheWindowIsShown()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        const BrightnessCurveModel model(path);
        const QSignalSpy changed(&model, &BrightnessCurveModel::changed);
        QVERIFY(writeBrightnessCurve(path, curveOf(BrightnessCurve::Mode::Curve, 900)));
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(model.mode(), QStringLiteral("curve"));
        QCOMPARE(model.hours().at(12).toInt(), 900);
    }

    void itsOwnSaveDoesNotComeBackAsAChange()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        BrightnessCurveModel model(path);
        const QSignalSpy changed(&model, &BrightnessCurveModel::changed);
        QVERIFY(model.setHour(4, 800.0).isEmpty());
        QCOMPARE(changed.size(), 1);
        QTest::qWait(300); // the watcher sees the file it just wrote: same curve, so nothing more
        QCOMPARE(changed.size(), 1);
    }

    void aFileThatCannotBeWrittenIsReportedAndTheWindowKeepsWhatItShowed()
    {
        const QTemporaryDir dir;
        QFile blocker(dir.filePath("file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        BrightnessCurveModel model(dir.filePath("file/brightness-curve.json"));
        const int before = model.hours().at(6).toInt();
        QVERIFY(!model.setHour(6, before == 500 ? 100.0 : 500.0).isEmpty());
        QCOMPARE(model.hours().at(6).toInt(), before);
        QVERIFY(!model.setMode("curve").isEmpty());
        QCOMPARE(model.mode(), QStringLiteral("slider"));
    }

    void dragPreviewsDoNotChangeOrSaveAnything()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("brightness-curve.json");
        const BrightnessCurveModel model(path);
        const double plain = model.smooth(10.0);
        const double dragged = model.smoothWith(10.0, 10, 0.0);
        QVERIFY(dragged < plain || plain == 0.0); // the point at 10 h pulled to 0
        QCOMPARE(model.smooth(10.0), plain);
        QVERIFY(!QFile::exists(path));
        QCOMPARE(model.smoothWith(10.0, -1, 0.0), plain); // no such hour: no override
    }

    void theSmoothCurveOfTheModelIsTheOneOfTheDaemon()
    {
        const QTemporaryDir dir;
        const BrightnessCurveModel model(dir.filePath("brightness-curve.json"));
        const auto points = defaultBrightnessCurve().hours;
        for (int tick = 0; tick < 48; ++tick)
            QCOMPARE(model.smooth(tick * 0.5), smoothBrightness(points, tick * 0.5));
    }

    void thePointMarkedNowIsTheOneTheDaemonUses()
    {
        const QTemporaryDir dir;
        const BrightnessCurveModel model(dir.filePath("brightness-curve.json"));
        QCOMPARE(model.pointForTime(7, 29), 7);
        QCOMPARE(model.pointForTime(7, 30), 8);
        QCOMPARE(model.pointForTime(23, 30), 0);
        QCOMPARE(model.pointForTime(99, 99), 0); // not a time
        QCOMPARE(model.snap(449.0), 400);
    }
};

QTEST_GUILESS_MAIN(TestBrightnessCurveModel)
#include "test_brightnesscurvemodel.moc"
