// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyInjector.h"
#ifdef Q_OS_WIN
#include "PlatformBackends.h"
#endif

#include <QtTest>
#include <vector>
#include <utility>

using namespace galleond;

class TestKeyInjector : public QObject
{
    Q_OBJECT
private slots:
#ifdef Q_OS_WIN
    void nativeFactoryDoesNotSendInputAtConstruction()
    {
        const auto keyboard = createKeyInjector();
        QVERIFY(keyboard != nullptr);
        QVERIFY(!keyboard->key(0, true));
    }
#endif

    void whitelist()
    {
        const auto names = allowedKeyNames().split(QStringLiteral(", "));
        QCOMPARE(names.size(), AllowedKeyCount);
        for (const auto &name : names) {
            const auto code = keyCodeForName(name);
            QVERIFY(code.has_value());
            QVERIFY(isAllowedKeyCode(code.value_or(0)));
        }
        QVERIFY(!keyCodeForName(QStringLiteral("enter")));
        QVERIFY(!keyCodeForName(QStringLiteral("KP7")));
        QVERIFY(!keyCodeForName(QString{}));
        QVERIFY(!isAllowedKeyCode(0));
        QVERIFY(!isAllowedKeyCode(AllowedKeyCount + 1));
    }

    void theFunctionKeysF1ToF24AndTheArrowsAreAllowed()
    {
        QSet<int> codes;
        for (int n = 1; n <= 24; ++n) {
            const auto code = keyCodeForName(QStringLiteral("f%1").arg(n));
            QVERIFY2(code.has_value(), qPrintable(QStringLiteral("f%1").arg(n)));
            QVERIFY(isAllowedKeyCode(code.value_or(0)));
            codes.insert(code.value_or(0));
        }
        for (const QString &name : {QStringLiteral("up"), QStringLiteral("down"), QStringLiteral("left"), QStringLiteral("right")}) {
            const auto code = keyCodeForName(name);
            QVERIFY2(code.has_value(), qPrintable(name));
            QVERIFY(isAllowedKeyCode(code.value_or(0)));
            codes.insert(code.value_or(0));
        }
        QCOMPARE(codes.size(), 28); // each name is its own key
        // No function key beyond F24, and no spelling that is not exactly a name of the list.
        for (const QString &name : {QStringLiteral("f25"), QStringLiteral("F13"), QStringLiteral("F1"), QStringLiteral("f013"), QStringLiteral("f01"),
                                    QStringLiteral("Up"), QStringLiteral("arrowup"), QStringLiteral("f0"), QStringLiteral("f")})
            QVERIFY2(!keyCodeForName(name), qPrintable(name));
    }

    // The list is the whole of what a pages file or a theme can make galleond type. This is the exact list: a key added by mistake, above
    // all one that types a character or confirms a dialog (a letter, Enter, Space, Tab, Escape, Delete, a modifier, a main-row digit),
    // fails here and has to be added to it on purpose, with the security review of AGENTS.md.
    void theWholeListIsExactlyTheOneTheOwnerDecided()
    {
        const QString expected = QStringLiteral(
            "kp0, kp1, kp2, kp3, kp4, kp5, kp6, kp7, kp8, kp9, kpdot, kpenter, kpplus, kpminus, kpasterisk, kpslash, numlock, backspace, "
            "f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, up, down, left, right");
        QCOMPARE(allowedKeyNames(), expected);
        for (const QString &name : {QStringLiteral("enter"), QStringLiteral("space"), QStringLiteral("tab"), QStringLiteral("esc"), QStringLiteral("escape"),
                                    QStringLiteral("delete"), QStringLiteral("a"), QStringLiteral("1"), QStringLiteral("ctrl"), QStringLiteral("alt"),
                                    QStringLiteral("shift"), QStringLiteral("meta"), QStringLiteral("super"), QStringLiteral("home"), QStringLiteral("end"),
                                    QStringLiteral("pageup")})
            QVERIFY2(!keyCodeForName(name), qPrintable(name));
    }

    void theArrowsAndFunctionKeysAreHeldAndReleasedByTheTrackedKeyboard()
    {
        for (const QString &name : {QStringLiteral("f1"), QStringLiteral("f12"), QStringLiteral("up"), QStringLiteral("right")}) {
            std::vector<std::pair<KeyCode, bool>> events;
            const int code = keyCodeForName(name).value_or(0);
            {
                TrackedKeyboard keyboard([&events](KeyCode key, bool down) {
                    events.emplace_back(key, down);
                    return true;
                });
                QVERIFY(keyboard.key(code, true));
                QVERIFY(keyboard.key(code, true)); // held: not sent twice
            }
            QCOMPARE(events.size(), std::size_t{2}); // down once, released at shutdown: never stuck
            QCOMPARE(static_cast<int>(events.front().first), code);
            QVERIFY(events.front().second);
            QVERIFY(!events.back().second);
        }
    }

    void aFunctionKeyIsHeldAndReleasedLikeAnyOther()
    {
        std::vector<std::pair<KeyCode, bool>> events;
        {
            TrackedKeyboard keyboard([&events](KeyCode key, bool down) {
                events.emplace_back(key, down);
                return true;
            });
            QVERIFY(keyboard.key(static_cast<int>(KeyCode::F13), true));
        }
        QCOMPARE(events.size(), std::size_t{2}); // released on shutdown: never stuck down
        QCOMPARE(events.back().first, KeyCode::F13);
        QVERIFY(!events.back().second);
    }

    void heldKeysReleasedOnShutdown()
    {
        std::vector<std::pair<KeyCode, bool>> events;
        const int code = static_cast<int>(KeyCode::Kp7);
        {
            TrackedKeyboard keyboard([&events](KeyCode key, bool down) {
                events.emplace_back(key, down);
                return true;
            });
            QVERIFY(!keyboard.key(0, true));
            QVERIFY(keyboard.key(code, true));
            QVERIFY(keyboard.key(code, true));
            QCOMPARE(events.size(), std::size_t{1});
        }
        QCOMPARE(events.size(), std::size_t{2});
        QCOMPARE(events.back().first, KeyCode::Kp7);
        QVERIFY(!events.back().second);
    }

    void failedPressDoesNotLeaveHeldKey()
    {
        int attempts = 0;
        {
            TrackedKeyboard keyboard([&attempts](KeyCode, bool) {
                ++attempts;
                return false;
            });
            QVERIFY(!keyboard.key(static_cast<int>(KeyCode::Kp1), true));
        }
        QCOMPARE(attempts, 1);
    }

    void failedReleaseRetriedOnShutdown()
    {
        int releases = 0;
        {
            TrackedKeyboard keyboard([&releases](KeyCode, bool down) {
                if (down)
                    return true;
                return ++releases > 1;
            });
            QVERIFY(keyboard.key(static_cast<int>(KeyCode::KpEnter), true));
            QVERIFY(!keyboard.key(static_cast<int>(KeyCode::KpEnter), false));
        }
        QCOMPARE(releases, 2);
    }
};

QTEST_GUILESS_MAIN(TestKeyInjector)
#include "test_keyinjector.moc"
