// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// KeyboardController with a fake keyboard that answers like the real one (a set echo, then the value on a get).
// Never opens real hardware: every opener here returns a fake.

#include "KeyboardController.h"

#include <galleon/Transport.h>
#include <galleon/keyboard/BragiFrame.h>

#include <QSignalSpy>
#include <QTest>

#include <deque>
#include <memory>
#include <mutex>
#include <vector>

using namespace galleond;
using namespace galleon::keyboard;

namespace {

// State shared with the worker thread, which owns the transport for one job.
struct Keyboard
{
    std::mutex mutex;
    std::vector<int> setValues; // every value written, in order
    int opens = 0;
    int closes = 0;
    int shownValue = 500;       // what the keyboard currently holds
    bool answersSet = true;
    bool available = true;
};

class FakeKeyboard final : public galleon::Transport
{
public:
    explicit FakeKeyboard(std::shared_ptr<Keyboard> state) : m_state(std::move(state)) {}

    bool open() override { m_open = true; return true; }
    void close() override
    {
        if (m_open) {
            const std::scoped_lock lock(m_state->mutex);
            ++m_state->closes;
        }
        m_open = false;
    }
    [[nodiscard]] bool isOpen() const override { return m_open; }
    [[nodiscard]] QString serialNumber() const override { return QStringLiteral("FAKE"); }
    bool sendFeatureReport(std::span<const std::uint8_t> /*report*/) override { return false; }

    bool write(std::span<const std::uint8_t> report) override
    {
        const std::scoped_lock lock(m_state->mutex);
        std::vector<std::uint8_t> answer(PayloadSize, 0);
        if (report.size() != WriteSize)
            return false;
        answer[3] = report[4];
        if (report[4] == CommandSet) {
            m_state->shownValue = report[7] | (report[8] << 8);
            m_state->setValues.push_back(m_state->shownValue);
            answer[4] = 0;
        } else {
            answer[4] = 0;
            for (std::size_t i = 0; i < 4; ++i)
                answer[5 + i] = static_cast<std::uint8_t>((m_state->shownValue >> (8U * i)) & 0xff);
        }
        if (report[4] != CommandSet || m_state->answersSet)
            m_answers.push_back(answer);
        return true;
    }

    int read(std::span<std::uint8_t> buffer) override
    {
        if (m_answers.empty())
            return 0;
        const auto answer = m_answers.front();
        m_answers.pop_front();
        std::copy_n(answer.begin(), std::min(answer.size(), buffer.size()), buffer.begin());
        return static_cast<int>(answer.size());
    }

private:
    std::shared_ptr<Keyboard> m_state;
    std::deque<std::vector<std::uint8_t>> m_answers;
    bool m_open = false;
};

KeyboardController::Opener openerFor(const std::shared_ptr<Keyboard> &state)
{
    return [state]() -> std::unique_ptr<galleon::Transport> {
        {
            const std::scoped_lock lock(state->mutex);
            if (!state->available)
                return nullptr;
            ++state->opens;
        }
        auto fake = std::make_unique<FakeKeyboard>(state);
        fake->open();
        return fake;
    };
}

} // namespace

class TestKeyboardController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void onlyWholeStepsOfOneHundredFromZeroToAThousandAreAccepted()
    {
        for (const int ok : {0, 100, 500, 1000})
            QCOMPARE(KeyboardController::acceptedBrightness(ok), std::optional<std::uint16_t>(ok));
        for (const int bad : {-100, -1, 1, 50, 150, 999, 1001, 1100, 100000})
            QVERIFY(!KeyboardController::acceptedBrightness(bad).has_value());
    }

    void aRefusedValueIsNeverSentAndTheKeyboardIsNotOpened()
    {
        const auto state = std::make_shared<Keyboard>();
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(!controller.SetBrightness(150));
        QVERIFY(!controller.SetBrightness(1100));
        QTest::qWait(50);
        QCOMPARE(written.size(), 0);
        QCOMPARE(state->opens, 0);
    }

    void nothingIsOpenedUntilARequestIsMade()
    {
        const auto state = std::make_shared<Keyboard>();
        const KeyboardController controller(openerFor(state));
        QTest::qWait(50);
        QCOMPARE(state->opens, 0);
    }

    void aRequestWritesTheValueReadsItBackAndClosesTheKeyboard()
    {
        const auto state = std::make_shared<Keyboard>();
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(controller.SetBrightness(300));
        QTRY_COMPARE(written.size(), 1);
        QCOMPARE(written.constFirst().at(0).toInt(), 300);
        QVERIFY(written.constFirst().at(1).toBool());
        QCOMPARE(state->setValues, std::vector<int>{300});
        QCOMPARE(state->opens, 1);
        QCOMPARE(state->closes, 1);
    }

    void aKeyboardThatCannotBeOpenedReportsWhyAndWritesNothing()
    {
        const auto state = std::make_shared<Keyboard>();
        state->available = false;
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(controller.SetBrightness(200));
        QTRY_COMPARE(written.size(), 1);
        QVERIFY(!written.constFirst().at(1).toBool());
        QVERIFY(!written.constFirst().at(2).toString().isEmpty());
        QVERIFY(state->setValues.empty());
    }

    void aWriteTheKeyboardDoesNotConfirmIsReportedAsFailed()
    {
        const auto state = std::make_shared<Keyboard>();
        state->answersSet = false; // no echo: the write is never confirmed
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QVERIFY(controller.SetBrightness(700));
        QTRY_COMPARE_WITH_TIMEOUT(written.size(), 1, 5000);
        QVERIFY(!written.constFirst().at(1).toBool());
    }

    void requestsMadeWhileOneIsServedKeepOnlyTheNewestAndAreSpacedApart()
    {
        const auto state = std::make_shared<Keyboard>();
        KeyboardController controller(openerFor(state));
        const QSignalSpy written(&controller, &KeyboardController::brightnessWritten);
        QElapsedTimer clock;
        clock.start();
        QVERIFY(controller.SetBrightness(100));
        QVERIFY(controller.SetBrightness(200));
        QVERIFY(controller.SetBrightness(300));
        QVERIFY(controller.SetBrightness(400));
        QTRY_COMPARE_WITH_TIMEOUT(written.size(), 2, 5000);
        // The first request goes at once, then only the newest of the others: 200 and 300 were replaced.
        QCOMPARE(state->setValues, (std::vector<int>{100, 400}));
        QVERIFY2(clock.elapsed() >= KeyboardController::MinIntervalMs, "two writes must be at least MinIntervalMs apart");
    }
};

QTEST_GUILESS_MAIN(TestKeyboardController)
#include "test_keyboardcontroller.moc"
