// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// Endurance of the screen controller (ScreenController, with its renderer and the module driver on a fake transport): wallpaper
// steps, the screensaver taking and giving back the screen, a track starting and stopping, the module unplugged and plugged back.
// After every action the pictures sent are checked, and the memory of the process in a timed run. It never touches hardware.
//
// Short by default (a fixed number of cycles, part of ctest). GALLEON_ENDURANCE_SECONDS=<n> plays cycles for n seconds instead; on
// Linux it then checks that the memory stops growing. Second step of the endurance plan (docs/ROADMAP.md), after test_endurance.

#include "FakeTransport.h"

#include <ScreenController.h>
#include <ScreenRenderer.h>
#include <galleon/StreamDeck.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <array>
#include <memory>
#include <random>
#include <utility>

using namespace galleond;
using galleon::StreamDeck;

namespace {

// The tests keep to their own configuration folder: the saved wallpaper must not reach the real ~/.config.
void isolateSettings()
{
    static const QTemporaryDir folder(QDir::tempPath() + QStringLiteral("/galleoncore-test-config-XXXXXX"));
    qputenv("XDG_CONFIG_HOME", folder.path().toUtf8());
}
Q_CONSTRUCTOR_FUNCTION(isolateSettings)

constexpr int DefaultCycles = 40;
constexpr qint64 MaxRssGrowthKib = qint64{40} * 1024; // from a quarter of a timed run to its end; images and the renderer included

qint64 residentKib()
{
    QFile file(QStringLiteral("/proc/self/statm"));
    if (!file.open(QIODevice::ReadOnly)) return 0;
    const QList<QByteArray> fields = file.readAll().split(' ');
    if (fields.size() < 2) return 0;
    return fields.at(1).toLongLong() * 4; // pages of 4 KiB: two readings of the same machine are compared, nothing else
}

class FakeMedia : public MediaController
{
public:
    [[nodiscard]] State state() const override { return m_state; }
    [[nodiscard]] NowPlaying nowPlaying() const override { return m_track; }
    void playPause() override {}
    void next() override {}
    void previous() override {}
    void stop() override {}
    void changeVolume(int /*percent*/) override {}
    void seek(int /*seconds*/) override {}
    void set(State state, const NowPlaying &track)
    {
        m_state = state;
        m_track = track;
        Q_EMIT stateChanged();
        Q_EMIT nowPlayingChanged();
    }

private:
    State m_state = State::NoPlayer;
    NowPlaying m_track;
};

// The pictures sent to the screen so far (each goes out twice in a row and counts once, as the module needs).
std::size_t screenPictures(const FakeTransport &transport)
{
    std::size_t count = 0;
    for (const auto &r : transport.writes) {
        if (r.size() > 10 && r[0] == 0x02 && r[1] == 0x0c && r[10] == 1) ++count;
    }
    return count / 2;
}

} // namespace

class TestEnduranceScreen : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void manyActionsAndUnpluggingsKeepTheScreenWorking()
    {
        const QTemporaryDir dir;
        QDir().mkpath(dir.filePath(QStringLiteral("wall/style")));
        const std::array<QColor, 3> colours{QColor(30, 160, 60), QColor(200, 30, 30), QColor(40, 60, 200)};
        for (int i = 0; i < 3; ++i) {
            QImage picture(1600, 900, QImage::Format_RGB32);
            picture.fill(colours.at(static_cast<std::size_t>(i)));
            QVERIFY(picture.save(dir.filePath(QStringLiteral("wall/style/w%1.png").arg(i))));
        }

        auto fake = std::make_unique<FakeTransport>();
        FakeTransport *transport = fake.get();
        StreamDeck deck(std::move(fake));
        QVERIFY(deck.open());
        const QString widgets = QStringLiteral(GALLEON_SOURCE_WIDGETS);
        ScreenRenderer wallpaperRenderer(widgets + QStringLiteral("/WallpaperClock.qml"));
        ScreenRenderer nowPlayingRenderer(widgets + QStringLiteral("/NowPlaying.qml"));
        FakeMedia media;
        ScreenController controller(&deck, &wallpaperRenderer, WallpaperCatalog(dir.filePath(QStringLiteral("wall"))),
                                    widgets + QStringLiteral("/WallpaperClock.qml"), &media, &nowPlayingRenderer);
        controller.setDeckCheckIntervalMs(50);
        QVERIFY(controller.SetWallpaper(QStringLiteral("style/w0.png")));

        const int seconds = qEnvironmentVariableIntValue("GALLEON_ENDURANCE_SECONDS");
        const bool timed = seconds > 0;
        std::mt19937 random(2468); // NOLINT(cert-msc32-c,cert-msc51-cpp): a fixed seed on purpose, a failure must be replayable
        QElapsedTimer clock;
        clock.start();
        qint64 reference = 0;
        int cycles = 0;

        // Each action is followed by the check of what it must (or must not) have drawn. The writes are cleared first: a real module
        // does not keep what it was sent, and the count must be of this action only.
        const auto expectPicture = [&](const char *what) {
            QTRY_VERIFY2_WITH_TIMEOUT(screenPictures(*transport) > 0, what, 3000);
        };

        while (timed ? clock.elapsed() < seconds * 1000LL : cycles < DefaultCycles) {
            transport->writes.clear();
            transport->featureReports.clear();
            switch (cycles % 5) {
            case 0: { // the dial turns: the wallpaper changes and is drawn
                controller.stepWallpaper(1 + static_cast<int>(random() % 3));
                expectPicture("a step of the wallpaper must draw a picture");
                break;
            }
            case 1: { // the screensaver takes the screen: nothing is drawn; it gives it back: the screen is drawn again
                controller.setSuspended(true);
                controller.stepWallpaper(1);
                QTest::qWait(60);
                QCOMPARE(screenPictures(*transport), std::size_t{0});
                controller.setSuspended(false);
                expectPicture("leaving the screensaver must draw the screen again");
                break;
            }
            case 2: { // a track plays, then stops
                NowPlaying track;
                track.title = QStringLiteral("Title %1").arg(cycles);
                track.artist = QStringLiteral("Artist");
                track.positionSeconds = 10;
                track.durationSeconds = 100;
                media.set(MediaController::State::Playing, track);
                QTest::qWait(30);
                media.set(MediaController::State::Stopped, {});
                break;
            }
            case 3: { // the module is unplugged, found again by itself, and the screen is drawn at once
                const QSignalSpy changed(&controller, &ScreenController::deckAvailableChanged);
                // A module that is gone cannot be opened either: without this the controller, looking again every 50 ms, would open it
                // between two failed reads and the signal would flap.
                transport->disconnected = true;
                transport->openSucceeds = false;
                QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
                QVERIFY(!controller.deckAvailable());
                transport->disconnected = false;
                transport->openSucceeds = true;
                QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 2, 3000);
                QVERIFY(controller.deckAvailable());
                expectPicture("a module plugged back must show the wallpaper at once");
                break;
            }
            default: { // a keep-alive refused takes the module away the same way
                const QSignalSpy changed(&controller, &ScreenController::deckAvailableChanged);
                transport->featureSucceeds = false;
                transport->openSucceeds = false;
                QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
                transport->featureSucceeds = true;
                transport->openSucceeds = true;
                QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 2, 3000);
                expectPicture("a module back after a refused keep-alive must be drawn");
                break;
            }
            }
            QVERIFY(controller.deckAvailable());
            ++cycles;
            if (timed && reference == 0 && clock.elapsed() >= seconds * 250LL) reference = residentKib();
        }

        if (timed && reference > 0) {
            const qint64 end = residentKib();
            qInfo().noquote() << "endurance (screen):" << cycles << "cycles in" << clock.elapsed() / 1000 << "s, memory" << reference << "KiB ->" << end << "KiB";
            QVERIFY2(end - reference <= MaxRssGrowthKib, qPrintable(QStringLiteral("memory grew from %1 to %2 KiB").arg(reference).arg(end)));
        }
    }
};

QTEST_MAIN(TestEnduranceScreen)
#include "test_endurance_screen.moc"
