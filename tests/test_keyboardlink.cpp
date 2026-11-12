// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FakeTransport.h"

#include <galleon/keyboard/BragiFrame.h>
#include <galleon/keyboard/BragiReply.h>
#include <galleon/keyboard/Brightness.h>
#include <galleon/keyboard/KeyboardLink.h>

#include <QTest>

#include <algorithm>
#include <chrono>
#include <span>
#include <vector>

using namespace galleon::keyboard;
using namespace std::chrono_literals;

namespace {

// A get answer as hidapi/hidraw return it: an unnumbered 128-byte report.
FakeTransport::Report getAnswer(std::uint8_t status, std::uint32_t value)
{
    FakeTransport::Report report(PayloadSize, 0);
    report[3] = CommandGet;
    report[4] = status;
    for (std::size_t i = 0; i < 4; ++i)
        report[5 + i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xffU);
    return report;
}

// A set echo: no value, just the status.
FakeTransport::Report setAnswer(std::uint8_t status)
{
    FakeTransport::Report report(PayloadSize, 0);
    report[3] = CommandSet;
    report[4] = status;
    return report;
}

} // namespace

class TestKeyboardLink : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void getRequestAsksToReadTheBrightnessProperty()
    {
        const auto request = getPropertyRequest(PropertyBrightness);
        QCOMPARE(request.size(), std::size_t{129});
        QCOMPARE(request[0], std::uint8_t{0x00}); // report ID
        QCOMPARE(request[2], std::uint8_t{0x01});
        QCOMPARE(request[4], CommandGet);
        QCOMPARE(request[5], PropertyBrightness);
        QVERIFY(std::ranges::all_of(std::span(request).subspan(6), [](std::uint8_t byte) { return byte == 0; }));
    }

    void setRequestCarriesThePropertyAndTheLittleEndianValue()
    {
        // 60 % from the Web Hub capture (2026-11-08): 01 ea 00 58 02 after the "00 01 00" header.
        const auto request = setPropertyRequest(PropertyBrightness, 600);
        QCOMPARE(request[2], std::uint8_t{0x01});
        QCOMPARE(request[4], CommandSet);
        QCOMPARE(request[5], PropertyBrightness);
        QCOMPARE(request[6], std::uint8_t{0x00}); // reserved byte, always 0
        QCOMPARE(request[7], std::uint8_t{0x58});
        QCOMPARE(request[8], std::uint8_t{0x02});
    }

    void getAnswerParsesTheFourValueBytes()
    {
        const auto reply = parsePropertyReply(getAnswer(0, 1000));
        if (!reply)
            QFAIL("a get answer with a full value should be accepted");
        QCOMPARE(reply->command, CommandGet);
        QCOMPARE(reply->status, std::uint8_t{0});
        QCOMPARE(reply->value, std::uint32_t{1000});
    }

    void setAnswerIsAnEchoWithNoValue()
    {
        const auto reply = parsePropertyReply(setAnswer(0));
        if (!reply)
            QFAIL("a set echo should be accepted");
        QCOMPARE(reply->command, CommandSet);
        QCOMPARE(reply->status, std::uint8_t{0});
        QCOMPARE(reply->value, std::uint32_t{0});
    }

    void aLeadingReportIdByteIsAccepted()
    {
        auto padded = getAnswer(0, 600);
        padded.insert(padded.begin(), std::uint8_t{0});
        QCOMPARE(padded.size(), WriteSize);
        const auto reply = parsePropertyReply(padded);
        if (!reply)
            QFAIL("a 129-byte answer with a leading 00 should be accepted");
        QCOMPARE(reply->value, std::uint32_t{600});
    }

    void aTooShortSetEchoIsRefused()
    {
        QVERIFY(!parsePropertyReply(FakeTransport::Report(4, 0)).has_value());
        QVERIFY(!parsePropertyReply({}).has_value());
        QVERIFY(!parsePropertyReply(FakeTransport::Report(130, 0)).has_value());
    }

    void brightnessOutsideZeroToAThousandIsInvalid()
    {
        QVERIFY(isValidBrightness(0));
        QVERIFY(isValidBrightness(1000));
        QVERIFY(!isValidBrightness(1001));
        QVERIFY(!isValidBrightness(65535));
    }

    void percentAndValueConvertBothWays()
    {
        QCOMPARE(brightnessFromPercent(0), std::uint16_t{0});
        QCOMPARE(brightnessFromPercent(60), std::uint16_t{600});
        QCOMPARE(brightnessFromPercent(100), std::uint16_t{1000});
        QCOMPARE(brightnessFromPercent(150), std::uint16_t{1000}); // clamped
        QCOMPARE(percentFromBrightness(600), 60);
        QCOMPARE(percentFromBrightness(1000), 100);
    }

    void readBrightnessReturnsTheValueTheKeyboardAnswers()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.beforeWrite = [&] { transport.pendingReads.push_back(getAnswer(0, 1000)); };
        const auto result = readBrightness(transport, 200ms);
        QCOMPARE(result.error, LinkError::None);
        QCOMPARE(result.value, std::uint16_t{1000});
        QCOMPARE(transport.writes.size(), std::size_t{1});
    }

    void readBrightnessDiscardsAStaleAnswerFirst()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.pendingReads.push_back(getAnswer(0, 200)); // meant for someone else
        transport.beforeWrite = [&] { transport.pendingReads.push_back(getAnswer(0, 800)); };
        const auto result = readBrightness(transport, 200ms);
        QCOMPARE(result.error, LinkError::None);
        QCOMPARE(result.value, std::uint16_t{800});
    }

    void aReplyOutsideZeroToAThousandIsInvalidInsteadOfTruncated()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.beforeWrite = [&] { transport.pendingReads.push_back(getAnswer(0, 65578)); }; // 0x10032 would read as 50
        const auto result = readBrightness(transport, 200ms);
        QCOMPARE(result.error, LinkError::InvalidReply);
    }

    void aQueueThatNeverEmptiesSendsNothingAndIsReported()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        for (int i = 0; i < 100; ++i)
            transport.pendingReads.push_back(getAnswer(0, 100));
        const auto result = readBrightness(transport, 200ms);
        QCOMPARE(result.error, LinkError::StaleReports);
        QVERIFY(transport.writes.empty());
    }

    void writeBrightnessConfirmsByReadingTheValueBack()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        int writeCount = 0;
        transport.beforeWrite = [&] {
            ++writeCount;
            // First write is the set request, second the confirming get.
            transport.pendingReads.push_back(writeCount == 1 ? setAnswer(0) : getAnswer(0, 600));
        };
        const auto result = writeBrightness(transport, 600, 200ms);
        QCOMPARE(result.error, LinkError::None);
        QCOMPARE(result.confirmed, std::uint16_t{600});
        QCOMPARE(transport.writes.size(), std::size_t{2});
    }

    void writeBrightnessRejectsAnOutOfRangeValueWithoutSendingAnything()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        const auto result = writeBrightness(transport, 1001, 50ms);
        QCOMPARE(result.error, LinkError::InvalidValue);
        QVERIFY(transport.writes.empty());
    }

    void writeBrightnessIsUnconfirmedWhenTheReadBackDiffers()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        int writeCount = 0;
        transport.beforeWrite = [&] {
            ++writeCount;
            // The keyboard answers the set, but the read-back shows a different value (another
            // host changed it in between, or the write silently failed).
            transport.pendingReads.push_back(writeCount == 1 ? setAnswer(0) : getAnswer(0, 400));
        };
        const auto result = writeBrightness(transport, 600, 200ms);
        QCOMPARE(result.error, LinkError::Unconfirmed);
        QCOMPARE(result.confirmed, std::uint16_t{400});
    }

    void writeBrightnessTimesOutWithoutAnEcho()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        const auto result = writeBrightness(transport, 600, 30ms);
        QCOMPARE(result.error, LinkError::Timeout);
    }

    void aDisconnectedTransportIsReportedAsGone()
    {
        FakeTransport transport;
        QVERIFY(transport.open());
        transport.disconnected = true;
        QCOMPARE(readBrightness(transport, 30ms).error, LinkError::Disconnected);
        QCOMPARE(writeBrightness(transport, 600, 30ms).error, LinkError::Disconnected);
    }

    void aFailedWriteIsReportedAndNothingIsAwaited()
    {
        FakeTransport transport; // never opened: write() fails
        QCOMPARE(readBrightness(transport, 30ms).error, LinkError::WriteFailed);
    }
};

QTEST_APPLESS_MAIN(TestKeyboardLink)
#include "test_keyboardlink.moc"
