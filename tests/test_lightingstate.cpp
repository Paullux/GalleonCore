// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// lighting-state.json: the strict format, the atomic write, and the publisher that writes it at every change of state of
// the live lighting (the controller's signal is emitted by hand: no keyboard is involved).

#include "LightingController.h"
#include "LightingState.h"
#include "LightingStatePublisher.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

namespace {

// A controller that never opens anything; the publisher only listens to its signal.
struct Idle final : WatchdogGuard
{
    bool start(qint64, QString *) override { return false; }
    void stop() override {}
};
struct NoPower final : PowerEvents
{
    bool available(QString *) const override { return false; }
    void holdSleep() override {}
    void allowSleep() override {}
};

} // namespace

class TestLightingState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aStateSurvivesBeingEncodedAndParsed()
    {
        for (const char *name : {"off", "starting", "listening", "taking", "holding", "yielded", "sleeping"}) {
            const LightingState state{.state = QString::fromLatin1(name), .detail = QStringLiteral("because"), .time = 1783938123};
            QCOMPARE(parseLightingState(encodeLightingState(state)), std::optional<LightingState>(state));
        }
    }

    void anythingOutOfShapeIsRefused()
    {
        const auto refused = [](const char *json) { return !parseLightingState(QByteArray(json)).has_value(); };
        QVERIFY(refused(""));
        QVERIFY(refused("not json"));
        QVERIFY(refused("[]"));
        QVERIFY(refused(R"({"version":1,"state":"holding","time":5})"));
        QVERIFY(refused(R"({"version":1,"state":"holding","detail":"","time":5,"extra":1})"));
        QVERIFY(refused(R"({"version":2,"state":"holding","detail":"","time":5})"));
        QVERIFY(refused(R"({"version":1,"state":"burning","detail":"","time":5})"));          // not a state
        QVERIFY(refused(R"({"version":1,"state":7,"detail":"","time":5})"));
        QVERIFY(refused(R"({"version":1,"state":"holding","detail":7,"time":5})"));
        QVERIFY(refused(R"({"version":1,"state":"holding","detail":"a\nb","time":5})"));    // a control character
        QVERIFY(refused(R"({"version":1,"state":"holding","detail":"","time":-1})"));
        QVERIFY(refused(R"({"version":1,"state":"holding","detail":"","time":1.5})"));
        QVERIFY(refused(R"({"version":1,"state":"holding","detail":"","time":99999999999999})"));
        const QByteArray tooLongDetail = QByteArray(R"j({"version":1,"state":"off","detail":")j") + QByteArray(301, 'x') + QByteArray(R"j(","time":5})j");
        QVERIFY(!parseLightingState(tooLongDetail));
        QVERIFY(!parseLightingState(QByteArray(5000, ' '))); // over the size limit
    }

    void aLongDetailIsCutWhenWrittenSoTheReaderStillAcceptsIt()
    {
        const LightingState longDetail{.state = QStringLiteral("off"), .detail = QString(1000, QLatin1Char('x')), .time = 5};
        const auto parsed = parseLightingState(encodeLightingState(longDetail));
        QVERIFY(parsed);
        QCOMPARE(parsed->detail.size(), MaxLightingDetail);
    }

    void aFreshStateIsOneWrittenAtOrAfterTheStart()
    {
        const LightingState state{.state = QStringLiteral("off"), .detail = {}, .time = 100};
        QVERIFY(isFreshLightingState(state, 100));
        QVERIFY(!isFreshLightingState(state, 101));
    }

    void theFileIsWrittenAsideAndReadBackAndAMissingOrBrokenOneIsNothing()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("later/lighting-state.json");
        QVERIFY(!readLightingState(path));
        const LightingState state{.state = QStringLiteral("holding"), .detail = {}, .time = 9};
        QVERIFY(writeLightingState(path, state)); // creates the folder
        QCOMPARE(readLightingState(path), std::optional<LightingState>(state));
        QFile broken(path);
        QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
        broken.write("{");
        broken.close();
        QVERIFY(!readLightingState(path));
    }

    void thePublisherWritesEveryStateTheControllerReportsAndNothingItWouldRefuse()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-state.json");
        Idle watchdog;
        NoPower power;
        LightingController controller([]() -> std::unique_ptr<galleon::Transport> { return nullptr; }, watchdog, power);
        qint64 now = 1000;
        const LightingStatePublisher publisher(controller, path, [&now] { return now; });
        Q_EMIT controller.stateChanged(QStringLiteral("holding"), QString());
        QCOMPARE(readLightingState(path), std::optional<LightingState>(LightingState{.state = QStringLiteral("holding"), .detail = {}, .time = 1000}));
        now = 1001;
        Q_EMIT controller.stateChanged(QStringLiteral("off"), QStringLiteral("the watchdog is not installed"));
        QCOMPARE(readLightingState(path), std::optional<LightingState>(LightingState{.state = QStringLiteral("off"), .detail = QStringLiteral("the watchdog is not installed"), .time = 1001}));
        now = 1002;
        Q_EMIT controller.stateChanged(QStringLiteral("burning"), QString());
        QCOMPARE(readLightingState(path)->time, 1001); // the unknown state wrote nothing
    }
};

QTEST_GUILESS_MAIN(TestLightingState)
#include "test_lightingstate.moc"
