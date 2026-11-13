// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

// The watchdog's release sequence over a fake keyboard that answers like the real one (answers and byte values
// from the capture in docs/protocol/README.md "Live per-key lighting (Windows, 2026-10-25)"), and the request
// builders against the allowlist. Never opens real hardware.

#include <galleon/Transport.h>
#include <galleon/keyboard/LiveCommandGuard.h>
#include <galleon/keyboard/LiveRelease.h>
#include <galleon/keyboard/LiveRequests.h>

#include <QTest>

#include <deque>
#include <set>
#include <vector>

using namespace galleon::keyboard;
using namespace std::chrono_literals;

namespace {

const std::array<std::uint8_t, 4> Random{0x11, 0x22, 0x33, 0x44};

class FakeKeyboard final : public galleon::Transport
{
public:
    // What the keyboard holds and how it misbehaves.
    std::uint8_t mode = 2;
    std::uint8_t sessionId = 1;
    std::set<std::uint8_t> silentCommands;     // commands that get no answer at all
    std::set<std::uint8_t> badStatusCommands;  // commands answered with a non-zero status
    int silentFirstExchanges = 0;              // the first N requests get no answer (then it recovers)
    bool modeStaysWhenSet = false;             // the keyboard ignores the set of the mode
    std::set<int> lostGets;                    // numbers (1, 2, ...) of the get requests that get no answer

    std::vector<std::vector<std::uint8_t>> sent; // every 129-byte report written

    bool open() override { m_open = true; return true; }
    void close() override { m_open = false; }
    [[nodiscard]] bool isOpen() const override { return m_open; }
    [[nodiscard]] QString serialNumber() const override { return QStringLiteral("FAKE"); }
    bool sendFeatureReport(std::span<const std::uint8_t> /*report*/) override { return false; }

    bool write(std::span<const std::uint8_t> report) override
    {
        sent.emplace_back(report.begin(), report.end());
        if (silentFirstExchanges > 0) {
            --silentFirstExchanges;
            return true;
        }
        const std::uint8_t command = report[4];
        if (command == CommandGet && lostGets.contains(++m_gets))
            return true;
        if (silentCommands.contains(command))
            return true;
        std::vector<std::uint8_t> answer(128, 0);
        answer[3] = command;
        answer[4] = badStatusCommands.contains(command) ? 3 : 0;
        if (command == CommandSessionStart) {
            for (std::size_t i = 0; i < 4; ++i)
                answer[5 + i] = report[6 + i];
            answer[9] = sessionId;
        } else if (command == CommandGet) {
            answer[5] = mode;
        } else if (command == CommandSet && !modeStaysWhenSet) {
            mode = report[7];
        }
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

    [[nodiscard]] std::vector<std::uint8_t> commands() const
    {
        std::vector<std::uint8_t> list;
        list.reserve(sent.size());
        for (const auto &report : sent)
            list.push_back(report[4]);
        return list;
    }

private:
    bool m_open = false;
    int m_gets = 0;
    std::deque<std::vector<std::uint8_t>> m_answers;
};

ReleaseResult run(FakeKeyboard &keyboard)
{
    return releaseIfHostMode(keyboard, Random, 20ms, 0ms);
}

using Commands = std::vector<std::uint8_t>;

} // namespace

class TestKeyboardRelease : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // ---- the request builders -----------------------------------------------------------------------
    void theRequestsAreTheBytesOfTheCapture()
    {
        // Each line of the command log, after the report ID: `00 01 <session> <command> ...`.
        const auto bytes = [](const Request &request) { return std::vector<std::uint8_t>(request.begin() + 1, request.begin() + 12); };
        QCOMPARE(bytes(sessionStartRequest(Random)), (std::vector<std::uint8_t>{0x00, 0x01, 0x00, 0x1b, 0x01, 0x11, 0x22, 0x33, 0x44, 0x00, 0x00}));
        QCOMPARE(bytes(getPropertyIn(1, 3)), (std::vector<std::uint8_t>{0x00, 0x01, 0x01, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
        QCOMPARE(bytes(setModeRequest(1, 2)), (std::vector<std::uint8_t>{0x00, 0x01, 0x01, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00}));
        QCOMPARE(bytes(setModeRequest(1, 1)), (std::vector<std::uint8_t>{0x00, 0x01, 0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}));
        QCOMPARE(bytes(openLiveFileRequest(1)), (std::vector<std::uint8_t>{0x00, 0x01, 0x01, 0x0d, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00}));
        QCOMPARE(bytes(unbindRequest(1)), (std::vector<std::uint8_t>{0x00, 0x01, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
        QCOMPARE(bytes(pingRequest(1)), (std::vector<std::uint8_t>{0x00, 0x01, 0x01, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    }

    void everyRequestBuilderProducesWhatTheAllowlistAllows()
    {
        const LiveCommandGuard guard;
        QCOMPARE(guard.check(sessionStartRequest(Random)), Refusal::None);
        QCOMPARE(guard.check(getPropertyIn(5, 3)), Refusal::None);
        QCOMPARE(guard.check(setModeRequest(5, ModeSelfOperated)), Refusal::None);
        QCOMPARE(guard.check(setModeRequest(5, ModeHostControlled)), Refusal::None);
        QCOMPARE(guard.check(unbindRequest(5)), Refusal::None);
        QCOMPARE(guard.check(pingRequest(5)), Refusal::None);
        QCOMPARE(guard.check(openLiveFileRequest(5)), Refusal::None);
    }

    // ---- the release --------------------------------------------------------------------------------
    void aKeyboardLeftInHostModeIsReleasedAndReadBackAsModeOne()
    {
        FakeKeyboard keyboard;
        const ReleaseResult result = run(keyboard);
        QCOMPARE(result.outcome, ReleaseOutcome::Released);
        QCOMPARE(result.modeBefore, std::optional<std::uint8_t>(2));
        QCOMPARE(result.attempts, 1);
        QCOMPARE(keyboard.mode, std::uint8_t{1});
        // session start, read the mode, unbind, set the mode, read it back: the sequence of the capture.
        QCOMPARE(keyboard.commands(), (Commands{CommandSessionStart, CommandGet, CommandUnbind, CommandSet, CommandGet}));
    }

    void aKeyboardAlreadyInItsOwnModeIsLeftAloneAndNothingIsWritten()
    {
        FakeKeyboard keyboard;
        keyboard.mode = 1; // the writer had released already
        const ReleaseResult result = run(keyboard);
        QCOMPARE(result.outcome, ReleaseOutcome::NotNeeded);
        QCOMPARE(keyboard.commands(), (Commands{CommandSessionStart, CommandGet}));
        QCOMPARE(keyboard.mode, std::uint8_t{1});
    }

    void anyOtherModeIsNotTheWatchdogsBusiness()
    {
        for (const std::uint8_t mode : std::initializer_list<std::uint8_t>{0, 3, 4, 5, 9}) {
            FakeKeyboard keyboard;
            keyboard.mode = mode;
            QCOMPARE(run(keyboard).outcome, ReleaseOutcome::NotNeeded);
            QCOMPARE(keyboard.mode, mode); // not even a bootloader mode is touched
            QCOMPARE(keyboard.commands().size(), std::size_t{2});
        }
    }

    void aFailedUnbindDoesNotStopTheModeFromBeingSetBack()
    {
        FakeKeyboard keyboard;
        keyboard.badStatusCommands.insert(CommandUnbind);
        const ReleaseResult result = run(keyboard);
        QCOMPARE(result.outcome, ReleaseOutcome::Released);
        QCOMPARE(keyboard.mode, std::uint8_t{1});
        QCOMPARE(result.attempts, 1);
    }

    void aModeTheKeyboardWillNotLeaveIsReportedAsFailedAfterThreeAttempts()
    {
        FakeKeyboard keyboard;
        keyboard.modeStaysWhenSet = true;
        const ReleaseResult result = run(keyboard);
        QCOMPARE(result.outcome, ReleaseOutcome::Failed);
        QCOMPARE(result.attempts, MaxReleaseAttempts);
        QCOMPARE(result.lastError, LinkError::Unconfirmed);
        // Never more than three tries, each a full sequence: no loop without end.
        QCOMPARE(keyboard.commands().size(), static_cast<std::size_t>(5) * MaxReleaseAttempts);
    }

    void aKeyboardThatAnswersNothingIsRetriedThenGivenUpOnWithoutAnyRelease()
    {
        FakeKeyboard keyboard;
        keyboard.silentCommands.insert(CommandGet); // the mode can never be read
        const ReleaseResult result = run(keyboard);
        QCOMPARE(result.outcome, ReleaseOutcome::Failed);
        QCOMPARE(result.attempts, MaxReleaseAttempts);
        QVERIFY(!result.modeBefore);
        for (const std::uint8_t command : keyboard.commands())
            QVERIFY(command == CommandSessionStart || command == CommandGet); // an unread mode is never acted on
        QCOMPARE(keyboard.mode, std::uint8_t{2});
    }

    void aKeyboardThatRecoversOnTheSecondAttemptIsReleased()
    {
        FakeKeyboard keyboard;
        keyboard.silentFirstExchanges = 1; // the very first session start is lost
        const ReleaseResult result = run(keyboard);
        QCOMPARE(result.outcome, ReleaseOutcome::Released);
        QCOMPARE(result.attempts, 2);
    }

    void aReleaseThatTookEffectButWasNotConfirmedIsReportedAsReleasedNotAsNotNeeded()
    {
        FakeKeyboard keyboard;
        keyboard.lostGets = {2}; // the read back that confirms the first release is lost
        const ReleaseResult result = run(keyboard);
        // The next attempt reads 1: the keyboard is released, and it must not be said that nothing was needed.
        QCOMPARE(result.outcome, ReleaseOutcome::Released);
        QCOMPARE(result.attempts, 2);
        QCOMPARE(result.modeBefore, std::optional<std::uint8_t>(2));
        QCOMPARE(keyboard.mode, std::uint8_t{1});
    }

    void aSessionStartWithABadStatusOrNoSessionIsAFailureAndNothingElseIsSent()
    {
        FakeKeyboard badStatus;
        badStatus.badStatusCommands.insert(CommandSessionStart);
        QCOMPARE(run(badStatus).outcome, ReleaseOutcome::Failed);
        for (const std::uint8_t command : badStatus.commands())
            QCOMPARE(command, CommandSessionStart);

        FakeKeyboard noSession;
        noSession.sessionId = 0;
        QCOMPARE(run(noSession).outcome, ReleaseOutcome::Failed);
        for (const std::uint8_t command : noSession.commands())
            QCOMPARE(command, CommandSessionStart);
    }

    void theWatchdogOnlyEverSendsWhatItsSequenceNeedsAndNeverALiveFrame()
    {
        FakeKeyboard keyboard;
        static_cast<void>(run(keyboard));
        const std::set<std::uint8_t> allowed{CommandSessionStart, CommandGet, CommandUnbind, CommandSet};
        for (const auto &report : keyboard.sent) {
            QCOMPARE(report.size(), WriteSize);
            QVERIFY(allowed.contains(report[4]));
            if (report[4] == CommandSet) {
                QCOMPARE(report[5], PropertyMode);
                QCOMPARE(report[7], ModeSelfOperated); // it only ever sets the mode to 1
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestKeyboardRelease)
#include "test_keyboardrelease.moc"
