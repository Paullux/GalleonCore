// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Endurance of the module driver (galleon::StreamDeck) over a fake transport: the same few thousand events, drops and
// reconnections as a long session, played in a few seconds and checked after every cycle. It never touches hardware.
//
// Short by default (a fixed number of cycles, part of ctest). GALLEON_ENDURANCE_SECONDS=<n> plays cycles for n seconds
// instead; on Linux it then also checks that the memory of the process stops growing. This is the first step of the
// endurance plan of docs/ROADMAP.md: it does not replace the hardware tests, the USB power cuts or the firmware.

#include "Bytes.h"
#include "FakeTransport.h"

#include <galleon/StreamDeck.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

using galleon::StreamDeck;

namespace {

constexpr int DefaultCycles = 60;
constexpr qint64 MaxRssGrowthKib = qint64{20} * 1024; // from a quarter of a timed run to its end, caches included

// The resident memory of this process, in KiB; 0 when it cannot be read (not Linux).
qint64 residentKib()
{
    QFile file(QStringLiteral("/proc/self/statm"));
    if (!file.open(QIODevice::ReadOnly)) return 0;
    const QList<QByteArray> fields = file.readAll().split(' ');
    if (fields.size() < 2) return 0;
    return fields.at(1).toLongLong() * 4; // pages of 4 KiB: the check only compares two readings of the same machine
}

// The report of a module with only `key` down, or with nothing down when `key` is negative.
std::vector<std::uint8_t> keyState(int key)
{
    std::vector<std::uint8_t> report = keyReport(galleon::protocol::KeyCount, 0);
    if (key >= 0) report.at(4 + static_cast<std::size_t>(key)) = 1;
    return report;
}

struct Cycle
{
    int presses = 0;
    int releases = 0;
    bool dial = false;
};

} // namespace

class TestEndurance : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void manyBurstsAndReconnectionsLeaveTheDriverConsistent()
    {
        auto fake = std::make_unique<FakeTransport>();
        FakeTransport *transport = fake.get();
        StreamDeck deck(std::move(fake));
        deck.setKeepAliveInterval(5);

        const QSignalSpy pressed(&deck, &StreamDeck::keyPressed);
        const QSignalSpy released(&deck, &StreamDeck::keyReleased);
        const QSignalSpy dial(&deck, &StreamDeck::dialRotated);
        const QSignalSpy lost(&deck, &StreamDeck::disconnected);

        const int seconds = qEnvironmentVariableIntValue("GALLEON_ENDURANCE_SECONDS");
        const bool timed = seconds > 0;
        std::mt19937 random(12345); // NOLINT(cert-msc32-c,cert-msc51-cpp): a fixed seed on purpose, a failure must be replayable
        QElapsedTimer clock;
        clock.start();
        qint64 reference = 0;
        qint64 expectedPresses = 0;
        qint64 expectedReleases = 0;
        qint64 expectedDials = 0;
        qint64 expectedLosses = 0;
        int cycles = 0;

        while (timed ? clock.elapsed() < seconds * 1000LL : cycles < DefaultCycles) {
            QVERIFY2(deck.open(), "the module must open again after every drop");
            QVERIFY(deck.isOpen());

            // A burst: some keys pressed then released one by one, a dial turn, and two reports that must be dropped.
            Cycle cycle;
            const int keys = 1 + static_cast<int>(random() % 12);
            const int ending = cycles % 4; // 0 clean close, 1 read lost, 2 keep-alive refused, 3 read lost with a key held down
            for (int i = 0; i < keys; ++i) {
                const int key = static_cast<int>(random() % galleon::protocol::KeyCount);
                transport->pendingReads.push_back(keyState(key));
                ++cycle.presses;
                const bool keepHeld = ending == 3 && i == keys - 1;
                if (!keepHeld) {
                    transport->pendingReads.push_back(keyState(-1));
                    ++cycle.releases;
                }
            }
            transport->pendingReads.push_back({0x01, 0x03, 0x03, 0x00, 0x01, 0x01, 0x00}); // dial 0, one step
            cycle.dial = true;
            transport->pendingReads.push_back({0x02});       // another report ID: ignored
            transport->pendingReads.push_back({0x01, 0x00}); // too short: invalid, dropped

            expectedPresses += cycle.presses;
            expectedReleases += cycle.releases;
            expectedDials += 1;
            QTRY_COMPARE_WITH_TIMEOUT(pressed.count(), expectedPresses, 3000);
            QTRY_COMPARE_WITH_TIMEOUT(released.count(), expectedReleases, 3000);
            QTRY_COMPARE_WITH_TIMEOUT(dial.count(), expectedDials, 3000);
            QTRY_VERIFY_WITH_TIMEOUT(transport->pendingReads.empty(), 3000);

            switch (ending) {
            case 0:
                deck.close();
                break;
            case 1:
            case 3:
                transport->disconnected = true;
                ++expectedLosses;
                QTRY_COMPARE_WITH_TIMEOUT(lost.count(), expectedLosses, 3000);
                transport->disconnected = false;
                break;
            default:
                transport->featureSucceeds = false;
                ++expectedLosses;
                QTRY_COMPARE_WITH_TIMEOUT(lost.count(), expectedLosses, 3000);
                transport->featureSucceeds = true;
                break;
            }
            QVERIFY2(!deck.isOpen(), "a drop must leave the module closed, not half open");
            // What the module sent while it was gone must not be read late, nor create events.
            QCOMPARE(pressed.count(), expectedPresses);
            QCOMPARE(released.count(), expectedReleases);

            // Nothing the transport stored for the tests is kept: a real module does not grow either.
            transport->writes.clear();
            transport->featureReports.clear();

            ++cycles;
            if (timed && reference == 0 && clock.elapsed() >= seconds * 250LL) reference = residentKib();
        }

        const StreamDeck::Diagnostics stats = deck.diagnostics();
        QCOMPARE(stats.opens, static_cast<std::uint64_t>(cycles));
        QCOMPARE(stats.ignoredReports, static_cast<std::uint64_t>(cycles));
        QCOMPARE(stats.invalidReports, static_cast<std::uint64_t>(cycles));
        QCOMPARE(stats.readErrors + stats.keepAliveFailures, static_cast<std::uint64_t>(expectedLosses));

        if (timed && reference > 0) {
            const qint64 end = residentKib();
            qInfo().noquote() << "endurance:" << cycles << "cycles in" << clock.elapsed() / 1000 << "s, memory" << reference << "KiB ->" << end << "KiB";
            QVERIFY2(end - reference <= MaxRssGrowthKib, qPrintable(QStringLiteral("memory grew from %1 to %2 KiB").arg(reference).arg(end)));
        }
    }
};

QTEST_GUILESS_MAIN(TestEndurance)
#include "test_endurance.moc"
