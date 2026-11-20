// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Every place that loads an icon of the desktop's theme goes through themeIconImage(), which takes one lock for the whole process: on a KDE
// desktop the icon loader (KIconLoader) does not stand two threads at once, and the settings window crashed when two of them were in it
// together (a segmentation fault in KIconLoader::loadScaledIcon, two threads in the core dumps). The first fix guarded only the provider of
// the tiles; the provider of the icons of the key editor (image://themeicon) loaded icons without it, from the same loader thread of Qt
// Quick. This test holds the lock itself and checks that each way of loading a theme icon waits for it, which does not need KIconLoader.
#include <AppsBackend.h>
#include <ThemeIcon.h>
#include <TileRenderer.h>

#include <QGuiApplication>
#include <QSize>
#include <QTest>
#include <QThread>

#include <atomic>
#include <functional>
#include <memory>

using namespace galleond;

namespace {

// Runs `call` on another thread while this one holds the lock of the theme icons: it must not finish before the lock is let go, and it must
// finish after.
[[nodiscard]] bool waitsForTheLock(const std::function<void()> &call)
{
    std::atomic<bool> done{false};
    std::unique_ptr<QThread> worker;
    bool finishedWhileHeld = false;
    {
        const ThemeIconLock held;
        worker.reset(QThread::create([&call, &done] {
            call();
            done = true;
        }));
        worker->start();
        QTest::qWait(250); // plenty for a loader that does not wait
        finishedWhileHeld = done.load();
    }
    const bool finishedAfter = worker->wait(10000) && done.load();
    return !finishedWhileHeld && finishedAfter;
}

KeyConfig keyWithIcon(const QString &icon)
{
    KeyConfig config;
    config.key = 0;
    config.label = QStringLiteral("k");
    config.icon.name = icon;
    return config;
}

} // namespace

class TestThemeIcon : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void theIconsOfTheKeyEditorWaitForTheLock()
    {
        ThemeIconProvider provider;
        QVERIFY(waitsForTheLock([&provider] {
            QSize size;
            static_cast<void>(provider.requestPixmap(QStringLiteral("application-x-executable"), &size, QSize(32, 32)));
        }));
    }

    void theTilesOfTheKeysWaitForTheLock()
    {
        const TileRenderer renderer;
        QVERIFY(waitsForTheLock([&renderer] {
            static_cast<void>(renderer.render(keyWithIcon(QStringLiteral("application-x-executable")), QStringLiteral("stopped"), false, QString(), -1));
        }));
    }

    void theLoaderItselfWaitsForTheLock()
    {
        QVERIFY(waitsForTheLock([] { static_cast<void>(themeIconImage(QStringLiteral("application-x-executable"), 32)); }));
    }

    void theLockIsFreeAgainAfterALoad()
    {
        static_cast<void>(themeIconImage(QStringLiteral("no-such-icon-anywhere"), 32));
        // Taking the lock again must not block: a load that left it taken would hang here.
        const ThemeIconLock again;
        QVERIFY(true);
    }

    void anIconThatTheThemeDoesNotHaveGivesAnImageOnlyThroughTheFallback()
    {
        QVERIFY(themeIconImage(QStringLiteral("no-such-icon-anywhere"), 32).isNull());
        // With a fallback that the theme does not have either, still nothing; the call never fails.
        QVERIFY(themeIconImage(QStringLiteral("no-such-icon-anywhere"), 32, QStringLiteral("no-such-fallback-either")).isNull());
    }
};

QTEST_MAIN(TestThemeIcon)
#include "test_themeicon.moc"
