// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The diagnostic of the live lighting: a transport that passes everything on and says what went through, in a file that stops
// when it is full. A fake transport stands for the keyboard; nothing is opened.

#include "FakeTransport.h"
#include "TransportTrace.h"

#include <QStringList>
#include <QTest>

#include <memory>
#include <mutex>

using namespace galleond;

namespace {

struct Lines
{
    QStringList list;
    std::mutex mutex;
    std::shared_ptr<TraceSink> sink(std::size_t limit = TraceFileLimit)
    {
        return std::make_shared<TraceSink>([this](const QString &text) { const std::scoped_lock lock(mutex); list.append(text); }, limit);
    }
};

} // namespace

class TestTransportTrace : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void whilePausedNothingIsWrittenAndOneLineSaysSoAtEachEnd()
    {
        Lines lines;
        auto inner = std::make_unique<FakeTransport>();
        FakeTransport *fake = inner.get();
        const auto sink = lines.sink();
        TracedTransport traced(std::move(inner), sink, [] { return 7; });
        QVERIFY(traced.open());
        const qsizetype before = lines.list.size();
        sink->setPaused(true);
        QVERIFY(sink->paused());
        QCOMPARE(lines.list.size(), before + 1);
        QVERIFY(lines.list.last().contains("paused"));
        const std::vector<std::uint8_t> frame{0, 0, 1, 6, 0x12, 0x00, 9, 9, 9};
        QVERIFY(traced.write(frame));
        QVERIFY(traced.sendFeatureReport(frame));
        QCOMPARE(fake->writes.size(), std::size_t{1}); // what goes to the keyboard is not touched by the pause
        sink->line(QStringLiteral("a line of the lighting while the keys glow"));
        QCOMPARE(lines.list.size(), before + 1);        // nothing was written
        sink->setPaused(true);                           // said twice: the same
        QCOMPARE(lines.list.size(), before + 1);
        sink->setPaused(false);
        QVERIFY(!sink->paused());
        QCOMPARE(lines.list.size(), before + 2);
        QVERIFY(lines.list.last().contains("resumes"));
        QVERIFY(traced.write(frame));
        QCOMPARE(lines.list.size(), before + 3);        // told again
        sink->setPaused(false);
        QCOMPARE(lines.list.size(), before + 3);
    }

    void theLimitOfTheFileStillHoldsWhenItIsPausedAndResumed()
    {
        Lines lines;
        const auto sink = lines.sink(120);
        for (int i = 0; i < 20; ++i) {
            sink->setPaused(true);
            sink->setPaused(false);
        }
        QVERIFY(lines.list.size() < 6);
        QVERIFY(lines.list.last().contains("the log is full"));
    }

    void aLineHasTheTimeTheDirectionTheLengthAndOnlyTheFirstBytes()
    {
        const std::vector<std::uint8_t> report(128, 0xab);
        const QString line = traceLine(1234, ">", report);
        QVERIFY2(line.contains("1234 ms"), qPrintable(line));
        QVERIFY(line.contains("> 128 bytes: ab ab ab"));
        QVERIFY(line.endsWith("..."));                 // the report goes on
        QCOMPARE(line.count("ab"), 16);               // and only the first sixteen bytes are told
        QVERIFY(!traceLine(5, "<", std::vector<std::uint8_t>{1, 2, 3}).contains("..."));
    }

    void everythingWrittenAndReadGoesThroughUnchangedAndIsTold()
    {
        Lines lines;
        auto inner = std::make_unique<FakeTransport>();
        FakeTransport *fake = inner.get();
        TracedTransport traced(std::move(inner), lines.sink(), [] { return 7; });
        QVERIFY(traced.open());
        const std::vector<std::uint8_t> request{0, 0, 1, 9, 2, 3};
        QVERIFY(traced.write(request));
        QCOMPARE(fake->writes.size(), std::size_t{1});
        QCOMPARE(fake->writes.front(), request); // exactly what was given
        fake->pendingReads.push_back({0, 0, 1, 9, 2, 0});
        std::array<std::uint8_t, 64> buffer{};
        QCOMPARE(traced.read(buffer), 6);
        QCOMPARE(buffer[4], std::uint8_t{2});
        QVERIFY(lines.list.at(0).contains("open: yes"));
        QVERIFY(lines.list.at(1).contains("> 6 bytes: 00 00 01 09 02 03"));
        QVERIFY(lines.list.at(2).contains("< 6 bytes: 00 00 01 09 02 00"));
    }

    void aFailedWriteAndAGoneDeviceAreToldAndAnEmptyReadIsNot()
    {
        Lines lines;
        auto inner = std::make_unique<FakeTransport>(); // never opened: its writes fail
        FakeTransport *fake = inner.get();
        TracedTransport traced(std::move(inner), lines.sink(), [] { return 1; });
        QVERIFY(!traced.write(std::vector<std::uint8_t>{1, 2}));
        QVERIFY(lines.list.constLast().contains("FAILED"));
        std::array<std::uint8_t, 8> buffer{};
        QCOMPARE(traced.read(buffer), 0); // nothing pending: not a line
        const qsizetype before = lines.list.size();
        QCOMPARE(traced.read(buffer), 0);
        QCOMPARE(lines.list.size(), before);
        fake->disconnected = true;
        QCOMPARE(traced.read(buffer), -1);
        QVERIFY(lines.list.constLast().contains("the device is gone"));
    }

    void theSerialNumberIsNeverInTheLog()
    {
        Lines lines;
        const TracedTransport traced(std::make_unique<FakeTransport>(), lines.sink(), [] { return 0; });
        QVERIFY(traced.serialNumber().isEmpty()); // the fake one is "FAKE0001"
    }

    void theLogStopsWithALastLineWhenItIsFullAndNeverGrowsAgain()
    {
        Lines lines;
        const auto sink = lines.sink(100);
        for (int i = 0; i < 50; ++i)
            sink->line(QStringLiteral("a line of some length %1").arg(i));
        const qsizetype size = lines.list.size();
        QVERIFY(size < 50);
        QVERIFY(lines.list.constLast().contains("the log is full"));
        sink->line(QStringLiteral("after the end"));
        QCOMPARE(lines.list.size(), size);
    }
};

QTEST_GUILESS_MAIN(TestTransportTrace)
#include "test_transporttrace.moc"
