// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The image provider of the window (image://tiles/...) is asked by two kinds of thread at the same time: the GUI thread, for the images that are
// not `asynchronous`, and the image loader thread of Qt Quick, for the others (the preview of the keyboard). Drawing a tile loads icons, and on a
// KDE desktop that goes through KIconLoader, which does not stand two threads at once: the window crashed (a segmentation fault in
// KIconLoader::loadScaledIcon with two threads in it in the core dump). This test forces the same situation, with a provider whose drawing
// counts how many threads are inside it together. The real icon loader is not here (the CI has no KDE platform theme): what is checked is the
// guarantee that matters, one tile drawn at a time, whatever the number of threads that ask.
#include <BosunBackend.h>

#include <QElapsedTimer>
#include <QSize>
#include <QTest>
#include <QThread>

#include <atomic>
#include <memory>
#include <vector>

namespace {

// Draws nothing, but stays inside long enough for any other thread that is let in to be seen.
class CountingProvider : public bosun::TileProvider
{
public:
    CountingProvider() : bosun::TileProvider(nullptr) {}

    std::atomic<int> inside{0};
    std::atomic<int> mostInside{0};
    std::atomic<int> drawn{0};

protected:
    [[nodiscard]] QPixmap renderTile(const QString &id, QSize *size) const override
    {
        auto *self = const_cast<CountingProvider *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast) -- counters of the probe only
        const int now = ++self->inside;
        int seen = self->mostInside.load();
        while (now > seen && !self->mostInside.compare_exchange_weak(seen, now)) {
        }
        QElapsedTimer held;
        held.start();
        while (held.nsecsElapsed() < 300000) { // 0.3 ms inside: two threads let in together overlap for certain
        }
        --self->inside;
        ++self->drawn;
        if (size)
            *size = QSize(static_cast<int>(id.size()), 6); // so that the answer can be checked
        return {};
    }
};

} // namespace

class TestTileProvider : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void theGuiThreadAndTheLoaderThreadsNeverDrawATileTogether()
    {
        CountingProvider provider;
        constexpr int Loaders = 3;
        constexpr int CallsEach = 40;
        std::atomic<bool> go{false};

        std::vector<std::unique_ptr<QThread>> loaders;
        for (int i = 0; i < Loaders; ++i) {
            loaders.emplace_back(QThread::create([&provider, &go] {
                while (!go.load()) {
                }
                for (int call = 0; call < CallsEach; ++call) {
                    QSize size;
                    static_cast<void>(provider.requestPixmap(QStringLiteral("main/%1").arg(call), &size, QSize()));
                }
            }));
            loaders.back()->start();
        }
        go = true;
        // And this thread, the GUI thread of the window, asks at the same time.
        for (int call = 0; call < CallsEach; ++call) {
            QSize size;
            static_cast<void>(provider.requestPixmap(QStringLiteral("main/%1").arg(call), &size, QSize()));
        }
        for (auto &loader : loaders)
            QVERIFY(loader->wait(20000));

        QCOMPARE(provider.drawn.load(), (Loaders + 1) * CallsEach); // every request was served
        QCOMPARE(provider.mostInside.load(), 1);                    // and never two drawings at the same time
    }

    void theAnswerOfTheDrawingAndItsSizeAreGivenBack()
    {
        CountingProvider provider;
        QSize size;
        const QPixmap pixmap = provider.requestPixmap(QStringLiteral("abc"), &size, QSize());
        QVERIFY(pixmap.isNull());
        QCOMPARE(size, QSize(3, 6));
        // No size asked: still served.
        static_cast<void>(provider.requestPixmap(QStringLiteral("abcd"), nullptr, QSize()));
        QCOMPARE(provider.drawn.load(), 2);
    }
};

QTEST_MAIN(TestTileProvider)
#include "test_tileprovider.moc"
