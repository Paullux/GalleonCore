// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FakeTransport.h"

#include "KeyboardReadProbe.h"

#include <QTest>

#include <algorithm>
#include <chrono>
#include <span>
#include <vector>

using namespace keyboardprobe;
using namespace std::chrono_literals;

namespace {

// An answer as hidapi and hidraw return an unnumbered report: 128 bytes.
FakeTransport::Report answer(std::uint8_t command, std::uint8_t status, std::uint32_t value)
{
    FakeTransport::Report report(PayloadSize, 0);
    report[3] = command;
    report[4] = status;
    for (std::size_t i = 0; i < 4; ++i)
        report[5 + i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xffU);
    return report;
}

} // namespace

class TestKeyboardReadProbe : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void requestIsOneGetOfTheNamedPropertyWithTheReportIdByte()
    {
        const auto request = getPropertyRequest(PropertyBrightness);
        QCOMPARE(request.size(), std::size_t{129});
        QCOMPARE(request[0], std::uint8_t{0x00}); // report ID
        QCOMPARE(request[2], std::uint8_t{0x01});
        QCOMPARE(request[4], CommandGet);
        QCOMPARE(request[5], PropertyBrightness);
        QVERIFY(std::ranges::all_of(std::span(request).subspan(6), [](std::uint8_t byte) { return byte == 0; }));
    }

    void brightnessAnswerFromTheWebHubCaptureReadsAsOneThousand()
    {
        // 00 00 00 02 00 e8 03: what the keyboard answered at 100 % (Web Hub log, 2026-11-08).
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.beforeWrite = [&] { transport.pendingReads.push_back(answer(CommandGet, 0, 1000)); };
        const auto result = readProperty(transport, PropertyBrightness, 200ms);
        QCOMPARE(result.error, ReadError::None);
        QCOMPARE(result.reply.status, std::uint8_t{0});
        QCOMPARE(result.reply.value, std::uint32_t{1000});
        QCOMPARE(transport.writes.size(), std::size_t{1});
        QCOMPARE(transport.writes.front().size(), WriteSize);
    }

    void firmwareAnswerReadsAsTheThreeVersionBytes()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.beforeWrite = [&] { transport.pendingReads.push_back(answer(CommandGet, 0, 0x580302)); };
        const auto result = readProperty(transport, PropertyFirmware, 200ms);
        QCOMPARE(result.error, ReadError::None);
        QCOMPARE(result.reply.value, std::uint32_t{0x580302});
    }

    void answerWithALeadingReportIdByteIsAccepted()
    {
        auto padded = answer(CommandGet, 0, 600);
        padded.insert(padded.begin(), std::uint8_t{0});
        QCOMPARE(padded.size(), WriteSize);
        const auto reply = parsePropertyReply(padded);
        if (!reply)
            QFAIL("a 129-byte answer with a leading 00 should be accepted");
        QCOMPARE(reply->value, std::uint32_t{600});
    }

    void answerOfAnotherLengthOrCommandIsRefused()
    {
        QVERIFY(!parsePropertyReply({}).has_value());
        QVERIFY(!parsePropertyReply(FakeTransport::Report(64, 0)).has_value());
        QVERIFY(!parsePropertyReply(FakeTransport::Report(130, 0)).has_value());
        QVERIFY(!parsePropertyReply(answer(0x12, 0, 1)).has_value());
        auto notReportId = answer(CommandGet, 0, 1);
        notReportId.insert(notReportId.begin(), std::uint8_t{7}); // 129 bytes, first one not 00
        QVERIFY(!parsePropertyReply(notReportId).has_value());
    }

    void propertyOutsideTheReadOnlyListIsNeverSent()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        for (const std::uint8_t property : {std::uint8_t{0x03}, std::uint8_t{0x01}, std::uint8_t{0x00}, std::uint8_t{0xff}}) {
            const auto result = readProperty(transport, property, 50ms);
            QCOMPARE(result.error, ReadError::NotAllowed);
        }
        QVERIFY(transport.writes.empty());
    }

    void reportsAlreadyWaitingAreDiscardedAndNeverTakenForTheAnswer()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.pendingReads.push_back(answer(CommandGet, 0, 600)); // stale: for someone else
        transport.beforeWrite = [&] { transport.pendingReads.push_back(answer(CommandGet, 0, 1000)); };
        const auto result = readProperty(transport, PropertyBrightness, 200ms);
        QCOMPARE(result.error, ReadError::None);
        QCOMPARE(result.reply.value, std::uint32_t{1000});
        QCOMPARE(result.staleDiscarded, 1);
    }

    void unrelatedReportsWhileWaitingAreCountedAndSkipped()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.beforeWrite = [&] {
            transport.pendingReads.push_back(answer(0x12, 0, 0)); // a ping answer meant for another host
            transport.pendingReads.emplace_back(std::size_t{64}, std::uint8_t{0});
            transport.pendingReads.push_back(answer(CommandGet, 0, 800));
        };
        const auto result = readProperty(transport, PropertyBrightness, 200ms);
        QCOMPARE(result.error, ReadError::None);
        QCOMPARE(result.reply.value, std::uint32_t{800});
        QCOMPARE(result.unexpected, 2);
    }

    void silenceEndsWithATimeoutInsteadOfWaitingForever()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        const auto result = readProperty(transport, PropertyBrightness, 30ms);
        QCOMPARE(result.error, ReadError::Timeout);
    }

    void aDeviceThatIsGoneIsReportedAsGone()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.disconnected = true;
        QCOMPARE(readProperty(transport, PropertyBrightness, 30ms).error, ReadError::Disconnected);
    }

    void aFailedWriteIsReportedAndNothingIsAwaited()
    {
        FakeTransport transport; // never opened: write() fails
        const auto result = readProperty(transport, PropertyBrightness, 30ms);
        QCOMPARE(result.error, ReadError::WriteFailed);
    }

    void statusOtherThanZeroIsKeptSoTheCallerCanSeeIt()
    {
        // Property 0x05 without its extra argument answered status 5 on this firmware.
        const auto reply = parsePropertyReply(answer(CommandGet, 5, 0));
        if (!reply)
            QFAIL("an answer with status 5 should still be parsed");
        QCOMPARE(reply->status, std::uint8_t{5});
    }
};

QTEST_APPLESS_MAIN(TestKeyboardReadProbe)
#include "test_keyboardreadprobe.moc"
