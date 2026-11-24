// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The Linux key source: it reads the keyboard's key node only while it is started, says each key that is down in a key report and nothing else, keeps
// nothing between reports and drops what it does not know. The node is replaced by a socket pair that keeps report boundaries, like hidraw does, so the real
// class (its descriptor, its reads, its stops) is what runs.
#include "HidrawKeyPressSource.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

#include <array>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

using namespace galleond;

namespace {

constexpr int ThreeKeys = 3;

// The four descriptors that the owner's tool printed for the keyboard (docs/protocol/README.md): only the first one of the key node is a keyboard.
const std::array<std::uint8_t, 60> MouseNode = {0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x09, 0x01, 0xa1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x08,
                                                0x15, 0x00, 0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x75, 0x10, 0x95, 0x02, 0x05, 0x01,
                                                0x09, 0x30, 0x09, 0x31, 0x16, 0x01, 0x80, 0x26, 0xff, 0x7f, 0x81, 0x06, 0x75, 0x08, 0x95, 0x01,
                                                0x05, 0x01, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7f, 0x81, 0x06, 0xc0, 0xc0};
const std::array<std::uint8_t, 21> VendorNode = {0x06, 0x42, 0xff, 0x09, 0x02, 0xa1, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x40, 0x09, 0x02, 0x81, 0x02, 0xc0};
const std::array<std::uint8_t, 27> BragiNode = {0x06, 0x42, 0xff, 0x09, 0x01, 0xa1, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08,
                                                0x95, 0x80, 0x09, 0x01, 0x81, 0x02, 0x95, 0x80, 0x09, 0x01, 0x91, 0x02, 0xc0};
const std::array<std::uint8_t, 8> KeyNodeStart = {0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x08}; // the first bytes of the 138 of the key node

using Report = std::array<std::uint8_t, 18>;

Report keyReport(std::initializer_list<std::pair<int, std::uint8_t>> bytes)
{
    Report report{};
    report[0] = 1;
    for (const auto &[at, value] : bytes)
        report.at(static_cast<std::size_t>(at)) = value;
    return report;
}

// A socket pair that keeps the boundaries of what is sent: `ours` is where the "keyboard" writes, the other end goes to the source.
struct Pair
{
    int ours = -1;
    int theirs = -1;
    Pair()
    {
        std::array<int, 2> fds{};
        if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, fds.data()) == 0) {
            ours = fds.at(0);
            theirs = fds.at(1);
        }
    }
    ~Pair()
    {
        if (ours >= 0)
            close(ours);
        if (theirs >= 0)
            close(theirs); // already closed when the source took it and stopped: harmless
    }
    Pair(const Pair &) = delete;
    Pair &operator=(const Pair &) = delete;
    template<std::size_t N> [[nodiscard]] bool send(const std::array<std::uint8_t, N> &report) const { return ::send(ours, report.data(), N, MSG_NOSIGNAL) == static_cast<ssize_t>(N); }
    // After the source has stopped, its end is closed: writing to ours then fails.
    [[nodiscard]] bool theOtherEndIsClosed() const
    {
        const std::array<std::uint8_t, 1> byte{0};
        return ::send(ours, byte.data(), byte.size(), MSG_NOSIGNAL) < 0 && errno == EPIPE;
    }
};

QList<quint16> usagesOf(const QSignalSpy &spy)
{
    QList<quint16> usages;
    for (const auto &arguments : spy)
        usages.append(arguments.at(0).value<quint16>());
    return usages;
}

} // namespace

class TestHidrawKeySource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onlyTheNodeThatOpensWithAKeyboardIsTheKeyNode()
    {
        QVERIFY(looksLikeKeyboardDescriptor(KeyNodeStart));
        QVERIFY(!looksLikeKeyboardDescriptor(MouseNode));
        QVERIFY(!looksLikeKeyboardDescriptor(VendorNode));
        QVERIFY(!looksLikeKeyboardDescriptor(BragiNode));
        QVERIFY(!looksLikeKeyboardDescriptor({}));
        QVERIFY(!looksLikeKeyboardDescriptor(std::span<const std::uint8_t>(KeyNodeStart).first(3)));
    }

    void nothingIsOpenedUntilItIsStarted()
    {
        int opened = 0;
        const HidrawKeyPressSource source([&] {
            ++opened;
            return -1;
        });
        QVERIFY(!source.watching());
        QCOMPARE(opened, 0);
    }

    void aKeyThatIsDownIsSaidOnceWithItsUsage()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        QVERIFY(source.watching());
        QVERIFY(pair.send(keyReport({{2, 0x10}}))); // the A key: bit 4 of the bitmap
        QTRY_COMPARE(keys.size(), 1);
        QCOMPARE(usagesOf(keys), (QList<quint16>{0x04}));
    }

    void severalKeysAreSaidInIncreasingOrderModifiersIncluded()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        QVERIFY(pair.send(keyReport({{1, 0x02}, {2, 0x30}, {9, 0x10}}))); // left Shift, A and B, F3
        QTRY_COMPARE(keys.size(), 4);
        QCOMPARE(usagesOf(keys), (QList<quint16>{0x04, 0x05, 0x3C, 0xE1}));
    }

    void aReportWithNothingDownSaysNothing()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        QVERIFY(pair.send(keyReport({})));
        QVERIFY(pair.send(keyReport({{2, 0x10}})));
        QTRY_COMPARE(keys.size(), 1); // the second one: the first made no sound
    }

    void aKeyThatStaysDownIsSaidAgainWithTheNextReportBecauseNothingIsRemembered()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        QVERIFY(pair.send(keyReport({{2, 0x10}})));          // A goes down
        QVERIFY(pair.send(keyReport({{2, 0x30}})));          // B goes down while A is held: both are down
        QTRY_COMPARE(keys.size(), 3);
        QCOMPARE(usagesOf(keys), (QList<quint16>{0x04, 0x04, 0x05}));
    }

    void aReportThatIsNotAKeyReportIsDroppedWithOneWarningThatHoldsNoByte()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*"))); // any warning that is not expected below fails the test
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        QTest::ignoreMessage(QtWarningMsg, "Key source: a report of another shape was dropped (ID 8, 15 bytes)");
        std::array<std::uint8_t, 15> arrayReport{};
        arrayReport[0] = 8;
        arrayReport[2] = 0x3C; // a key, in the array report: never read as one
        QVERIFY(pair.send(arrayReport));
        std::array<std::uint8_t, 18> other = keyReport({{2, 0x10}});
        other[0] = 9;
        QVERIFY(pair.send(other)); // another one: no second warning (a second one would fail the test)
        QVERIFY(pair.send(keyReport({{2, 0x20}})));
        QTRY_COMPARE(keys.size(), 1); // only the real key report counted, after the two that were dropped
        QCOMPARE(usagesOf(keys), (QList<quint16>{0x05}));
    }

    void theMediaKeyReportIsDroppedWithoutAWord()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        const std::array<std::uint8_t, 3> media{2, 0xE9, 0}; // volume up
        QVERIFY(pair.send(media));
        QVERIFY(pair.send(keyReport({{2, 0x10}})));
        QTRY_COMPARE(keys.size(), 1); // no warning was raised: it would have failed the test
    }

    void aSecondWatchWarnsAgainAboutAnUnknownShape()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        std::array<Pair, 2> pairs;
        std::size_t next = 0;
        HidrawKeyPressSource source([&] { return pairs.at(next++).theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        for (std::size_t watch = 0; watch < 2; ++watch) {
            source.start();
            QTest::ignoreMessage(QtWarningMsg, "Key source: a report of another shape was dropped (ID 9, 2 bytes)");
            QVERIFY(pairs.at(watch).send(std::array<std::uint8_t, 2>{9, 1}));
            QVERIFY(pairs.at(watch).send(keyReport({{2, 0x10}}))); // read after the first: when it is said, the report of another shape was read too
            QTRY_COMPARE(keys.size(), static_cast<qsizetype>(watch) + 1);
            source.stop();
        }
    }

    void stoppingClosesTheNodeAndNothingIsReadAfterwards()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        source.stop();
        QVERIFY(!source.watching());
        QVERIFY(pair.theOtherEndIsClosed());
        QTest::qWait(50);
        QCOMPARE(keys.size(), 0);
    }

    void startingTwiceOpensOnceAndStartingAgainAfterAStopOpensAgain()
    {
        std::array<Pair, 2> pairs;
        int opened = 0;
        HidrawKeyPressSource source([&] { return pairs.at(static_cast<std::size_t>(opened++)).theirs; });
        source.start();
        source.start();
        QCOMPARE(opened, 1);
        source.stop();
        source.start();
        QCOMPARE(opened, 2);
        QVERIFY(source.watching());
    }

    void withoutANodeItSaysSoOnceWithNoDetailAndDoesNotWatch()
    {
        HidrawKeyPressSource source([] { return -1; });
        const QSignalSpy gone(&source, &KeyPressSource::unavailable);
        source.start();
        QCOMPARE(gone.size(), 1);
        QVERIFY(!source.watching());
        QCOMPARE(gone.constFirst().size(), 0); // no argument: nothing is said about why
    }

    void whenTheKeyboardGoesAwayItSaysSoOnceAndStops()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy gone(&source, &KeyPressSource::unavailable);
        source.start();
        close(pair.ours); // the "keyboard" is unplugged: the reads end
        pair.ours = -1;
        QTRY_COMPARE(gone.size(), 1);
        QVERIFY(!source.watching());
        QTest::qWait(50);
        QCOMPARE(gone.size(), 1);
    }

    void aFloodOfReportsIsReadInTurnsAndNoneIsLost()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        const QSignalSpy keys(&source, &KeyPressSource::keyDown);
        source.start();
        int sent = 0;
        for (; sent < 400; ++sent) {
            if (!pair.send(keyReport({{2, 0x10}})))
                break;
        }
        QVERIFY(sent > HidrawKeyPressSource::ReportsPerWakeup); // more than one turn's worth
        QCoreApplication::processEvents(QEventLoop::AllEvents); // one pass of the loop: the descriptor is ready once
        QVERIFY(!keys.isEmpty());
        QVERIFY(keys.size() <= HidrawKeyPressSource::ReportsPerWakeup); // one turn reads no more than its share: the loop is not kept busy
        QTRY_COMPARE_WITH_TIMEOUT(keys.size(), sent, 5000);             // and the rest is read on the turns that follow
    }

    void aSlotThatStopsTheSourceDuringAReportEndsTheReadingEvenInTheMiddleOfIt()
    {
        Pair pair;
        HidrawKeyPressSource source([&] { return pair.theirs; });
        int said = 0;
        connect(&source, &KeyPressSource::keyDown, &source, [&](quint16) {
            ++said;
            source.stop();
        });
        source.start();
        QVERIFY(pair.send(keyReport({{2, 0x30}}))); // two keys in one report
        QVERIFY(pair.send(keyReport({{2, 0x10}})));
        QTest::qWait(100);
        QCOMPARE(said, 1); // the second key of the report was not said, and the second report was never read: the node was closed
        QVERIFY(!source.watching());
    }
};

QTEST_GUILESS_MAIN(TestHidrawKeySource)
#include "test_hidrawkeysource.moc"
