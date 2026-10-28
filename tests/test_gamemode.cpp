// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include <GameMode.h>
#include <GameInstallations.h>
#include <HomeWidgets.h>
#include <ScreenRenderer.h>
#include <QFile>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
using namespace galleond;
class TestGameMode : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void steamNonStoreShortcutsIdentifyTheActualExecutable() {
        const auto fixture = [](const QByteArray &executable) {
            QByteArray bytes;
            const auto field = [&bytes](char type, const QByteArray &key) { bytes.append(type); bytes.append(key); bytes.append('\0'); };
            field(0, "shortcuts"); field(0, "0");
            field(2, "appid"); bytes.append(QByteArray(4, '\0'));
            field(1, "AppName"); bytes.append("Grand Theft Auto III"); bytes.append('\0');
            field(1, "Exe"); bytes.append(executable); bytes.append('\0');
            field(1, "LaunchOptions"); bytes.append("ignored --arguments"); bytes.append('\0');
            field(0, "tags"); bytes.append(char(8));
            bytes.append(QByteArray(3, char(8)));
            return bytes;
        };
        const auto bytes = fixture(R"("G:\DVD Games\GTA III\gta3.exe")");
        const auto games = GameInstallations::steamShortcuts(bytes);
        QCOMPARE(games.size(), 1);
        QCOMPARE(GameInstallations::titleForPath("g:/dvd games/gta iii/GTA3.EXE", games), QStringLiteral("Grand Theft Auto III"));
        QVERIFY(GameInstallations::titleForPath("G:/DVD Games/GTA III/unrelated.exe", games).isEmpty());
        for (qsizetype i = 0; i < bytes.size(); ++i)
            QVERIFY(GameInstallations::steamShortcuts(bytes.left(i)).isEmpty());
        QVERIFY(GameInstallations::steamShortcuts(bytes + "trailing").isEmpty());
        QVERIFY(GameInstallations::steamShortcuts(fixture("steam://rungameid/123")).isEmpty());
        QVERIFY(GameInstallations::steamShortcuts(fixture("\"G:/Game/game.exe\" --arguments")).isEmpty());
        QVERIFY(GameInstallations::steamShortcuts(fixture("//server/game.exe")).isEmpty());
        QVERIFY(GameInstallations::steamShortcuts(QByteArray(1024 * 1024 + 1, 'x')).isEmpty());
    }
    void wineCaptureMatchesExeSuffixAndOnlyItsOwnCommand() {
        const QString game = QStringLiteral("/games/Heroic/Ghostrunner/Ghostrunner.exe");
        QCOMPARE(GameMode::capturedExecutable("Ghostrunner", game, {}, {}), game);
        QCOMPARE(GameMode::capturedExecutable("Ghostrunner", "/usr/bin/wine", game.toUtf8(), "/tmp"), game);
        QCOMPARE(GameMode::capturedExecutable("Ghostrunner", "/usr/bin/wine", "Ghostrunner.exe", "/games/Heroic/Ghostrunner"), game);
        QVERIFY(GameMode::capturedExecutable("Ghostrunner", "/usr/bin/launcher", QByteArray("launcher\0Ghostrunner.exe", 24), "/tmp").isEmpty());
        QVERIFY(GameMode::capturedExecutable("Ghostrunner", "/usr/bin/wine", "OtherGame.exe", "/tmp").isEmpty());
#ifndef Q_OS_WIN
        const QString shipping = QStringLiteral("/home/test/Games/Heroic/Ghostrunner/Ghostrunner/Binaries/Win64/Ghostrunner-Win64-Shipping.exe");
        QCOMPARE(GameMode::capturedExecutable("Ghostrunner-Win64-Shipping", "/usr/bin/wine64-preloader",
            "Z:\\home\\test\\Games\\Heroic\\Ghostrunner\\Ghostrunner\\Binaries\\Win64\\Ghostrunner-Win64-Shipping.exe", {}), shipping);
        QCOMPARE(GameMode::capturedExecutable("Ghostrunner-Win64-Shipping", "/usr/bin/wine64-preloader",
            "C:/Games/Ghostrunner/Binaries/Win64/Ghostrunner-Win64-Shipping.exe", {}, "/custom/prefix/drive_c"),
            QStringLiteral("/custom/prefix/drive_c/Games/Ghostrunner/Binaries/Win64/Ghostrunner-Win64-Shipping.exe"));
        QVERIFY(GameMode::capturedExecutable("Ghostrunner-Win64-Shipping", "/usr/bin/wine64-preloader",
            "C:/Games/Ghostrunner/Binaries/Win64/Ghostrunner-Win64-Shipping.exe", {}).isEmpty());
        const auto installed = GameInstallations::registryRecord("/home/test/Games/Heroic/Ghostrunner", "Ghostrunner");
        QCOMPARE(GameInstallations::titleForPath(shipping, {installed}), QStringLiteral("Ghostrunner"));
#endif
    }
    void launcherMetadataWorksOnAnyDriveWithoutFolderHeuristics() {
        const auto epic = GameInstallations::epicManifest(R"({"AppName":"opaque","DisplayName":"Red Dead Redemption 2","InstallLocation":"K:/Custom/opaque","LaunchExecutable":"RDR2.exe"})");
        QCOMPARE(GameInstallations::titleForPath("k:/custom/opaque/x64/RDR2.exe", {epic}), QStringLiteral("Red Dead Redemption 2"));
        QVERIFY(GameInstallations::titleForPath("K:/Custom/opaque-other/RDR2.exe", {epic}).isEmpty());
        const auto gog = GameInstallations::registryRecord("G:/Unusual/123/", "GOG Title");
        QCOMPARE(GameInstallations::titleForPath("G:/Unusual/123/game.exe", {gog}), QStringLiteral("GOG Title"));
        const auto ea = GameInstallations::registryRecord("K:/Anywhere/Sims", "The Sims 4");
        QCOMPARE(GameInstallations::titleForPath("K:/Anywhere/Sims/Game/Bin/TS4_x64.exe", {ea}), QStringLiteral("The Sims 4"));
        QVERIFY(GameInstallations::registryRecord("K:/", "Bad root").directory.isEmpty());
        QVERIFY(GameInstallations::registryRecord("relative/game", "Bad path").directory.isEmpty());
        QVERIFY(GameInstallations::epicManifest(R"({"DisplayName":"Incomplete","InstallLocation":"K:/Game","LaunchExecutable":"game.exe","bIsIncompleteInstall":true})").directory.isEmpty());
    }
    void installedMetadataOverridesOpaqueFolderNamesWithoutPrefixCollisions() {
        const QByteArray installed = R"({"opaque-id":{"install_path":"/games/opaque","title":"Red Dead Redemption 2"}})";
        const auto games = GameInstallations::legendaryManifest(installed);
        QCOMPARE(GameInstallations::titleForPath("/games/opaque/x64/RDR2.exe", games), QStringLiteral("Red Dead Redemption 2"));
        QVERIFY(GameInstallations::titleForPath("/games/opaque-other/RDR2.exe", games).isEmpty());
        QVERIFY(GameInstallations::legendaryManifest(QByteArray(1024 * 1024 + 1, ' ')).isEmpty());
    }
    void uniqueProcessNamesResolveBuildChildrenButNeverGenericOrAmbiguousNames() {
        auto ghost = GameInstallations::registryRecord("/games/opaque", "Ghostrunner");
        QCOMPARE(GameInstallations::titleForProcess("Ghostrunner-Win64-Shipping.exe", {ghost}), QStringLiteral("Ghostrunner"));
        ghost.executable = QStringLiteral("/games/opaque/custom.exe");
        QCOMPARE(GameInstallations::titleForProcess("custom", {ghost}), QStringLiteral("Ghostrunner"));
        auto other = GameInstallations::registryRecord("/games/other", "Other Game");
        other.executable = QStringLiteral("/games/other/custom.exe");
        QVERIFY(GameInstallations::titleForProcess("custom.exe", {ghost, other}).isEmpty());
        other.executable = QStringLiteral("/games/other/game.exe");
        QVERIFY(GameInstallations::titleForProcess("game.exe", {other}).isEmpty());
        QVERIFY(GameInstallations::titleForProcess("Ghostrunner2.exe", {ghost}).isEmpty());
    }
    void heroicSideloadsKeepTheirTitleWithoutTreatingTheWholeBinFolderAsAGame() {
        const auto games = GameInstallations::heroicLibrary(R"({"games":[{"title":"My Native Game","is_installed":true,"install":{"executable":"/custom/bin/game"}},{"title":"Not installed","is_installed":false,"install":{"executable":"/custom/bin/other"}}]})");
        QCOMPARE(GameInstallations::titleForPath("/custom/bin/game", games), QStringLiteral("My Native Game"));
        QVERIFY(GameInstallations::titleForPath("/custom/bin/other", games).isEmpty());
    }
    void linuxLauncherConfigsMatchOnlyTheirProgramWithoutExecutingYaml() {
        const auto lutris = GameInstallations::lutrisConfig("game:\n  exe: /custom/game/bin/run\nname: A Game\n", "fallback");
        QCOMPARE(GameInstallations::titleForPath("/custom/game/bin/run", {lutris}), QStringLiteral("A Game"));
        QVERIFY(GameInstallations::titleForPath("/custom/game/bin/helper", {lutris}).isEmpty());
        const auto bottle = GameInstallations::bottlesConfig("External_Programs:\n  id:\n    name: Bottle Game\n    path: C:/Games/Test/game.exe\n", "/custom/bottle");
        QCOMPARE(GameInstallations::titleForPath("/custom/bottle/drive_c/Games/Test/game.exe", bottle), QStringLiteral("Bottle Game"));
        QVERIFY(GameInstallations::lutrisConfig("game:\n  exe: !execute evil\n", "ignored").directory.isEmpty());
    }
    void joinedFolderTitlesAreReadableForArtworkSearch() {
        QCOMPARE(GameMode::readableTitle("RedDeadRedemption2"), QStringLiteral("Red Dead Redemption 2"));
        QCOMPARE(GameMode::readableTitle("Ghostrunner.exe"), QStringLiteral("Ghostrunner"));
    }
    void gamePathsProvideConservativeTitles() {
        QCOMPARE(GameMode::titleFromInstallPath("D:/Epic Games/Cyberpunk_2077/bin/game.exe"),
                 QStringLiteral("Cyberpunk 2077"));
        QCOMPARE(GameMode::titleFromInstallPath("/home/test/Games/Heroic/The-Witcher-3/bin/game.exe"),
                 QStringLiteral("The Witcher 3"));
        QCOMPARE(GameMode::titleFromInstallPath("/home/test/.var/app/com.usebottles.bottles/data/bottles/bottles/Old_Game/drive_c/game.exe"),
                 QStringLiteral("Old Game"));
        QVERIFY(GameMode::titleFromInstallPath("C:/Windows/System32/svchost.exe").isEmpty());
    }
    void captureControlsModeAndMissingSensorsAreExplicit() {
        GameMode mode;
        FpsSample sample{144.0, {}}; sample.process = "galleon-fixture.exe"; sample.token = "frame1";
        const auto active = mode.update(sample, {});
        QVERIFY(active.value("active").toBool());
        QCOMPARE(active.value("gpu").toString(), QString(QChar(0x2014)));
        QVERIFY(!mode.update(sample, {{"automatic", false}}).value("active").toBool());
        sample.cpuTemperature = 54; sample.gpuTemperature = 62;
        sample.cpuLoad = 37; sample.gpuLoad = 91;
        const auto temperatures = mode.update(sample, {});
        QVERIFY(temperatures.value("cpu").toString().contains("54"));
        QVERIFY(temperatures.value("gpu").toString().contains("62"));
        QVERIFY(!mode.update({}, {}).value("active").toBool());
    }
    void bundledGamingFallbackExists() {
        const QString resource = QStringLiteral(":/galleon/galleoncore_gaming_fallback.png");
        QVERIFY2(QFile::exists(resource), "Gaming fallback must be bundled for Windows and Linux");
        const QImage image(resource);
        QVERIFY(!image.isNull());
        QVERIFY(image.width() >= 200);
        QVERIFY(image.height() >= 100);
        ScreenRenderer renderer(QStringLiteral(GALLEON_SOURCE_WIDGETS "/WallpaperClock.qml"));
        const auto frame = renderer.render({{"gameState", QVariantMap{{"active", true},
                                                                        {"title", "Fallback test"},
                                                                        {"fps", "60"}}},
                                           {"live", false}}, QSize(720, 384));
        QVERIFY(!frame.isNull());
    }
    void fpsShadowImprovesContrastOnBrightArtwork() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString imagePath = dir.filePath(QStringLiteral("bright.png"));
        QImage artwork(720, 384, QImage::Format_RGB32);
        artwork.fill(Qt::white);
        QVERIFY(artwork.save(imagePath));
        const QVariantMap game{{"active", true}, {"title", "Bright game"},
                               {"image", QUrl::fromLocalFile(imagePath).toString()},
                               {"fps", "150"}};
        ScreenRenderer renderer(QStringLiteral(GALLEON_SOURCE_WIDGETS "/WallpaperClock.qml"));
        const QImage frame = renderer.render({{"gameState", game}, {"live", false}}, QSize(720, 384));
        QVERIFY(!frame.isNull());
        // Both points are below the same full-screen overlay and outside the FPS
        // glyphs; only the point inside the new halo should be much darker.
        const QColor shaded = frame.pixelColor(185, 220);
        const QColor clear = frame.pixelColor(245, 220);
        QVERIFY2(shaded.red() + 25 < clear.red(), "FPS halo should darken bright artwork");
        QVERIFY2(shaded.green() + 25 < clear.green(), "FPS halo should improve contrast");
    }
    void localArtworkAndSettingsAreAppliedAtomically() {
        const QTemporaryDir dir;
        const auto image = dir.filePath("art.png");
        QImage art(720, 384, QImage::Format_RGB32); art.fill(QColor("#204080")); QVERIFY(art.save(image));
        HomeWidgets editor(nullptr, dir.filePath("widgets.json"), true);
        QVERIFY(editor.setGameOptions(true, true));
        QVERIFY(editor.setGameProfile("Galleon-Fixture.exe", "Test Game", image));
        QVERIFY(!QFile::exists(dir.filePath("widgets.json")));
        QVERIFY(editor.apply());
        const HomeWidgets stored(nullptr, dir.filePath("widgets.json"));
        QVERIFY(stored.gameConfig().value("localSensors").toBool());
        GameMode mode;
        FpsSample sample{144.0, {}}; sample.process = "galleon-fixture.exe"; sample.token = "1";
        sample.cpuTemperature = 54; sample.gpuTemperature = 62;
        sample.cpuLoad = 37; sample.gpuLoad = 91;
        const auto state = mode.update(sample, stored.gameConfig());
        QCOMPARE(state.value("title").toString(), QStringLiteral("Test Game"));
        QVERIFY(!state.value("image").toString().isEmpty());
        ScreenRenderer renderer(QStringLiteral(GALLEON_SOURCE_WIDGETS "/WallpaperClock.qml"));
        const auto frame = renderer.render({{"gameState", state}, {"live", false}}, QSize(720, 384));
        QVERIFY(!frame.isNull());
        int cpuArc = 0;
        int gpuArc = 0;
        for (int y = 134; y < 361; ++y) {
            for (int x = 252; x < 461; ++x) {
                const auto color = frame.pixelColor(x, y);
                if (color.red() < 160 && color.green() > 160 && color.blue() > 130) ++cpuArc;
            }
            for (int x = 482; x < 691; ++x) {
                const auto color = frame.pixelColor(x, y);
                if (color.red() > 200 && color.green() < 160 && color.blue() < 160) ++gpuArc;
            }
        }
        QVERIFY(cpuArc > 30); // dial arcs must be present in offscreen software rendering
        QVERIFY(gpuArc > 30);
        const auto output = qEnvironmentVariable("GALLEON_GAME_SCREENSHOT");
        if (!output.isEmpty()) QVERIFY(frame.save(output));
        const auto home = renderer.render({{"gameState", QVariantMap()}}, QSize(720, 384));
        QVERIFY(home != frame);
    }
    void loadPercentagesValidateAndFallBackIndependently() {
        GameMode mode;
        FpsSample sample{144.0, {}}; sample.process = "galleon-fixture.exe"; sample.token = "load1";
        auto state = mode.update(sample, {});
        QVERIFY(!state.contains("cpuLoad"));
        QVERIFY(!state.contains("gpuLoad"));
        sample.cpuLoad = 0; sample.gpuLoad = 100;
        state = mode.update(sample, {});
        QCOMPARE(state.value("cpuLoad").toDouble(), 0.0);
        QCOMPARE(state.value("gpuLoad").toDouble(), 100.0);
        sample.cpuLoad = -1; sample.gpuLoad = 101;
        state = mode.update(sample, {}, {{"cpuLoad", 48.0}, {"gpuLoad", 72.0}});
        QCOMPARE(state.value("cpuLoad").toDouble(), 48.0);
        QCOMPARE(state.value("gpuLoad").toDouble(), 72.0);
        state = mode.update(sample, {}, {{"cpuLoad", "bad"}, {"gpuLoad", 200.0}});
        QVERIFY(!state.contains("cpuLoad"));
        QVERIFY(!state.contains("gpuLoad"));
    }
    void localMonitorSelectsTotalLoadNotMemoryOrCores() {
        const auto values = GameMode::parseSensors(R"({"Children":[
          {"SensorId":"/intelcpu/0/load/0","Value":"43.5 %"},
          {"SensorId":"/intelcpu/0/load/1","Value":"99.0 %"},
          {"SensorId":"/gpu-nvidia/0/load/0","Value":"0.0 %"},
          {"SensorId":"/gpu-nvidia/0/load/1","Value":"88.0 %"}]})");
        QCOMPARE(values.value("cpuLoad").toDouble(), 43.5);
        QVERIFY(values.contains("gpuLoad"));
        QCOMPARE(values.value("gpuLoad").toDouble(), 0.0);
    }
    void sensorJsonIsBoundedAndValidated() {
        const auto data = QByteArray(R"({"Children":[{"SensorId":"/intelcpu/0/temperature/0","Value":"55.0 °C"},{"SensorId":"/gpu-nvidia/0/temperature/0","Value":"140.0 °F"},{"SensorId":"/nvme/0/temperature/0","Value":"90 °C"}]})");
        const auto values = GameMode::parseSensors(data);
        QCOMPARE(values.value("cpu").toDouble(), 55.0);
        QCOMPARE(values.value("gpu").toDouble(), 60.0);
        QVERIFY(GameMode::parseSensors("{}").isEmpty());
        QVERIFY(GameMode::parseSensors(QByteArray(1024 * 1024 + 1, ' ')).isEmpty());
    }
    void missingMangoHudGpuTelemetryUsesNativeLoad() {
        const QTemporaryDir dir;
        const auto path = dir.filePath("cs2.csv");
        const auto write = [&](const QByteArray &row) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write("fps,cpu_load,gpu_load,gpu_temp,gpu_core_clock,gpu_vram_used,elapsed\n" + row) > 0;
        };
        QVERIFY(write("127,22,0,0,0,0,1000\n"));
        auto sample = readFpsLog(path);
        QVERIFY(sample.fps);
        QVERIFY(!sample.gpuLoad); // CS2 capture with GPU name but no actual GPU telemetry
        GameMode mode;
        auto state = mode.update(sample, {}, {{"gpuLoad", 66.0}, {"gpu", 55.0}});
        QCOMPARE(state.value("gpuLoad").toDouble(), 66.0);
        QVERIFY(state.value("gpu").toString().contains("55"));
        QVERIFY(write("127,22,0,40,210,1,1500\n"));
        sample = readFpsLog(path);
        QVERIFY(sample.gpuLoad); // a real idle GPU remains a valid 0%, not missing
        QCOMPARE(sample.gpuLoad.value_or(-1.0), 0.0);
        state = mode.update(sample, {}, {{"gpuLoad", 66.0}});
        QCOMPARE(state.value("gpuLoad").toDouble(), 0.0);
    }
    void amdFallbackReadsTheHwmonLayout() {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath("hwmon3/device"));
        const auto write = [&](const QString &name, const QByteArray &data) {
            QFile file(dir.filePath("hwmon3/" + name));
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write("name", "amdgpu\n"));
        QVERIFY(write("temp1_input", "55000\n"));
        QVERIFY(write("device/gpu_busy_percent", "73\n"));
        const auto values = GameMode::readAmdGpuSensors(dir.path());
        QCOMPARE(values.value("gpu").toDouble(), 55.0);
        QCOMPARE(values.value("gpuLoad").toDouble(), 73.0);
        QVERIFY(write("device/gpu_busy_percent", "101\n"));
        QVERIFY(!GameMode::readAmdGpuSensors(dir.path()).contains("gpuLoad"));
    }
    void csvCarriesGameAndTemperatures() {
        const QTemporaryDir dir;
        QFile f(dir.filePath("fixture_2026-10-23_12-00-00.csv"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("fps,cpu_temp,gpu_temp,cpu_load,gpu_load,elapsed\n144,54,62,37,91,1000\n"); f.close();
        const auto sample = readFpsLog(dir.path());
        QCOMPARE(sample.process, QStringLiteral("fixture"));
        QCOMPARE(sample.cpuLoad.value_or(-1), 37.0);
        QCOMPARE(sample.gpuLoad.value_or(-1), 91.0);
        QCOMPARE(sample.cpuTemperature.value_or(-1), 54.0);
        QCOMPARE(sample.gpuTemperature.value_or(-1), 62.0);
        const auto stale = readFpsLog(dir.path(), {}, QDateTime::currentDateTime().addSecs(11));
        GameMode mode;
        QVERIFY(!mode.update(stale, {}).value("active").toBool());
    }
};
QTEST_MAIN(TestGameMode)
#include "test_gamemode.moc"
