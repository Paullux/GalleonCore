// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BosunBackend.h"
#include "LanguageSettings.h"
#include "DaemonStatus.h"
#include "WallpaperLibrary.h"
#include "Required.h"

#include <AppsBackend.h>
#include <BrightnessCurve.h>

#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QPointer>
#include <QScopedPointer>
#include <QQuickItem>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Two themes and two pages: the first has an application, a media key and a page key.
QByteArray twoThemes()
{
    return R"({
  "version": 1,
  "home": "main",
  "themes": [
    {"name": "Control", "page": "main", "color": "#ffd600"},
    {"name": "Music", "page": "music", "color": "#e53935"}
  ],
  "pages": {
    "main": {"keys": [
      {"key": 0, "label": "Play", "action": {"type": "media", "command": "play-pause"}},
      {"key": 1, "action": {"type": "app", "desktop": "nothing-like-this.desktop"}},
      {"key": 2, "label": "More", "action": {"type": "page", "page": "music"}}
    ]},
    "music": {"keys": [
      {"key": 11, "label": "Back", "action": {"type": "back"}}
    ], "steam": {"max": 4}}
  }
})";
}

// A program that prints what ffprobe would, in place of ffprobe.
QString fakeProbe(const QString &dir, const QByteArray &json)
{
    QString script = dir + QStringLiteral("/fake-ffprobe");
    QFile file(script);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write("#!/bin/sh\ncat <<'EOF'\n" + json + "\nEOF\n");
    file.close();
    QFile::setPermissions(script, QFile::permissions(script) | QFile::ExeUser);
    return script;
}

// A program in place of FFmpeg: lists an H.264 encoder, and for a conversion runs `body`
// (the last argument is the file to write; its arguments are kept in `<file>.args`).
QString fakeFfmpeg(const QString &dir, const QString &body)
{
    QString script = dir + QStringLiteral("/fake-ffmpeg");
    QFile file(script);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(("#!/bin/sh\ncase \"$*\" in\n  *-encoders*) printf ' V....D libx264 libx264 H.264\\n'; exit 0;;\nesac\n"
                "for last; do :; done\nfor a in \"$@\"; do echo \"$a\" >> \"$last.args\"; done\n" + body + "\n").toUtf8());
    file.close();
    QFile::setPermissions(script, QFile::permissions(script) | QFile::ExeUser);
    return script;
}

QByteArray wideProbe()
{
    return R"({"streams": [{"codec_name": "h264", "width": 1920, "height": 1080, "avg_frame_rate": "24/1"}], "format": {"duration": "12"}})";
}

QByteArray saverPages(const QString &videos, const QString &geometry = QStringLiteral(
                                                 R"("geometry": {"screen_mm": [62.6, 33.0], "key_mm": [13.7, 13.7], "gap_mm": [5.4, 5.4], "screen_to_keys_mm": 5.4},)"))
{
    return (QStringLiteral(R"({"version": 1, "home": "main", "pages": {"main": {"keys": []}}, "screensaver": {)") + geometry
            + QStringLiteral(R"("idle_minutes": 3, "fps": 12, "while_playing": true, "video": [)") + videos + QStringLiteral("]}}"))
        .toUtf8();
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(bytes) == bytes.size();
}

class FakeFilePicker final : public QObject
{
    Q_OBJECT
public:
    QString picturePath;
    QString videoPath;
    Q_INVOKABLE QString picture() { return picturePath; }
    Q_INVOKABLE QString video() { return videoPath; }
    QString themePath;
    QString themeSavePath;
    Q_INVOKABLE QString theme() { return themePath; }
    Q_INVOKABLE QString themeSave(const QString &/*name*/) { return themeSavePath; }
    QStringList wallpaperPaths;
    Q_INVOKABLE QStringList pictures() { return wallpaperPaths; }
};

// The items called `name` among the visual descendants of `item`: the items a Repeater makes are not QObject children of the
// window, only visual ones.
void collectNamed(QQuickItem *item, const QString &name, QList<QQuickItem *> &found)
{
    if (!item)
        return;
    if (item->objectName() == name)
        found.append(item);
    for (QQuickItem *child : item->childItems())
        collectNamed(child, name, found);
}

QList<QQuickItem *> namedItems(QObject *root, const QString &name)
{
    QList<QQuickItem *> found;
    if (auto *window = qobject_cast<QQuickWindow *>(root))
        collectNamed(window->contentItem(), name, found);
    return found;
}

// What galleond's keyboard object is, as far as Bosun can tell: it is asked to read the brightness, and says nothing here.
class KeyboardEndpoint : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.Keyboard")
public:
    int reads = 0;
public Q_SLOTS:
    void ReadBrightness() { ++reads; }
};

// The lighting object of galleond, as far as Bosun can tell: it says that it holds the keyboard, and notes the colours it is given.
class LightingEndpoint : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.Lighting")
public:
    QList<QList<int>> colours;
public Q_SLOTS:
    [[nodiscard]] QString State() const { return QStringLiteral("holding"); }
    QString Enable(bool /*on*/) { return {}; }
    bool SetColor(int red, int green, int blue) { colours.append({red, green, blue}); return true; }
Q_SIGNALS:
    void stateChanged(const QString &_t1, const QString &_t2); // NOLINT(readability-inconsistent-declaration-parameter-name): moc names them
};

class TestBosun : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fpsDialogGroupsSelectionsAndKeepsApplyVisible() {
        // An in-memory model: this UI test never scans or edits real launchers.
        QQmlEngine engine;
        QQmlComponent fakeComponent(&engine);
        fakeComponent.setData(R"(import QtQml
QtObject {
    property var rows: [
        {id: "common", group: "Commun", label: "Configuration MangoHud et dossier CSV", enabled: true, note: ""},
        {id: "one", group: "Steam · natif", label: "Jeu 1", enabled: true, note: "mangohud %command%"},
        {id: "two", group: "Steam · natif", label: "Jeu 2", enabled: true, note: "mangohud %command%"},
        {id: "future", group: "Heroic · Flatpak", label: "Paramètres généraux · futurs jeux", enabled: true, note: ""},
        {id: "installed", group: "Heroic · Flatpak", label: "Jeu déjà installé", enabled: true, note: ""},
        {id: "unsupported", group: "Lutris · natif", label: "Format non reconnu", enabled: false, note: "Action manuelle nécessaire"}
    ]
    property var results: []
    property bool busy: false
    property string error: ""
    property var applied: []
    signal changed()
    function scan() { changed() }
    function apply(ids) { applied = ids; busy = true; changed() }
    function restore(token) { }
})", QUrl());
        const std::unique_ptr<QObject> fake(fakeComponent.create());
        QVERIFY2(fake, qPrintable(fakeComponent.errorString()));
        // BosunBackend initializes the embedded UI resource.
        const QTemporaryDir folder;
        const bosun::BosunBackend backend(folder.filePath("pages.json"));
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/bosun/qml/FpsSetupDialog.qml")));
        const std::unique_ptr<QObject> dialog(component.createWithInitialProperties({{"setup", QVariant::fromValue(fake.get())}}));
        QVERIFY2(dialog, qPrintable(component.errorString()));
        bosun::LanguageSettings languages({});
        languages.setPreference("de");
        QVariant label;
        const QVariantMap steamRow{{"kind", "steam-shortcut"}, {"label", "Mon jeu %1 · hors boutique · compte 42"}};
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "rowLabel", Q_RETURN_ARG(QVariant, label), Q_ARG(QVariant, steamRow)));
        QCOMPARE(label.toString(), QStringLiteral("Mon jeu %1 · ohne Shop · Konto 42"));
        const QVariantMap bottleRow{{"kind", "bottles"}, {"label", "Ma bouteille · toute la bouteille"}};
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "rowLabel", Q_RETURN_ARG(QVariant, label), Q_ARG(QVariant, bottleRow)));
        QCOMPARE(label.toString(), QStringLiteral("Ma bouteille · ganze Flasche"));
        languages.setPreference("en");
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "selectAll", Q_ARG(QVariant, true)));
        QCOMPARE(dialog->property("selectionCount").toInt(), 5);
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "toggleGroup", Q_ARG(QVariant, QStringLiteral("Steam · natif"))));
        QCOMPARE(dialog->property("selectionCount").toInt(), 3);
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "toggleGroup", Q_ARG(QVariant, QStringLiteral("Steam · natif"))));
        QCOMPARE(dialog->property("selectionCount").toInt(), 5);
        auto *window = qobject_cast<QQuickWindow *>(dialog.get());
        QVERIFY(window);
        window->show();
        QTest::qWait(80);
        auto *apply = dialog->findChild<QQuickItem *>("fpsApplySelection");
        QVERIFY(apply);
        const QPointF position = apply->mapToScene(QPointF(0, 0));
        QVERIFY(position.y() >= 0);
        QVERIFY(position.y() + apply->height() <= window->height());
        QVERIFY(!window->grabWindow().isNull());
        const auto screenshot = qEnvironmentVariable("GALLEON_FPS_SETUP_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(window->grabWindow().save(screenshot));
        QVERIFY(QMetaObject::invokeMethod(apply, "clicked"));
        QCOMPARE(fake->property("applied").toList().size(), 5);
        QVERIFY(!apply->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(dialog.get(), "selectAll", Q_ARG(QVariant, false)));
        QCOMPARE(dialog->property("selectionCount").toInt(), 0);
    }
    void gamingArtworkGuideLoopsAndStopsWhenHidden() {
        const QTemporaryDir dir;
        galleond::HomeWidgets widgets(nullptr, dir.filePath("widgets.json"), true);
        QVERIFY(widgets.setGameOptions(false, false));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/bosun/qml/GameModePage.qml")));
        const std::unique_ptr<QObject> page(component.createWithInitialProperties({{"widgets", QVariant::fromValue(&widgets)}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        QQuickWindow window;
        auto *item = qobject_cast<QQuickItem *>(page.get());
        QVERIFY(item);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(900, 700));
        window.resize(900, 700);
        window.show();
        auto *guide = page->findChild<QObject *>("gamingArtworkGuide");
        auto *timer = page->findChild<QObject *>("gamingArtworkGuideTimer");
        QVERIFY(guide);
        QVERIFY(timer);
        QTRY_VERIFY(timer->property("running").toBool());
        QCOMPARE(guide->property("instruction").toString(), QStringLiteral("Appuyez sur la molette de droite"));
        for (int phase = 1; phase <= 13; ++phase) {
            QVERIFY(QMetaObject::invokeMethod(timer, "triggered"));
            QCOMPARE(guide->property("phase").toInt(), phase % 13);
            if (phase == 2) QCOMPARE(guide->property("imageIndex").toInt(), 1);
            if (phase == 4) QCOMPARE(guide->property("imageIndex").toInt(), 2);
            if (phase >= 6 && phase <= 9) QCOMPARE(guide->property("secondsLeft").toInt(), 10 - phase);
            if (phase == 10) QCOMPARE(guide->property("instruction").toString(), QStringLiteral("Image sauvée"));
        }
        QVERIFY(page->setProperty("visible", false));
        QVERIFY(!timer->property("running").toBool());
        QCOMPARE(guide->property("phase").toInt(), 0);
        QVERIFY(page->setProperty("visible", true));
        QTRY_VERIFY(timer->property("running").toBool());
        window.hide();
        QTRY_VERIFY(!timer->property("running").toBool());
    }
    void widgetApplyButtonPublishesTheDraft() {
        const QTemporaryDir dir;
        const auto file = dir.filePath("widgets.json");
        galleond::HomeWidgets widgets(nullptr, file, true);
        QVERIFY(widgets.setCity("Lyon"));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/bosun/qml/WidgetsPage.qml")));
        const std::unique_ptr<QObject> page(component.createWithInitialProperties({{"widgets", QVariant::fromValue(&widgets)}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        auto *apply = page->findChild<QObject *>("applyWidgets");
        QVERIFY(apply);
        QVERIFY(!QFile::exists(file));
        QVERIFY(QMetaObject::invokeMethod(apply, "clicked"));
        QVERIFY(QFile::exists(file));
        QVERIFY(!widgets.pending());
        const galleond::HomeWidgets saved(nullptr, file);
        QCOMPARE(saved.values().value("weatherCity").toString(), QStringLiteral("Lyon"));
    }
    void captureButtonTracksConfirmedStateWithoutProcessFlag() {
        QQmlPropertyMap widgets;
        widgets.insert("windowsCapture", true);
        widgets.insert("pending", false);
        widgets.insert("error", QString());
        widgets.insert("fpsConfig", QVariantMap{});
        widgets.insert("gameConfig", QVariantMap{});
        widgets.insert("gameState", QVariantMap{});
        widgets.insert("recommendedFpsFolder", QStringLiteral("/tmp/fps"));
        widgets.insert("steamLaunchOptions", QString());
        widgets.insert("captureStatus", QVariantMap{{"installed", true}, {"state", "waiting"}});
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/bosun/qml/GameModePage.qml")));
        const std::unique_ptr<QObject> page(component.createWithInitialProperties({{"widgets", QVariant::fromValue(static_cast<QObject *>(&widgets))}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        auto *button = page->findChild<QObject *>("startPresentMon");
        QVERIFY(button);
        QCOMPARE(button->property("text").toString(), QStringLiteral("Prêt — lancez un jeu"));
        QVERIFY(!button->property("enabledLook").toBool());
        widgets.insert("captureStatus", QVariantMap{{"installed", true}, {"state", "capturing"}, {"running", false}});
        QCOMPARE(button->property("text").toString(), QStringLiteral("Arrêter la capture FPS"));
        QVERIFY(button->property("selected").toBool());
        widgets.insert("captureStatus", QVariantMap{{"installed", true}, {"state", "starting"}, {"starting", true}});
        QCOMPARE(button->property("text").toString(), QStringLiteral("Démarrage de la capture FPS…"));
        QVERIFY(!button->property("enabledLook").toBool());
        widgets.insert("captureStatus", QVariantMap{{"installed", true}, {"state", "error"}, {"message", "API inaccessible"}});
        QCOMPARE(button->property("text").toString(), QStringLiteral("Démarrer la capture FPS"));
        QVERIFY(!button->property("selected").toBool());
        QVERIFY(button->property("enabledLook").toBool());
        widgets.insert("captureStatus", QVariantMap{{"state", "offline"}});
        QVERIFY(!button->property("enabledLook").toBool());
        // IPC messages remain in the daemon's language; translate fixed UI text locally.
        const QString message = QStringLiteral("Ready. Start a supported game; capture starts automatically.");
        widgets.insert("captureStatus", QVariantMap{{"installed", true}, {"state", "waiting"}, {"message", message}});
        auto *status = page->findChild<QObject *>("gameCaptureStatus");
        QVERIFY(status);
        bosun::LanguageSettings languages({});
        connect(&languages, &bosun::LanguageSettings::changed, &engine, &QQmlEngine::retranslate);
        languages.setPreference("fr");
        QCOMPARE(status->property("text").toString(), QStringLiteral("Prêt. Lancez un jeu pris en charge ; la capture démarre automatiquement."));
        languages.setPreference("en");
        QCOMPARE(status->property("text").toString(), message);
        QCOMPARE(widgets.value("captureStatus").toMap().value("message").toString(), message);
    }
    void gameTabAppliesItsOwnFields() {
        const QTemporaryDir dir;
        const auto file = dir.filePath("widgets.json");
        galleond::HomeWidgets widgets(nullptr, file, true);
        QVERIFY(widgets.setGameOptions(false, false));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/bosun/qml/GameModePage.qml")));
        const std::unique_ptr<QObject> page(component.createWithInitialProperties({{"widgets", QVariant::fromValue(&widgets)}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        const auto setField = [&](const char *name, const QString &value) {
            auto *field = page->findChild<QObject *>(name);
            return field && field->setProperty("value", value);
        };
        QVERIFY(setField("gameFpsSource", dir.path()));
        QVERIFY(setField("gameProfileProcess", "cs2"));
        QVERIFY(setField("gameProfileTitle", "Counter-Strike 2"));
        QVERIFY(setField("gameProfileImage", dir.filePath("art.png")));
        QVERIFY(!QFile::exists(file));
        auto *apply = page->findChild<QObject *>("applyGameMode");
        QVERIFY(apply);
        QVERIFY(QMetaObject::invokeMethod(apply, "clicked"));
        QVERIFY(!widgets.pending());
        const galleond::HomeWidgets saved(nullptr, file);
        QCOMPARE(saved.fpsConfig().value("source").toString(), dir.path());
        QVERIFY(!saved.gameConfig().value("automatic").toBool());
        QCOMPARE(saved.gameConfig().value("profiles").toMap().value("cs2").toMap().value("title").toString(), QStringLiteral("Counter-Strike 2"));
        QVERIFY(!page->findChild<QObject *>("applyWidgets"));
    }
    void gameArtworkApplyNearImageSavesTheSameDraftAsTopApply() {
        const QTemporaryDir dir;
        const QString file = dir.filePath("widgets.json");
        const QString picture = dir.filePath("neurohell.png");
        QImage image(720, 384, QImage::Format_RGB32);
        image.fill(Qt::darkRed);
        QVERIFY(image.save(picture));
        // QML invokes the non-const setGameProfile()/apply() methods through QObject.
        galleond::HomeWidgets widgets(nullptr, file, true); // NOLINT(misc-const-correctness)
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/bosun/qml/GameModePage.qml")));
        const std::unique_ptr<QObject> page(component.createWithInitialProperties({{"widgets", QVariant::fromValue(&widgets)}}));
        QVERIFY2(page, qPrintable(component.errorString()));
        const auto setField = [&](const char *name, const QString &value) {
            auto *field = page->findChild<QObject *>(name);
            return field && field->setProperty("value", value);
        };
        QVERIFY(setField("gameProfileProcess", "neurohell.exe"));
        QVERIFY(setField("gameProfileTitle", "NeuroHell"));
        QVERIFY(setField("gameProfileImage", picture));
        QVERIFY(!QFile::exists(file));
        auto *top = page->findChild<QObject *>("applyGameMode");
        auto *nearArtwork = page->findChild<QObject *>("applyGameArtwork");
        QVERIFY(top);
        QVERIFY(nearArtwork);
        QVERIFY(QMetaObject::invokeMethod(nearArtwork, "clicked"));
        QVERIFY(!widgets.pending());
        QVERIFY(QFile::exists(file));
        const galleond::HomeWidgets saved(nullptr, file);
        const auto profile = saved.gameConfig().value("profiles").toMap().value("neurohell.exe").toMap();
        QCOMPARE(profile.value("title").toString(), QStringLiteral("NeuroHell"));
        QCOMPARE(profile.value("image").toString(), picture);
    }
    void browseComesFirstAndImportsOnlyWhenAFileIsChosen()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const QString picture = dir.filePath("my picture.png");
        QImage image(720, 384, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QVERIFY(image.save(picture));
        const QString video = dir.filePath("my video.mp4");
        QVERIFY(writeFile(video, "fake video"));
        const auto backend = std::make_unique<bosun::BosunBackend>(file, bosun::BosunBackend::Tools{.ffprobe = fakeProbe(dir.path(), wideProbe()),
                                         .thumbnailFolder = dir.filePath("thumbs")});
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const auto library = std::make_unique<bosun::WallpaperLibrary>(bosun::WallpaperLibrary::Places{.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        FakeFilePicker picker;
        picker.picturePath = picture;
        picker.videoPath = video;
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(backend.get()));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(library.get()));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(library.get()));
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(backend.get())},
                                     {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(library.get())},
                                     {QStringLiteral("filePicker"), QVariant::fromValue(&picker)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().constFirst();
        root->setProperty("currentTab", root->property("tabs").toList().size() - 4);
        auto *pictureButton = root->findChild<QQuickItem *>("browseWallpaper");
        auto *videoButton = root->findChild<QQuickItem *>("browseScreensaver");
        QVERIFY(pictureButton);
        QVERIFY(videoButton);
        const auto *pictureField = root->findChild<QQuickItem *>("wallpaperPath");
        const auto *videoField = root->findChild<QQuickItem *>("screensaverPath");
        QVERIFY(pictureField);
        QVERIFY(videoField);
        QTRY_VERIFY(pictureButton->x() < pictureField->x());
        QTRY_VERIFY(videoButton->x() < videoField->x());
        QVERIFY(QMetaObject::invokeMethod(pictureButton, "clicked"));
        QCOMPARE(library->candidate().value("chosen").toString(), picture);
        QCOMPARE(library->candidate().value("state").toString(), QStringLiteral("ready"));
        const auto *crop = root->findChild<QQuickItem *>("wallpaperCrop");
        QVERIFY(crop);
        QTRY_VERIFY(crop->isVisible());
        picker.picturePath.clear(); // cancel preserves the existing preview
        QVERIFY(QMetaObject::invokeMethod(pictureButton, "clicked"));
        QCOMPARE(library->candidate().value("chosen").toString(), picture);
        root->setProperty("currentTab", root->property("tabs").toList().size() - 1);
        QVERIFY(QMetaObject::invokeMethod(videoButton, "clicked"));
        QCOMPARE(backend->candidate().value("path").toString(), video);
        picker.videoPath.clear();
        QVERIFY(QMetaObject::invokeMethod(videoButton, "clicked"));
        QCOMPARE(backend->candidate().value("path").toString(), video);
    }

    void aThemeFileChosenInTheNewTabIsInstalledAndShown()
    {
        const QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.filePath("source")));
        QVERIFY(QDir().mkpath(dir.filePath("target")));
        const QString source = dir.filePath("source/pages.json");
        QVERIFY(writeFile(source, R"({"version": 1, "home": "mine",
            "themes": [{"name": "Mine", "page": "mine", "color": "#336699"}],
            "pages": {"mine": {"keys": [{"key": 0, "label": "Next", "action": {"type": "media", "command": "next"}}]}}})"));
        bosun::BosunBackend from(source, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        const QVariantMap exported = from.exportTheme(0, dir.filePath("mine"), {{"author", "A. Person"}, {"license", "CC-BY-SA-4.0"}});
        QVERIFY(exported.value("ok").toBool());

        const QString target = dir.filePath("target/pages.json");
        QVERIFY(writeFile(target, twoThemes()));
        const auto backend = std::make_unique<bosun::BosunBackend>(target, bosun::BosunBackend::Tools{.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent",
            .thumbnailFolder = dir.filePath("thumbs"), .wallpaperRoot = dir.filePath("wallpapers")});
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const auto library = std::make_unique<bosun::WallpaperLibrary>(bosun::WallpaperLibrary::Places{.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        FakeFilePicker picker;
        picker.themePath = exported.value("file").toString();
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(backend.get()));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(library.get()));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(library.get()));
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(backend.get())},
                                     {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(library.get())},
                                     {QStringLiteral("filePicker"), QVariant::fromValue(&picker)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().constFirst();
        root->setProperty("currentTab", backend->themes().size()); // the "+" tab
        auto *button = root->findChild<QQuickItem *>("importTheme");
        QVERIFY(button);
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QCOMPARE(backend->themes().size(), 3);
        QCOMPARE(root->property("currentTab").toInt(), 2); // the window shows the new tab
        QVERIFY(!root->property("messageIsError").toBool());

        picker.themePath = dir.filePath("none.gctheme"); // a file that is not there: said, nothing added
        root->setProperty("currentTab", backend->themes().size());
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QCOMPARE(backend->themes().size(), 3);
        QVERIFY(root->property("messageIsError").toBool());

        picker.themePath.clear(); // cancelled: nothing happens
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QCOMPARE(backend->themes().size(), 3);
    }

    void aTabIsExportedFromItsEditor()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const auto backend = std::make_unique<bosun::BosunBackend>(file, bosun::BosunBackend::Tools{.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent",
                                                                                                .thumbnailFolder = dir.filePath("thumbs")});
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const auto library = std::make_unique<bosun::WallpaperLibrary>(bosun::WallpaperLibrary::Places{.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        FakeFilePicker picker;
        picker.themeSavePath = dir.filePath("music");
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(backend.get()));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(library.get()));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(library.get()));
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(backend.get())},
                                     {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(library.get())},
                                     {QStringLiteral("filePicker"), QVariant::fromValue(&picker)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().constFirst();
        root->setProperty("currentTab", 1);
        auto *editor = root->findChild<QQuickItem *>("tabEditor");
        auto *button = root->findChild<QQuickItem *>("exportTheme");
        QVERIFY(editor);
        QVERIFY(button);
        QVERIFY(!button->property("enabledLook").toBool()); // no author yet
        editor->setProperty("author", "A. Person");
        QVERIFY(button->property("enabledLook").toBool());
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QVERIFY2(QFile::exists(dir.filePath("music.gctheme")), "the file is made with its extension");
        QVERIFY(!root->property("messageIsError").toBool());
    }

    void wallpapersChosenInTheEditorGoIntoTheExportedTheme()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        QImage wallpaper(1200, 700, QImage::Format_RGB32);
        wallpaper.fill(Qt::darkMagenta);
        const QString picture = dir.filePath("bg.png");
        QVERIFY(wallpaper.save(picture));
        const auto backend = std::make_unique<bosun::BosunBackend>(file, bosun::BosunBackend::Tools{.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent",
                                                                                                .thumbnailFolder = dir.filePath("thumbs")});
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const auto library = std::make_unique<bosun::WallpaperLibrary>(bosun::WallpaperLibrary::Places{.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        FakeFilePicker picker;
        picker.themeSavePath = dir.filePath("music");
        picker.wallpaperPaths = {picture, picture, dir.filePath("bg.png")}; // the same file three times: kept once
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(backend.get()));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(library.get()));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(library.get()));
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(backend.get())},
                                     {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(library.get())},
                                     {QStringLiteral("filePicker"), QVariant::fromValue(&picker)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().constFirst();
        root->setProperty("currentTab", 1);
        auto *editor = root->findChild<QQuickItem *>("tabEditor");
        auto *add = root->findChild<QQuickItem *>("addThemeWallpapers");
        auto *button = root->findChild<QQuickItem *>("exportTheme");
        QVERIFY(editor);
        QVERIFY(add);
        QVERIFY(button);
        editor->setProperty("author", "A. Person");
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QCOMPARE(editor->property("wallpapers").toList().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QVERIFY2(QFile::exists(dir.filePath("music.gctheme")), "the theme is exported");
        QFile exported(dir.filePath("music.gctheme"));
        QVERIFY(exported.open(QIODevice::ReadOnly));
        const QByteArray bytes = exported.readAll();
        QVERIFY(bytes.contains("wallpapers/wallpaper-01.jpg")); // stored entries: the name is readable in the file
        root->setProperty("currentTab", 0); // another tab: the list is forgotten
        QCOMPARE(editor->property("wallpapers").toList().size(), 0);
    }

    void aTabCanHaveItsOwnWallpaperAndGiveItBack()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        QImage picture(1800, 900, QImage::Format_RGB32);
        picture.fill(Qt::darkCyan);
        const QString source = dir.filePath("my picture.png");
        QVERIFY(picture.save(source));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs"),
                                           .wallpaperRoot = dir.filePath("wallpapers")});
        QVERIFY(backend.themes().at(1).toMap().value("wallpaper").toString().isEmpty());

        QCOMPARE(backend.setThemeWallpaper(1, source), QString());
        QCOMPARE(backend.themes().at(1).toMap().value("wallpaper").toString(), QStringLiteral("music/music-tab.jpg"));
        QVERIFY(backend.themes().at(0).toMap().value("wallpaper").toString().isEmpty()); // the other tab keeps the user's own
        const QImage saved(dir.filePath("wallpapers/music/music-tab.jpg"));
        QVERIFY(!saved.isNull());
        QVERIFY(saved.width() <= 720 && saved.height() <= 384); // shrunk to the screen

        QCOMPARE(backend.clearThemeWallpaper(1), QString());
        QVERIFY(backend.themes().at(1).toMap().value("wallpaper").toString().isEmpty());
    }

    void somethingThatIsNotAPictureIsNotMadeAWallpaper()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const QString notAPicture = dir.filePath("notes.txt");
        QVERIFY(writeFile(notAPicture, "just text"));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs"),
                                           .wallpaperRoot = dir.filePath("wallpapers")});
        QVERIFY(!backend.setThemeWallpaper(1, notAPicture).isEmpty());
        QVERIFY(!backend.setThemeWallpaper(1, dir.filePath("missing.png")).isEmpty());
        QVERIFY(!backend.setThemeWallpaper(9, notAPicture).isEmpty()); // no such tab
        QVERIFY(backend.themes().at(1).toMap().value("wallpaper").toString().isEmpty());
        QVERIFY(!QDir(dir.filePath("wallpapers/music")).exists());
    }

    void thePictureChosenInTheEditorBecomesTheWallpaperOfTheTab()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        QImage picture(720, 384, QImage::Format_RGB32);
        picture.fill(Qt::darkGreen);
        const QString source = dir.filePath("bg.png");
        QVERIFY(picture.save(source));
        const auto backend = std::make_unique<bosun::BosunBackend>(file, bosun::BosunBackend::Tools{.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent",
                                                                                                .thumbnailFolder = dir.filePath("thumbs"),
                                                                                                .wallpaperRoot = dir.filePath("wallpapers")});
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const auto library = std::make_unique<bosun::WallpaperLibrary>(bosun::WallpaperLibrary::Places{.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        FakeFilePicker picker;
        picker.picturePath = source;
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(backend.get()));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(library.get()));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(library.get()));
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(backend.get())},
                                     {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(library.get())},
                                     {QStringLiteral("filePicker"), QVariant::fromValue(&picker)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *root = engine.rootObjects().constFirst();
        root->setProperty("currentTab", 1);
        auto *choose = root->findChild<QQuickItem *>("chooseTabWallpaper");
        auto *clear = root->findChild<QQuickItem *>("clearTabWallpaper");
        QVERIFY(choose);
        QVERIFY(clear);
        QVERIFY(!clear->property("enabledLook").toBool()); // nothing to give back yet
        QVERIFY(QMetaObject::invokeMethod(choose, "clicked"));
        QCOMPARE(backend->themes().at(1).toMap().value("wallpaper").toString(), QStringLiteral("music/music-tab.jpg"));
        QVERIFY(!root->property("messageIsError").toBool());
        QTRY_VERIFY(clear->property("enabledLook").toBool());
        QVERIFY(QMetaObject::invokeMethod(clear, "clicked"));
        QVERIFY(backend->themes().at(1).toMap().value("wallpaper").toString().isEmpty());
        picker.picturePath.clear(); // cancelled: nothing happens
        QVERIFY(QMetaObject::invokeMethod(choose, "clicked"));
        QVERIFY(backend->themes().at(1).toMap().value("wallpaper").toString().isEmpty());
    }

    void themesAreTheTabs()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QVERIFY2(backend.problem().isEmpty(), qPrintable(backend.problem()));
        const QVariantList themes = backend.themes();
        QCOMPARE(themes.size(), 2);
        QCOMPARE(themes.at(0).toMap().value("name").toString(), QStringLiteral("Control"));
        QCOMPARE(themes.at(1).toMap().value("page").toString(), QStringLiteral("music"));
        QCOMPARE(themes.at(1).toMap().value("color").toString(), QStringLiteral("#e53935"));
    }

    void anApplicationKeyIsMissingItsApplicationOnlyWhileTheApplicationIsNotInstalled()
    {
        const QTemporaryDir dir;
        const QString data = dir.filePath("share");
        QVERIFY(QDir().mkpath(data + "/applications"));
        QVERIFY(writeFile(data + "/applications/present-tool.desktop",
                          "[Desktop Entry]\nType=Application\nName=Present Tool\nExec=present-tool\n"));
        qputenv("XDG_DATA_HOME", data.toUtf8());
        qputenv("XDG_DATA_DIRS", data.toUtf8());
        const auto restore = qScopeGuard([] {
            qunsetenv("XDG_DATA_HOME");
            qunsetenv("XDG_DATA_DIRS");
        });
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main", "pages": {"main": {"keys": [
            {"key": 0, "action": {"type": "app", "desktop": "present-tool.desktop"}},
            {"key": 1, "action": {"type": "app", "desktop": "absent-tool.desktop"}}]}}})"));
        const bosun::BosunBackend backend(file);
        const QVariantList keys = backend.keys("main");
        QVERIFY(!keys.at(0).toMap().value("appMissing").toBool());
        QVERIFY(keys.at(1).toMap().value("appMissing").toBool());
    }

    void openingTheKeyboardTabAsksGalleondToReadTheBrightnessAgain()
    {
        // galleond as another program: its own connection to the bus, with the name Bosun looks for and the keyboard object.
        const QString connectionName = QStringLiteral("fake-galleond-for-the-keyboard-tab");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        KeyboardEndpoint endpoint;
        QVERIFY(bus.registerObject("/org/galleoncore/Keyboard", &endpoint, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(bosun::DaemonStatus::BusName));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/Keyboard");
            bus.unregisterService(bosun::DaemonStatus::BusName);
            QDBusConnection::disconnectFromBus(connectionName);
        });

        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QTRY_VERIFY_WITH_TIMEOUT(daemon.running(), 3000);
        QTest::qWait(300); // the read made when the daemon appeared
        const int before = endpoint.reads;

        QVERIFY(root->setProperty("currentTab", 3)); // the keyboard tab: after the two themes and the "+" tab
        QTRY_COMPARE_WITH_TIMEOUT(endpoint.reads, before + 1, 3000);
        QVERIFY(root->setProperty("currentTab", 0)); // another tab asks for nothing
        QTest::qWait(300);
        QCOMPARE(endpoint.reads, before + 1);
        QVERIFY(root->setProperty("currentTab", 3)); // and every time the keyboard tab is opened
        QTRY_COMPARE_WITH_TIMEOUT(endpoint.reads, before + 2, 3000);
    }

    void aPageHasTwelveKeysAndSaysWhatTheyDo()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        const QVariantList keys = backend.keys("main");
        QCOMPARE(keys.size(), 12);
        const QVariantMap play = keys.at(0).toMap();
        QCOMPARE(play.value("kind").toString(), QStringLiteral("media"));
        QCOMPARE(play.value("label").toString(), QStringLiteral("Play"));
        QCOMPARE(play.value("detail").toString(), QStringLiteral("play / pause"));
        QVERIFY(!play.value("empty").toBool());
        // An application that is not installed is shown by the name of its file.
        const QVariantMap app = keys.at(1).toMap();
        QCOMPARE(app.value("kind").toString(), QStringLiteral("app"));
        QCOMPARE(app.value("label").toString(), QStringLiteral("nothing-like-this.desktop"));
        QVERIFY(app.value("appMissing").toBool()); // and the window says it is not installed
        QVERIFY(!play.value("appMissing").toBool()); // only an application key can miss its application
        QCOMPARE(keys.at(2).toMap().value("detail").toString(), QStringLiteral("music"));
        QVERIFY(keys.at(3).toMap().value("empty").toBool());
        QCOMPARE(keys.at(3).toMap().value("kind").toString(), QStringLiteral("empty"));
        QVERIFY(backend.keys("no-such-page").isEmpty());
    }

    void whatTheModuleAddsIsSaid()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QVERIFY(backend.autoFill("main").isEmpty());
        QVERIFY(backend.autoFill("music").contains("Steam"));
        QVERIFY(backend.autoFill("no-such-page").isEmpty());
    }

    void everyKeyHasItsPicture()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        for (const int key : {0, 1, 3, 11}) {
            const QPixmap tile = backend.tile("main", key);
            QCOMPARE(tile.size(), QSize(160, 160));
        }
        // A key with a label is not drawn like an empty one.
        QVERIFY(backend.tile("main", 0).toImage() != backend.tile("main", 3).toImage());
        // Nothing to show: an empty key, never a crash or a null picture.
        QCOMPARE(backend.tile("no-such-page", 0).size(), QSize(160, 160));
        QCOMPARE(backend.tile("main", 12).size(), QSize(160, 160));
        QCOMPARE(backend.tile("main", -1).size(), QSize(160, 160));
    }

    void theProviderReadsPageAndKeyFromTheUrl()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        bosun::TileProvider provider(&backend);
        QSize size;
        const QPixmap labelled = provider.requestPixmap("main/0?r=3", &size, {});
        QCOMPARE(size, QSize(160, 160));
        QVERIFY(labelled.toImage() == backend.tile("main", 0).toImage());
        // Bad identifiers give an empty key.
        for (const QString &id : {QString(), QStringLiteral("main"), QStringLiteral("main/x"), QStringLiteral("/3"), QStringLiteral("main/99")}) {
            const QPixmap blank = provider.requestPixmap(id, &size, {});
            QCOMPARE(size, QSize(160, 160));
            QVERIFY(blank.toImage() == backend.tile("main", 3).toImage() || blank.toImage() == backend.tile("no-such-page", 0).toImage());
        }
    }

    void aBrokenFileIsReportedAndShowsNothing()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main", "pages": {"main": {"keys": [{"key": 99}]}}})"));
        const bosun::BosunBackend backend(file);
        QVERIFY(!backend.problem().isEmpty());
        QVERIFY(backend.themes().isEmpty());
        QVERIFY(backend.keys("main").isEmpty());
        QCOMPARE(backend.tile("main", 0).size(), QSize(160, 160));
    }

    void aMissingFileShowsTheBuiltInPages()
    {
        const QTemporaryDir dir;
        const bosun::BosunBackend backend(dir.filePath("none.json"));
        QVERIFY2(backend.problem().isEmpty(), qPrintable(backend.problem()));
        QCOMPARE(backend.keys("main").size(), 12);
    }

    void aChangeOnDiskIsSeen()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        const int before = backend.revision();
        QSignalSpy changed(&backend, &bosun::BosunBackend::changed);
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main", "pages": {"main": {"keys": []}}})"));
        QVERIFY(changed.wait(3000));
        QVERIFY(backend.revision() > before);
        QVERIFY(backend.themes().isEmpty());
        // The file replaced as an editor does (new file, then renamed over): still watched.
        QVERIFY(writeFile(file + ".new", twoThemes()));
        QVERIFY(QFile::remove(file));
        QVERIFY(QFile::rename(file + ".new", file));
        changed.clear();
        QTRY_VERIFY_WITH_TIMEOUT(backend.themes().size() == 2, 3000);
    }

    void theScreensaverIsOffWithoutGeometryOrVideo()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend nothing(file, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        QVERIFY(!nothing.screensaver().value("enabled").toBool());
        QVERIFY(!nothing.screensaver().value("hasGeometry").toBool());
        QVERIFY(nothing.screensaver().value("videos").toList().isEmpty());
        QVERIFY(nothing.screensaver().value("canvas").toString().isEmpty());

        // Geometry but no video: still off, and the size of the picture is known.
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main", "pages": {"main": {"keys": []}}, "screensaver": {
            "geometry": {"screen_mm": [62.6, 33.0], "key_mm": [13.7, 13.7], "gap_mm": [5.4, 5.4], "screen_to_keys_mm": 5.4}}})"));
        const bosun::BosunBackend noVideo(file, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        QVERIFY(noVideo.screensaver().value("hasGeometry").toBool());
        QVERIFY(!noVideo.screensaver().value("enabled").toBool());
        QCOMPARE(noVideo.screensaver().value("canvas").toString(), QStringLiteral("720 × 1258"));
    }

    void theSettingsAndTheVideosAreListed()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("night.mp4"), "not really a video"));
        QVERIFY(writeFile(dir.filePath("notes.txt"), "not a video at all"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"({"file": "night.mp4", "title": "Night"}, {"file": "gone.mp4"}, {"file": "notes.txt"})")));
        const bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        const QVariantMap saver = backend.screensaver();
        QVERIFY(saver.value("enabled").toBool());
        QCOMPARE(saver.value("idleMinutes").toInt(), 3);
        QCOMPARE(saver.value("fps").toInt(), 12);
        QVERIFY(saver.value("whilePlaying").toBool());
        const QVariantList videos = saver.value("videos").toList();
        QCOMPARE(videos.size(), 3);
        const QVariantMap first = videos.at(0).toMap();
        QCOMPARE(first.value("title").toString(), QStringLiteral("Night"));
        QCOMPARE(first.value("name").toString(), QStringLiteral("night.mp4"));
        QVERIFY(first.value("found").toBool());
        QVERIFY(first.value("notes").toStringList().isEmpty()); // without ffprobe nothing is known, so nothing to say
        QVERIFY(saver.value("probeMissing").toBool());
        // A missing file and a file that is not a video are said, not hidden.
        for (const int index : {1, 2}) {
            const QVariantMap missing = videos.at(index).toMap();
            QVERIFY(!missing.value("found").toBool());
            QCOMPARE(missing.value("notes").toStringList().size(), 1);
        }
    }

    void whatFfprobeSaysIsShownWithItsWarnings()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("tall.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"({"file": "tall.mp4"})")));
        // 640 x 640 and 10 fps: square for a tall picture, below the 720 px of the screen, slower than 12.
        const QString probe = fakeProbe(dir.path(),
                                        R"({"streams": [{"codec_name": "h264", "width": 640, "height": 640, "avg_frame_rate": "10/1"}], "format": {"duration": "75.5"}})");
        const bosun::BosunBackend backend(file, {.ffprobe = probe, .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        QTRY_VERIFY_WITH_TIMEOUT(!backend.screensaver().value("videos").toList().at(0).toMap().value("details").toString().isEmpty(), 8000);
        const QVariantMap video = backend.screensaver().value("videos").toList().at(0).toMap();
        QCOMPARE(video.value("details").toString(), QStringLiteral("640 × 640 · 10.0 fps · 1:16 · h264"));
        const QString notes = video.value("notes").toStringList().join('\n');
        QVERIFY2(notes.contains("cut off at the sides"), qPrintable(notes));
        QVERIFY2(notes.contains("Smaller than the screen"), qPrintable(notes));
        QVERIFY2(notes.contains("fewer images per second"), qPrintable(notes));
    }

    void aVideoThatFfprobeCannotReadIsSaid()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("bad.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"({"file": "bad.mp4"})")));
        const QString probe = fakeProbe(dir.path(), "this is not what ffprobe prints");
        const bosun::BosunBackend backend(file, {.ffprobe = probe, .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        QTRY_VERIFY_WITH_TIMEOUT(!backend.screensaver().value("videos").toList().at(0).toMap().value("notes").toStringList().isEmpty(), 8000);
        QVERIFY(backend.screensaver().value("videos").toList().at(0).toMap().value("notes").toStringList().at(0).contains("ffprobe"));
    }

    void theScreensaverTabEditsTheFileAndShowsTheResult()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("a.mp4"), "x"));
        QVERIFY(writeFile(dir.filePath("b.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4", {"file": "b.mp4", "title": "Second"})")));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent/none", .ffmpeg = "/nonexistent/none", .thumbnailFolder = dir.filePath("thumbs")});

        QVERIFY(backend.setIdleMinutes(30).isEmpty());
        QVERIFY(backend.setFps(10).isEmpty());
        QVERIFY(backend.setWhilePlaying(false).isEmpty());
        QCOMPARE(backend.screensaver().value("idleMinutes").toInt(), 30); // read again at once, not after the file watch
        QCOMPARE(backend.screensaver().value("fps").toInt(), 10);
        QVERIFY(!backend.screensaver().value("whilePlaying").toBool());

        QVERIFY(backend.setVideoTitle(0, "First").isEmpty());
        QCOMPARE(backend.screensaver().value("videos").toList().at(0).toMap().value("title").toString(), QStringLiteral("First"));

        QVERIFY(backend.removeVideo(0).isEmpty());
        const QVariantList left = backend.screensaver().value("videos").toList();
        QCOMPARE(left.size(), 1);
        QCOMPARE(left.at(0).toMap().value("title").toString(), QStringLiteral("Second"));
        QVERIFY(QFile::exists(dir.filePath("a.mp4"))); // the video file itself is never touched
        QVERIFY(backend.removeVideo(0).isEmpty());
        QVERIFY(!backend.screensaver().value("enabled").toBool());
    }

    void aRefusedEditSaysWhyAndChangesNothing()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent/none", .ffmpeg = "/nonexistent/none", .thumbnailFolder = dir.filePath("thumbs")});
        const int before = backend.revision();
        QVERIFY(!backend.setFps(99).isEmpty());
        QVERIFY(!backend.setIdleMinutes(0).isEmpty());
        QVERIFY(!backend.removeVideo(7).isEmpty());
        QCOMPARE(backend.screensaver().value("fps").toInt(), 12);
        QCOMPARE(backend.revision(), before); // not even read again
    }

    void aChosenFileThatIsNotAVideoIsSaidAndCannotBeAdded()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("notes.txt"), "text"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent/none", .ffmpeg = "/nonexistent/none", .thumbnailFolder = dir.filePath("thumbs")});
        QVERIFY(backend.candidate().isEmpty());
        for (const QString &chosen : {dir.filePath("notes.txt"), dir.filePath("missing.mp4"), QString("file://") + dir.filePath("notes.txt")}) {
            backend.inspect(chosen);
            const QVariantMap candidate = backend.candidate();
            QCOMPARE(candidate.value("state").toString(), QStringLiteral("unusable"));
            QVERIFY(!candidate.value("canAdd").toBool());
            QVERIFY(!candidate.value("notes").toStringList().isEmpty());
            QVERIFY(!backend.addCandidate("x").isEmpty());
        }
        QCOMPARE(backend.screensaver().value("videos").toList().size(), 1); // nothing was added
    }

    void aChosenVideoIsLookedAtThenAddedAsItIs()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("Night Rain.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        // Wide (1920 x 1080), 24 fps: the sides will be cut.
        const QString probe = fakeProbe(dir.path(),
                                        R"({"streams": [{"codec_name": "h264", "width": 1920, "height": 1080, "avg_frame_rate": "24/1"}], "format": {"duration": "12"}})");
        bosun::BosunBackend backend(file, {.ffprobe = probe, .ffmpeg = "/nonexistent/none", .thumbnailFolder = dir.filePath("thumbs")});
        backend.inspect(QUrl::fromLocalFile(dir.filePath("Night Rain.mp4")).toString()); // as a file dialog gives it
        QCOMPARE(backend.candidate().value("state").toString(), QStringLiteral("checking"));
        QVERIFY(!backend.candidate().value("canAdd").toBool());
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        const QVariantMap candidate = backend.candidate();
        QCOMPARE(candidate.value("details").toString(), QStringLiteral("1920 × 1080 · 24.0 fps · 12 s · h264"));
        QVERIFY(candidate.value("notes").toStringList().join('\n').contains("at the sides"));
        QVERIFY(candidate.value("canAdd").toBool());
        QCOMPARE(candidate.value("suggestedTitle").toString(), QStringLiteral("Night Rain"));

        QVERIFY(backend.addCandidate("").isEmpty()); // no title given: the name of the file
        QVERIFY(backend.candidate().isEmpty());
        const QVariantList videos = backend.screensaver().value("videos").toList();
        QCOMPARE(videos.size(), 2);
        QCOMPARE(videos.at(1).toMap().value("title").toString(), QStringLiteral("Night Rain"));
        QCOMPARE(videos.at(1).toMap().value("file").toString(), dir.filePath("Night Rain.mp4")); // where it is, not a copy
    }

    void aVideoAlreadyInTheListOrAFullListCannotBeAdded()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("a.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent/none", .ffmpeg = "/nonexistent/none", .thumbnailFolder = dir.filePath("thumbs")});
        backend.inspect(dir.filePath("a.mp4"));
        QVERIFY(!backend.candidate().value("canAdd").toBool());
        QVERIFY(backend.candidate().value("why").toString().contains("already"));
        QVERIFY(!backend.addCandidate("again").isEmpty());

        QStringList names;
        for (int i = 0; i < galleond::MaxScreensaverVideos; ++i)
            names << QStringLiteral(R"("v%1.mp4")").arg(i);
        QVERIFY(writeFile(file, saverPages(names.join(','))));
        backend.reload();
        QVERIFY(writeFile(dir.filePath("new.mp4"), "x"));
        backend.inspect(dir.filePath("new.mp4"));
        QVERIFY(!backend.candidate().value("canAdd").toBool());
        QVERIFY(backend.candidate().value("why").toString().contains("full"));
    }

    void aVideoThatFfprobeCannotReadCannotBeAdded()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("bad.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        const QString probe = fakeProbe(dir.path(), "not what ffprobe prints");
        bosun::BosunBackend backend(file, {.ffprobe = probe, .ffmpeg = "/nonexistent/none", .thumbnailFolder = dir.filePath("thumbs")});
        backend.inspect(dir.filePath("bad.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("unusable"), 8000);
        QVERIFY(!backend.candidate().value("canAdd").toBool());
        QVERIFY(!backend.addCandidate("x").isEmpty());
    }

    void theChoiceCanBeDropped()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("a.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("b.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent/none", .ffmpeg = "/nonexistent/none", .thumbnailFolder = dir.filePath("thumbs")});
        backend.inspect(dir.filePath("a.mp4"));
        QVERIFY(!backend.candidate().isEmpty());
        backend.clearCandidate();
        QVERIFY(backend.candidate().isEmpty());
        QVERIFY(!backend.addCandidate("x").isEmpty()); // nothing chosen
    }

    void aConversionCutsTheVideoAddsTheResultAndKeepsTheOriginal()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("Night Rain.mp4"), "original"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()),
                                           .ffmpeg = fakeFfmpeg(dir.path(), "echo out_time_us=6000000; printf converted > \"$last\"; exit 0"),
                                           .thumbnailFolder = dir.filePath("thumbs"),
                                           .convertedFolder = dir.filePath("converted")});
        backend.inspect(dir.filePath("Night Rain.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        // A rectangle of other proportions: it is made valid before FFmpeg sees it (500 wide, 874 high).
        QVERIFY(backend.startConversion(100, 0, 500, 900, "").isEmpty());
        QCOMPARE(backend.conversion().value("state").toString(), QStringLiteral("running"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.conversion().value("state").toString(), QStringLiteral("done"), 8000);

        const QString result = dir.filePath("converted/Night Rain.mp4");
        QCOMPARE(backend.conversion().value("message").toString(), result);
        const QVariantList videos = backend.screensaver().value("videos").toList();
        QCOMPARE(videos.size(), 2);
        QCOMPARE(videos.at(1).toMap().value("file").toString(), result);
        QCOMPARE(videos.at(1).toMap().value("title").toString(), QStringLiteral("Night Rain"));
        QVERIFY(backend.candidate().isEmpty());
        QFile original(dir.filePath("Night Rain.mp4"));
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("original")); // the source is never touched
        QFile arguments(result + ".partial.args");
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(arguments.readAll());
        QVERIFY2(text.contains("crop=500:874:100:0,scale=720:1258:flags=lanczos,fps=12"), qPrintable(text));
    }

    void aConversionThatFailsSaysWhyAndAddsNothing()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("v.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()),
                                           .ffmpeg = fakeFfmpeg(dir.path(), "echo 'Invalid data' >&2; exit 1"),
                                           .thumbnailFolder = dir.filePath("thumbs"), .convertedFolder = dir.filePath("converted")});
        backend.inspect(dir.filePath("v.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        QVERIFY(backend.startConversion(651, 0, 618, 1080, "T").isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(backend.conversion().value("state").toString(), QStringLiteral("failed"), 8000);
        QVERIFY(backend.conversion().value("message").toString().contains("Invalid data"));
        QCOMPARE(backend.screensaver().value("videos").toList().size(), 1);
        QVERIFY(!backend.candidate().isEmpty()); // still there: the user can try again
    }

    void aConversionCanBeCancelled()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("v.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()), .ffmpeg = fakeFfmpeg(dir.path(), "sleep 30"),
                                           .thumbnailFolder = dir.filePath("thumbs"), .convertedFolder = dir.filePath("converted")});
        backend.inspect(dir.filePath("v.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        QVERIFY(backend.startConversion(651, 0, 618, 1080, "T").isEmpty());
        QVERIFY(!backend.startConversion(651, 0, 618, 1080, "T").isEmpty()); // one at a time
        backend.cancelConversion();
        QTRY_COMPARE_WITH_TIMEOUT(backend.conversion().value("state").toString(), QStringLiteral("cancelled"), 8000);
        QCOMPARE(backend.screensaver().value("videos").toList().size(), 1);
    }

    void nothingIsConvertedWhenThereIsNothingToConvertOrNoRoom()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("v.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()), .ffmpeg = fakeFfmpeg(dir.path(), "exit 0"),
                                           .thumbnailFolder = dir.filePath("thumbs"), .convertedFolder = dir.filePath("converted")});
        QVERIFY(!backend.startConversion(0, 0, 100, 100, "x").isEmpty()); // nothing chosen
        QStringList names;
        for (int i = 0; i < galleond::MaxScreensaverVideos; ++i)
            names << QStringLiteral(R"("v%1.mp4")").arg(i);
        QVERIFY(writeFile(file, saverPages(names.join(','))));
        backend.reload();
        backend.inspect(dir.filePath("v.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        QVERIFY(!backend.startConversion(651, 0, 618, 1080, "x").isEmpty()); // the list is full: no use converting
        QVERIFY(!QFileInfo::exists(dir.filePath("converted")));
    }

    void theKeysCarryWhatAnEditorNeeds()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        const QVariantMap play = backend.keys("main").at(0).toMap();
        QCOMPARE(play.value("type").toString(), QStringLiteral("media"));
        QCOMPARE(play.value("command").toString(), QStringLiteral("play-pause"));
        QCOMPARE(play.value("rawLabel").toString(), QStringLiteral("Play"));
        QCOMPARE(backend.keys("main").at(2).toMap().value("pageTarget").toString(), QStringLiteral("music"));
        QVERIFY(backend.keys("main").at(5).toMap().value("type").isNull()); // an empty key has no action to describe
        QVERIFY(backend.pageNames().contains("music"));
        QCOMPARE(backend.mediaCommands().size(), 12);
        QVERIFY(bosun::BosunBackend::keypadKeys().contains("kp0"));
    }

    void aKeyIsEditedThroughTheBackend()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        bosun::BosunBackend backend(file);
        const int before = backend.revision();
        QVERIFY(backend.setKey("music", 3, {{"type", "media"}, {"command", "volume"}, {"step", -10}, {"label", "Quieter"}}).isEmpty());
        QVERIFY(backend.revision() > before); // read again at once
        const QVariantMap key = backend.keys("music").at(3).toMap();
        QCOMPARE(key.value("command").toString(), QStringLiteral("volume"));
        QCOMPARE(key.value("step").toInt(), -10);
        QCOMPARE(key.value("label").toString(), QStringLiteral("Quieter"));
        QVERIFY(backend.clearKey("music", 3).isEmpty());
        QVERIFY(backend.keys("music").at(3).toMap().value("empty").toBool());
        QVERIFY(!backend.setKey("music", 3, {{"type", "media"}, {"command", "nonsense"}}).isEmpty());
        QVERIFY(!backend.setKey("music", 3, {{"type", "page"}, {"page", "no-such-page"}}).isEmpty());
        QVERIFY(backend.keys("music").at(3).toMap().value("empty").toBool()); // refused: nothing written
    }

    void aTabIsAddedRenamedMovedAndRemovedThroughTheBackend()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        bosun::BosunBackend backend(file);
        QVERIFY(backend.addTheme("Création Musicale", "#ab47bc").isEmpty());
        QCOMPARE(backend.themes().size(), 3);
        QCOMPARE(backend.themes().at(2).toMap().value("page").toString(), QStringLiteral("creation-musicale")); // the page name is made from the name
        QCOMPARE(backend.themes().at(2).toMap().value("color").toString(), QStringLiteral("#ab47bc"));
        QVERIFY(backend.addTheme("Création Musicale", "#ab47bc").isEmpty()); // same name again: its own page
        QCOMPARE(backend.themes().at(3).toMap().value("page").toString(), QStringLiteral("creation-musicale-2"));
        QCOMPARE(backend.keys("creation-musicale").size(), 12); // the page is there, empty

        QVERIFY(backend.setTheme(2, "Studio", "#00acc1").isEmpty());
        QCOMPARE(backend.themes().at(2).toMap().value("name").toString(), QStringLiteral("Studio"));
        QVERIFY(backend.moveTheme(2, -1).isEmpty());
        QCOMPARE(backend.themes().at(1).toMap().value("name").toString(), QStringLiteral("Studio"));
        QVERIFY(backend.removeTheme(1).isEmpty());
        QCOMPARE(backend.themes().size(), 3);
        QVERIFY(!backend.pageNames().contains("creation-musicale"));
        QVERIFY(!backend.removeTheme(0).isEmpty()); // the home page's tab stays
        QVERIFY(!backend.addTheme("", "#ab47bc").isEmpty());
    }

    void aTabGetsItsPlusPageThroughTheBackend()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        bosun::BosunBackend backend(file);
        QVERIFY(backend.subPagesOf("music").isEmpty());
        QVERIFY(backend.addSubPage("music").isEmpty());
        QCOMPARE(backend.keys("music").at(10).toMap().value("label").toString(), QStringLiteral("NEXT >"));
        QCOMPARE(backend.keys("music-plus").at(11).toMap().value("label").toString(), QStringLiteral("< BACK"));
        QCOMPARE(backend.subPagesOf("music"), (QStringList{"music-plus"})); // listed with its tab, and only there
        QVERIFY(backend.subPagesOf("main").isEmpty() || !backend.subPagesOf("main").contains("music-plus"));
        // The 12th key of this fixture's "music" is taken (a Back key): the link takes the last free one, the 11th.
        QCOMPARE(backend.keys("music").at(10).toMap().value("pageTarget").toString(), QStringLiteral("music-plus"));
        QCOMPARE(backend.keys("music-plus").at(11).toMap().value("type").toString(), QStringLiteral("back"));
        QCOMPARE(backend.themes().size(), 2); // not a new tab
        QVERIFY(backend.addSubPage("no-such-page").size() > 0);
    }

    void aSubPageTakesTheColourOfTheTabItIsShownIn()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        // "music" is the page of a theme with its own colour; shown in the tab of "main" it takes main's.
        QVERIFY(backend.tile("music", 0, "main").toImage() != backend.tile("music", 0, "music").toImage());
        QVERIFY(backend.tile("music", 0).toImage() == backend.tile("music", 0, "music").toImage());
        bosun::TileProvider provider(&backend);
        QSize size;
        QVERIFY(provider.requestPixmap("music/0?r=7&theme=main", &size, {}).toImage() == backend.tile("music", 0, "main").toImage());
    }

    void aVideoThatCanBeCutShowsWhereTheModuleFallsAndASensibleCrop()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("v.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("v.mp4")"))); // the video is in the list already: it can still be cut
        bosun::BosunBackend backend(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()), .ffmpeg = fakeFfmpeg(dir.path(), "exit 0"),
                                           .thumbnailFolder = dir.filePath("thumbs"), .convertedFolder = dir.filePath("converted")});
        backend.inspect(dir.filePath("v.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        const QVariantMap candidate = backend.candidate();
        QVERIFY(!candidate.value("canAdd").toBool());  // already there
        QVERIFY(candidate.value("canCut").toBool());   // but a cut is a new file
        QCOMPARE(candidate.value("width").toInt(), 1920);
        QCOMPARE(candidate.value("height").toInt(), 1080);
        QCOMPARE(candidate.value("canvasWidth").toInt(), 720);
        QCOMPARE(candidate.value("canvasHeight").toInt(), 1258);
        QCOMPARE(candidate.value("keyRegions").toList().size(), 12);
        QCOMPARE(candidate.value("screen").toMap().value("width").toDouble(), 720.0);
        const QVariantMap crop = candidate.value("crop").toMap();
        QCOMPARE(crop.value("x").toInt(), 651);
        QCOMPARE(crop.value("width").toInt(), 618);
        QCOMPARE(crop.value("height").toInt(), 1080);
    }

    void theCropIsMovedAndZoomedWithinTheVideo()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("v.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend backend(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()), .ffmpeg = fakeFfmpeg(dir.path(), "exit 0"),
                                           .thumbnailFolder = dir.filePath("thumbs"), .convertedFolder = dir.filePath("converted")});
        // Nothing chosen: what is given comes back.
        const QVariantMap given{{"x", 5}, {"y", 6}, {"width", 7}, {"height", 8}};
        QCOMPARE(backend.cropMoved(given, 10, 10), given);
        QCOMPARE(backend.cropZoomed(given, 2, 0, 0), given);
        QVERIFY(backend.cropCentered().value("width").toInt() == 0);

        backend.inspect(dir.filePath("v.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(backend.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        const QVariantMap crop = backend.candidate().value("crop").toMap();
        QCOMPARE(backend.cropMoved(crop, 100, 0).value("x").toInt(), 751);
        QCOMPARE(backend.cropMoved(crop, 99999, 0).value("x").toInt(), 1302); // kept inside the video
        QCOMPARE(backend.cropMoved(crop, -99999, 0).value("x").toInt(), 0);
        const QVariantMap closer = backend.cropZoomed(crop, 2, 960, 540); // around the middle of the video
        QCOMPARE(closer.value("width").toInt(), 309);
        QCOMPARE(closer.value("height").toInt(), 540);
        QCOMPARE(backend.cropZoomed(crop, 0.1, 960, 540).value("width").toInt(), 618); // cannot go wider than the video allows
    }

    void aFullListOrNoFfmpegMeansNoCut()
    {
        const QTemporaryDir dir;
        QVERIFY(writeFile(dir.filePath("v.mp4"), "x"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, saverPages(R"("a.mp4")")));
        bosun::BosunBackend noFfmpeg(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()), .ffmpeg = "/nonexistent/none",
                                            .thumbnailFolder = dir.filePath("thumbs"), .convertedFolder = dir.filePath("converted")});
        noFfmpeg.inspect(dir.filePath("v.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(noFfmpeg.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        QVERIFY(!noFfmpeg.candidate().value("canCut").toBool());
        QVERIFY(!noFfmpeg.candidate().value("cutWhy").toString().isEmpty());
        QVERIFY(noFfmpeg.candidate().value("canAdd").toBool()); // as it is still works
        QVERIFY(!noFfmpeg.startConversion(651, 0, 618, 1080, "x").isEmpty());

        QStringList names;
        for (int i = 0; i < galleond::MaxScreensaverVideos; ++i)
            names << QStringLiteral(R"("v%1.mp4")").arg(i);
        QVERIFY(writeFile(file, saverPages(names.join(','))));
        bosun::BosunBackend full(file, {.ffprobe = fakeProbe(dir.path(), wideProbe()), .ffmpeg = fakeFfmpeg(dir.path(), "exit 0"),
                                        .thumbnailFolder = dir.filePath("thumbs"), .convertedFolder = dir.filePath("converted")});
        full.inspect(dir.filePath("v.mp4"));
        QTRY_COMPARE_WITH_TIMEOUT(full.candidate().value("state").toString(), QStringLiteral("ready"), 8000);
        QVERIFY(!full.candidate().value("canCut").toBool());
        QVERIFY(full.candidate().value("cutWhy").toString().contains("full"));
    }

    void withTheTrayIconClosingTheWindowOnlyHidesIt()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        window->setProperty("hideOnClose", true);
        window->show();
        QVERIFY(window->isVisible());
        QVERIFY(window->close() || !window->isVisible());
        QVERIFY(!window->isVisible());          // hidden...
        window->show();                         // ...and it can come back: it was not destroyed
        QVERIFY(window->isVisible());
        // Without the tray the window closes like any window (nothing keeps it).
        window->setProperty("hideOnClose", false);
        window->close();
        QVERIFY(!window->isVisible());
    }

    void aTabShowsOnlyTheSubPagesItsOwnKeysLeadTo()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        // main -> sub-a -> sub-b; music -> sub-c; games -> main (a theme's page: not a sub-page); an orphan page nobody leads to.
        QVERIFY(writeFile(file, QByteArray(R"({"version": 1, "home": "main",
          "themes": [{"name": "Control", "page": "main", "color": "#ffd600"}, {"name": "Music", "page": "music", "color": "#e53935"},
                     {"name": "Games", "page": "games", "color": "#3949ab"}],
          "pages": {
            "main": {"keys": [{"key": 0, "label": "A", "action": {"type": "page", "page": "sub-a"}}]},
            "sub-a": {"keys": [{"key": 0, "label": "B", "action": {"type": "page", "page": "sub-b"}}, {"key": 1, "label": "Home", "action": {"type": "back"}}]},
            "sub-b": {"keys": [{"key": 0, "label": "Loop", "action": {"type": "page", "page": "sub-a"}}]},
            "music": {"keys": [{"key": 0, "label": "C", "action": {"type": "page", "page": "sub-c"}}]},
            "sub-c": {"keys": []},
            "games": {"keys": [{"key": 0, "label": "Main", "action": {"type": "page", "page": "main"}}]},
            "orphan": {"keys": []}}})")));
        const bosun::BosunBackend backend(file);
        QVERIFY2(backend.problem().isEmpty(), qPrintable(backend.problem()));
        QCOMPARE(backend.subPagesOf("main"), (QStringList{"sub-a", "sub-b"}));   // two levels, a loop does not repeat
        QCOMPARE(backend.subPagesOf("music"), (QStringList{"sub-c"}));            // not main's
        QVERIFY(backend.subPagesOf("games").isEmpty());                           // main is a theme's page: it has its tab
        QVERIFY(backend.subPagesOf("orphan").isEmpty());
        QVERIFY(backend.subPagesOf("no-such-page").isEmpty());
    }

    void selectedTabKeepsItsTitleVisibleWithoutMovingLanguagePicker()
    {
        const QTemporaryDir folder;
        const auto file = folder.filePath("pages.json");
        QJsonArray themes;
        QJsonObject pages;
        for (int i = 0; i < 8; ++i) {
            const auto page = QStringLiteral("page%1").arg(i);
            themes.append(QJsonObject{{"name", QString::number(i) + QString(22, QLatin1Char('W'))},
                                      {"page", page}, {"color", "#80e5d1"}});
            pages.insert(page, QJsonObject{{"keys", QJsonArray{}}});
        }
        QVERIFY(writeFile(file, QJsonDocument(QJsonObject{{"version", 1}, {"home", "page0"},
                                                         {"themes", themes}, {"pages", pages}}).toJson()));
        const bosun::BosunBackend backend(file);
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = folder.filePath("own"), .settingsFile = folder.filePath("galleond.conf")});
        bosun::LanguageSettings languages({});
        languages.setPreference("de");
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.setInitialProperties({{"backend", QVariant::fromValue(&backend)}, {"daemon", QVariant::fromValue(&daemon)},
                                     {"library", QVariant::fromValue(&library)}, {"languages", QVariant::fromValue(&languages)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        window->setWidth(1000);
        auto *picker = window->findChild<QQuickItem *>("languageMenu");
        auto *viewport = window->findChild<QQuickItem *>("tabViewport");
        auto *bar = window->findChild<QQuickItem *>("tabBar");
        QVERIFY(picker && viewport && bar);
        QTRY_VERIFY(picker->x() > 0); // Wait for the initial window anchors.
        const auto pickerPosition = picker->position();
        const auto pickerSize = picker->size();
        for (const int index : {7, 13}) {
            window->setProperty("currentTab", index);
            QPointer<QQuickItem> selected;
            for (auto *item : bar->childItems())
                if (item->property("current").toBool()) selected = item;
            QVERIFY(selected);
            const auto target = viewport->property("tabWidths").toList().at(index).toDouble();
            const auto initialWidth = selected->width();
            QTRY_VERIFY(selected && selected->property("resizing").toBool());
            QTest::qWait(200);
            QVERIFY(selected && selected->width() > initialWidth && selected->width() < target);
            QTRY_VERIFY(selected && qAbs(selected->width() - target) < 0.5);
            QFont font = QGuiApplication::font();
            font.setPixelSize(14);
            const auto title = selected->property("modelData").toMap().value("title").toString();
            QTRY_VERIFY(selected && selected->width() >= QFontMetricsF(font).horizontalAdvance(title) + 8);
            QTRY_VERIFY(selected && selected->mapToItem(viewport, QPointF()).x() >= -1);
            QTRY_VERIFY(selected && selected->mapToItem(viewport, QPointF()).x() + selected->width() <= viewport->width() + 1);
            QCOMPARE(picker->position(), pickerPosition);
            QCOMPARE(picker->size(), pickerSize);
            QVERIFY(viewport->x() + viewport->width() <= picker->x());
        }
    }

    void theKeyboardBrightnessSliderSendsOneStepOfTenPercentWhenLetGo()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *slider = engine.rootObjects().constFirst()->findChild<QQuickItem *>("keyboardBrightness");
        QVERIFY(slider);
        QCOMPARE(slider->property("value").toInt(), -1); // unknown: it cannot read the keyboard
        QVERIFY(!slider->property("enabledLook").toBool()); // galleond is not running: nothing to ask

        const QSignalSpy committed(slider, SIGNAL(committed(int)));
        QVERIFY(QMetaObject::invokeMethod(slider, "choose", Q_ARG(QVariant, 3)));
        QVERIFY(QMetaObject::invokeMethod(slider, "choose", Q_ARG(QVariant, 6)));
        QCOMPARE(committed.size(), 0); // moving shows a value, sends nothing
        QCOMPARE(slider->property("shown").toInt(), 600);
        QVERIFY(QMetaObject::invokeMethod(slider, "commit"));
        QCOMPARE(committed.size(), 1);
        QCOMPARE(committed.constFirst().constFirst().toInt(), 600);
        QCOMPARE(slider->property("value").toInt(), 600);

        QVERIFY(QMetaObject::invokeMethod(slider, "choose", Q_ARG(QVariant, 6))); // let go where it already is: nothing to send
        QVERIFY(QMetaObject::invokeMethod(slider, "commit"));
        QCOMPARE(committed.size(), 1);

        QVERIFY(QMetaObject::invokeMethod(slider, "choose", Q_ARG(QVariant, 0)));
        QVERIFY(QMetaObject::invokeMethod(slider, "commit"));
        QCOMPARE(committed.size(), 2);
        QCOMPARE(committed.constLast().constFirst().toInt(), 0); // 0 % is a stop like any other

        QVERIFY(QMetaObject::invokeMethod(slider, "choose", Q_ARG(QVariant, 10)));
        QVERIFY(QMetaObject::invokeMethod(slider, "commit"));
        QCOMPARE(committed.constLast().constFirst().toInt(), 1000);
    }

    void theBrightnessCurveEditorSavesEachPointItsHandleIsLetGoAtAndTheRadiosChooseWhoIsInCharge()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        auto *editor = root->findChild<QQuickItem *>("brightnessCurveEditor");
        auto *sliderRadio = root->findChild<QQuickItem *>("brightnessModeSlider");
        auto *curveRadio = root->findChild<QQuickItem *>("brightnessModeCurve");
        auto *slider = root->findChild<QQuickItem *>("keyboardBrightness");
        QVERIFY(editor);
        QVERIFY(sliderRadio);
        QVERIFY(curveRadio);
        QVERIFY(slider);
        const QString curveFile = dir.filePath("brightness-curve.json"); // next to the pages file
        QVERIFY(!QFile::exists(curveFile)); // opening the window writes nothing
        QVERIFY(sliderRadio->property("checked").toBool());
        QVERIFY(!curveRadio->property("checked").toBool());

        // Dragging shows a value and sends nothing; letting go commits one point, brought to a step of 10 %.
        const QSignalSpy committed(editor, SIGNAL(committed(int, int)));
        QVERIFY(QMetaObject::invokeMethod(editor, "choose", Q_ARG(QVariant, 3), Q_ARG(QVariant, 640)));
        QCOMPARE(editor->property("previewHour").toInt(), 3);
        QCOMPARE(editor->property("previewValue").toInt(), 600);
        QCOMPARE(committed.size(), 0);
        QVERIFY(!QFile::exists(curveFile));
        QVERIFY(QMetaObject::invokeMethod(editor, "commit"));
        QCOMPARE(committed.size(), 1);
        QCOMPARE(committed.constFirst().at(0).toInt(), 3);
        QCOMPARE(committed.constFirst().at(1).toInt(), 600);
        QCOMPARE(galleond::readBrightnessCurve(curveFile).value_or(galleond::BrightnessCurve{}).hours.at(3), 600);
        QCOMPARE(editor->property("previewHour").toInt(), -1);

        // Letting go where the point already is sends nothing.
        QVERIFY(QMetaObject::invokeMethod(editor, "choose", Q_ARG(QVariant, 3), Q_ARG(QVariant, 604)));
        QVERIFY(QMetaObject::invokeMethod(editor, "commit"));
        QCOMPARE(committed.size(), 1);

        // The radio buttons choose who is in charge; the slider gives way to the curve.
        QVERIFY(QMetaObject::invokeMethod(curveRadio, "chosen"));
        QTRY_VERIFY(curveRadio->property("checked").toBool());
        QVERIFY(!sliderRadio->property("checked").toBool());
        QCOMPARE(galleond::readBrightnessCurve(curveFile).value_or(galleond::BrightnessCurve{}).mode, galleond::BrightnessCurve::Mode::Curve);
        QVERIFY(!slider->property("enabledLook").toBool());
        QVERIFY(QMetaObject::invokeMethod(sliderRadio, "chosen"));
        QTRY_VERIFY(sliderRadio->property("checked").toBool());
        QCOMPARE(galleond::readBrightnessCurve(curveFile).value_or(galleond::BrightnessCurve{}).mode, galleond::BrightnessCurve::Mode::Slider);
    }

    void theKeyboardTabScrollsWhenItIsTallerThanTheWindowSoNothingIsOutOfReach()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        auto *scroll = root->findChild<QQuickItem *>("keyboardScroll");
        auto *rail = root->findChild<QQuickItem *>("keyboardScrollRail");
        auto *editor = root->findChild<QQuickItem *>("brightnessCurveEditor");
        QVERIFY(scroll);
        QVERIFY(rail);
        QVERIFY(editor);

        // The Keyboard tab is after the two themes and the "+" tab.
        QVERIFY(root->setProperty("currentTab", 3));
        // A short window: more is below than fits, and the bar is there to reach it.
        QVERIFY(root->setProperty("height", 520));
        QTRY_VERIFY(scroll->property("contentHeight").toDouble() > scroll->height() + 1);
        QVERIFY(rail->property("needed").toBool());
        QTRY_VERIFY(rail->isVisible());

        // At the end of its travel the bottom of the curve editor is in view.
        const double travel = scroll->property("contentHeight").toDouble() - scroll->height();
        QVERIFY(scroll->setProperty("contentY", travel));
        QTRY_VERIFY(editor->mapToItem(scroll, QPointF(0, editor->height())).y() <= scroll->height() + 1);

        // The window is wide enough for everything: no bar at the bottom (it is there for wider content to come).
        auto *horizontalRail = root->findChild<QQuickItem *>("keyboardScrollRailHorizontal");
        QVERIFY(horizontalRail);
        QVERIFY(!horizontalRail->property("needed").toBool());

        // A window tall enough for everything needs no bar.
        QVERIFY(root->setProperty("height", 2600));
        QTRY_VERIFY(!rail->property("needed").toBool());
    }

    void aKeyWhoseApplicationIsNotInstalledShowsASymbolAndTheEditorSaysSo()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes())); // the key 1 of the first theme names an application that does not exist
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QVERIFY(root->setProperty("currentTab", 0));
        // Exactly one key of this theme names an application, and it is missing: one symbol, on a visible key.
        QTRY_VERIFY_WITH_TIMEOUT(!namedItems(root, "missingApp").isEmpty(), 3000); // one per key, shown for the one that needs it
        QCOMPARE(namedItems(root, "missingApp").size(), 12);
        int shown = 0;
        for (const auto *badge : namedItems(root, "missingApp")) {
            if (badge->isVisible())
                ++shown;
        }
        QCOMPARE(shown, 1);
        // Selecting it: the editor of the key says that the application is missing.
        QVERIFY(root->setProperty("selectedKey", 1));
        auto *note = root->findChild<QQuickItem *>("missingAppNote");
        QVERIFY(note);
        QTRY_VERIFY_WITH_TIMEOUT(note->isVisible(), 3000);
        QVERIFY(root->setProperty("selectedKey", 0)); // a media key: no note
        QTRY_VERIFY_WITH_TIMEOUT(!note->isVisible(), 3000);
    }

    void theTwelveKeysEndAboveTheStatusBarInALowWindowAndKeepTheirSizeInATallOne()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QVERIFY(root->setProperty("currentTab", 0));
        const auto page = namedItems(root, "themePage");
        QCOMPARE(page.size(), 1);
        const auto lowestKeyBottom = [&] {
            double bottom = 0;
            for (const QQuickItem *cell : namedItems(root, "keyCell"))
                bottom = std::max(bottom, cell->mapToScene(QPointF(0, cell->height())).y());
            return bottom;
        };
        const auto pageBottom = [&] { return page.constFirst()->mapToScene(QPointF(0, page.constFirst()->height())).y(); };

        QVERIFY(root->setProperty("height", 700)); // lower than the four rows of 140 px and what is above them
        QTRY_VERIFY_WITH_TIMEOUT(namedItems(root, "keyCell").size() == 12, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(namedItems(root, "keyCell").constFirst()->height() < 140, 3000);
        // The grid lays its keys out a moment after they changed size: wait for the final place.
        QTRY_VERIFY2_WITH_TIMEOUT(lowestKeyBottom() <= pageBottom() + 1,
                                  qPrintable(QStringLiteral("the last row of keys is under the status bar: keys end at %1, the page at %2, a key is %3")
                                                 .arg(lowestKeyBottom()).arg(pageBottom()).arg(namedItems(root, "keyCell").constFirst()->height())), 3000);

        QVERIFY(root->setProperty("height", 1200));
        QTRY_COMPARE_WITH_TIMEOUT(namedItems(root, "keyCell").constFirst()->height(), 140.0, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(lowestKeyBottom() <= pageBottom() + 1, 3000);
    }

    void aWallpaperThatArrivedAfterTheLibraryWasReadIsStillShownAsTheThumbnailOfItsTab()
    {
        // What happens when a theme is installed: the library of wallpapers was read when Bosun started, the theme brings its own
        // picture afterwards, and the tab is shown after that.
        const QTemporaryDir dir;
        const QString own = dir.filePath("own");
        QVERIFY(QDir().mkpath(own));
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = own, .settingsFile = dir.filePath("galleond.conf")});
        QVERIFY(QDir().mkpath(own + "/son"));
        QImage picture(720, 384, QImage::Format_RGB32);
        picture.fill(Qt::blue);
        QVERIFY(picture.save(own + "/son/late.jpg", "JPEG"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main",
            "themes": [{"name": "Son", "page": "main", "color": "#e53935", "wallpaper": "son/late.jpg"}],
            "pages": {"main": {"keys": []}}})"));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QVERIFY(root->setProperty("currentTab", 0));
        QTRY_VERIFY_WITH_TIMEOUT(!namedItems(root, "tabWallpaperThumbnail").isEmpty(), 3000);
        QQuickItem *thumbnail = namedItems(root, "tabWallpaperThumbnail").constFirst();
        QTRY_COMPARE_WITH_TIMEOUT(thumbnail->property("status").toInt(), 1, 3000); // Image.Ready
    }

    void aTabWhosePictureIsNotInTheListSaysThereIsNoPreviewInsteadOfAnEmptyFrame()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main",
            "themes": [{"name": "Son", "page": "main", "color": "#e53935", "wallpaper": "son/not-there.jpg"}],
            "pages": {"main": {"keys": []}}})"));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QVERIFY(root->setProperty("currentTab", 0));
        QTRY_VERIFY_WITH_TIMEOUT(!namedItems(root, "tabWallpaperMissing").isEmpty(), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(namedItems(root, "tabWallpaperMissing").constFirst()->isVisible(), 3000);
    }

    void theThumbnailFollowsTheTabEvenWhenThePagesWereReadAgainWhileAnotherTabWasShown()
    {
        // What happens when a theme is installed: the pages file is read again while the tab on screen has no wallpaper, then the new
        // tab (which has one) is shown. The thumbnail must follow it.
        const QTemporaryDir dir;
        const QString own = dir.filePath("own");
        QVERIFY(QDir().mkpath(own + "/son"));
        QImage picture(720, 384, QImage::Format_RGB32);
        picture.fill(Qt::green);
        QVERIFY(picture.save(own + "/son/after.jpg", "JPEG"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main",
            "themes": [{"name": "Plain", "page": "main", "color": "#ffd600"},
                       {"name": "Son", "page": "other", "color": "#e53935", "wallpaper": "son/after.jpg"}],
            "pages": {"main": {"keys": []}, "other": {"keys": []}}})"));
        bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = own, .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QVERIFY(root->setProperty("currentTab", 0)); // the tab with no wallpaper
        QTest::qWait(100);
        backend.reload(); // the pages are read again, as after an install
        QTest::qWait(100);
        QVERIFY(root->setProperty("currentTab", 1)); // the tab with one
        const auto thumbnail = [&]() -> QQuickItem * {
            const auto found = namedItems(root, "tabWallpaperThumbnail");
            return found.isEmpty() ? nullptr : found.constFirst();
        };
        QTRY_VERIFY_WITH_TIMEOUT(thumbnail() != nullptr && thumbnail()->isVisible(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(thumbnail()->property("status").toInt(), 1, 3000); // Image.Ready
    }

    void aTabWithItsOwnWallpaperShowsItsThumbnailAndOneWithoutShowsNone()
    {
        const QTemporaryDir dir;
        const QString own = dir.filePath("own");
        QVERIFY(QDir().mkpath(own + "/son"));
        QImage picture(720, 384, QImage::Format_RGB32);
        picture.fill(Qt::red);
        QVERIFY(picture.save(own + "/son/pic.jpg", "JPEG"));
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, R"({"version": 1, "home": "main",
            "themes": [{"name": "Son", "page": "main", "color": "#e53935", "wallpaper": "son/pic.jpg"},
                       {"name": "Plain", "page": "other", "color": "#ffd600"}],
            "pages": {"main": {"keys": []}, "other": {"keys": []}}})"));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = own, .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QVERIFY(root->setProperty("currentTab", 0));
        const auto thumbnail = [&]() -> QQuickItem * {
            const auto found = namedItems(root, "tabWallpaperThumbnail");
            return found.isEmpty() ? nullptr : found.constFirst();
        };
        QTRY_VERIFY_WITH_TIMEOUT(thumbnail() != nullptr, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(thumbnail()->isVisible(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(thumbnail()->property("status").toInt(), 1, 3000); // Image.Ready: the picture was found
        QVERIFY(root->setProperty("currentTab", 1)); // the tab with no wallpaper of its own
        QTRY_VERIFY_WITH_TIMEOUT(!thumbnail()->isVisible(), 3000);
    }

    void everyTabHasItsScrollBarAndOnlyTheShownTabShowsOne()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        // The tabs after the two themes: the "+" tab (2), the keyboard (3), then the pages below.
        const QList<QPair<int, QString>> tabs = {{1, "themeScrollRail"}, {4, "wallpapersPageScrollRail"}, {5, "widgetsPageScrollRail"},
                                                 {6, "gamingPageScrollRail"}, {7, "screensaversPageScrollRail"}};
        QVERIFY(root->setProperty("height", 520));
        for (const auto &[index, shown] : tabs) {
            QVERIFY(root->setProperty("currentTab", index));
            for (const auto &[otherIndex, name] : tabs) {
                auto *rail = root->findChild<QQuickItem *>(name);
                QVERIFY2(rail, qPrintable(name));
                if (name != shown)
                    QTRY_VERIFY2(!rail->isVisible(), qPrintable(name + " is shown on tab " + QString::number(index)));
            }
            auto *rail = root->findChild<QQuickItem *>(shown);
            // Shown exactly when its page has something out of sight.
            QTRY_COMPARE(rail->isVisible(), rail->property("needed").toBool());
            if (index == 6)
                QVERIFY2(rail->property("needed").toBool(), qPrintable(shown + " should be needed in a 520 px window"));
        }
    }

    void aScrollRailFollowsItsFlickableInBothDirectionsAndOnlyShowsWhenSomethingIsOutOfView()
    {
        QQmlEngine engine;
        QQmlComponent flickableComponent(&engine);
        flickableComponent.setData("import QtQuick\nFlickable { width: 300; height: 200; contentWidth: 900; contentHeight: 100 }", QUrl());
        const QScopedPointer<QObject> flickable(flickableComponent.create());
        QVERIFY(flickable);
        QQmlComponent railComponent(&engine, QUrl(QStringLiteral("qrc:/bosun/qml/ScrollRail.qml")));
        QVERIFY2(railComponent.status() == QQmlComponent::Ready, qPrintable(railComponent.errorString()));
        const QScopedPointer<QObject> horizontal(railComponent.createWithInitialProperties(
            {{QStringLiteral("flickable"), QVariant::fromValue(flickable.data())}, {QStringLiteral("orientation"), Qt::Horizontal},
             {QStringLiteral("width"), 300}, {QStringLiteral("height"), 10}}));
        const QScopedPointer<QObject> vertical(railComponent.createWithInitialProperties(
            {{QStringLiteral("flickable"), QVariant::fromValue(flickable.data())}, {QStringLiteral("width"), 10}, {QStringLiteral("height"), 200}}));
        QVERIFY(horizontal);
        QVERIFY(vertical);

        // 900 wide in 300: the bar is needed, its handle is a third of the track, at the start.
        QVERIFY(horizontal->property("needed").toBool());
        QCOMPARE(horizontal->property("handleLength").toDouble(), 100.0);
        QCOMPARE(horizontal->property("handleOffset").toDouble(), 0.0);
        // 100 high in 200: nothing is out of view vertically, so no vertical bar.
        QVERIFY(!vertical->property("needed").toBool());

        // At the end of the travel the handle is at the end of the track.
        QVERIFY(flickable->setProperty("contentX", 600.0));
        QCOMPARE(horizontal->property("handleOffset").toDouble(), 200.0);
        QVERIFY(QMetaObject::invokeMethod(horizontal.data(), "moveTo", Q_ARG(QVariant, 150.0)));
        QCOMPARE(flickable->property("contentX").toDouble(), 150.0);

        // Content that fits needs no bar.
        QVERIFY(flickable->setProperty("contentWidth", 250.0));
        QVERIFY(!horizontal->property("needed").toBool());
    }

    void withTheLiveLightingOnTheColourGivenToEveryKeyGoesToTheKeyboardAndTheBrightnessSaysItIsSuspended()
    {
        const QString connectionName = QStringLiteral("fake-galleond-for-the-preview");
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
        LightingEndpoint endpoint;
        QVERIFY(bus.registerObject("/org/galleoncore/Lighting", &endpoint, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        QVERIFY(bus.registerService(bosun::DaemonStatus::BusName));
        const auto cleanUp = qScopeGuard([&] {
            bus.unregisterObject("/org/galleoncore/Lighting");
            bus.unregisterService(bosun::DaemonStatus::BusName);
            QDBusConnection::disconnectFromBus(connectionName);
        });

        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        QTRY_VERIFY_WITH_TIMEOUT(daemon.lightingActive(), 3000); // the daemon says that it holds the keyboard
        QVERIFY(root->setProperty("currentTab", 3));
        auto *board = root->findChild<QQuickItem *>("keyboardMap");
        QVERIFY(board);

        // The note under the brightness says that it is suspended.
        auto *note = root->findChild<QQuickItem *>("brightnessSuspendedNote");
        QVERIFY(note);
        QVERIFY(note->isVisible());

        // A colour chosen for a key, then given to every key: it goes to the keyboard.
        const QVariantList lamps{QVariant(23)};
        QVERIFY(QMetaObject::invokeMethod(board, "capClicked", Q_ARG(int, 40), Q_ARG(QVariant, QVariant(lamps))));
        auto *swatches = root->findChild<QQuickItem *>("lampSwatches");
        QVERIFY(swatches);
        QVERIFY(QMetaObject::invokeMethod(swatches, "picked", Q_ARG(QString, QStringLiteral("#e53935"))));
        QVERIFY(endpoint.colours.isEmpty()); // one key only: a colour per key cannot be sent yet
        auto *all = root->findChild<QQuickItem *>("lampsColourAll");
        QVERIFY(all);
        QVERIFY(QMetaObject::invokeMethod(all, "clicked"));
        QTRY_COMPARE_WITH_TIMEOUT(endpoint.colours.size(), 1, 3000);
        QCOMPARE(endpoint.colours.constFirst(), (QList<int>{229, 57, 53}));
    }

    void aKeyOfTheKeyboardDrawingTakesTheColourPickedForItAndNothingIsSent()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const bosun::BosunBackend backend(file);
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *root = engine.rootObjects().constFirst();
        auto *board = root->findChild<QQuickItem *>("keyboardMap");
        QVERIFY(board);

        // The drawing knows the 94 key lamps, and the caps that carry them.
        QCOMPARE(board->property("allLamps").toList().size(), 94);
        QVERIFY(board->property("map").toMap().value("caps").toList().size() > 80);
        QVERIFY(root->property("lampColors").toMap().isEmpty());
        QCOMPARE(root->property("selectedCap").toInt(), -1);

        // Clicking a cap chooses it, with the lamps under it.
        const QVariantList lamps{QVariant(23)};
        QVERIFY(QMetaObject::invokeMethod(board, "capClicked", Q_ARG(int, 40), Q_ARG(QVariant, QVariant(lamps))));
        QCOMPARE(root->property("selectedCap").toInt(), 40);
        QCOMPARE(root->property("selectedLamps").toList(), lamps);

        // A colour picked for it goes to its lamps and to no other.
        auto *swatches = root->findChild<QQuickItem *>("lampSwatches");
        QVERIFY(swatches);
        QVERIFY(QMetaObject::invokeMethod(swatches, "picked", Q_ARG(QString, QStringLiteral("#e53935"))));
        const QVariantMap colours = root->property("lampColors").toMap();
        QCOMPARE(colours.size(), 1);
        QCOMPARE(colours.value("23").toString(), QStringLiteral("#e53935"));

        // The rainbow gives every lamp a colour, a different one at each end of the keyboard.
        QVariant rainbow;
        QVERIFY(QMetaObject::invokeMethod(board, "rainbow", Q_RETURN_ARG(QVariant, rainbow)));
        const QVariantMap all = rainbow.toMap();
        QCOMPARE(all.size(), 94);
        QVERIFY(all.value("0").toString() != all.value("85").toString());

        // Clearing forgets the colours and the choice.
        auto *clear = root->findChild<QQuickItem *>("lampsClear");
        QVERIFY(clear);
        QVERIFY(QMetaObject::invokeMethod(clear, "clicked"));
        QVERIFY(root->property("lampColors").toMap().isEmpty());
        QCOMPARE(root->property("selectedCap").toInt(), -1);
    }

    void theWindowHasATabPerThemeAndTheFixedOnes()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        bosun::BosunBackend backend(file);
        bosun::LanguageSettings languages({});
        languages.setPreference("en");
        QQmlApplicationEngine engine;
        connect(&languages, &bosun::LanguageSettings::changed, &engine, &QQmlApplicationEngine::retranslate);
        connect(&languages, &bosun::LanguageSettings::changed, &backend, [&backend] { Q_EMIT backend.changed(); });
        engine.addImageProvider(QStringLiteral("tiles"), new bosun::TileProvider(&backend));
        const bosun::DaemonStatus daemon("/nonexistent/systemctl");
        const bosun::WallpaperLibrary library({.roots = {}, .userRoot = dir.filePath("own"), .settingsFile = dir.filePath("galleond.conf")});
        engine.addImageProvider(QStringLiteral("wallpapers"), new bosun::WallpaperThumbnails(&library));
        engine.addImageProvider(QStringLiteral("wallpaperpreview"), new bosun::WallpaperPreview(&library));
        engine.addImageProvider(QStringLiteral("themeicon"), new galleond::ThemeIconProvider);
        engine.setInitialProperties({{QStringLiteral("backend"), QVariant::fromValue(&backend)}, {QStringLiteral("daemon"), QVariant::fromValue(&daemon)},
                                     {QStringLiteral("library"), QVariant::fromValue(&library)},
                                     {QStringLiteral("languages"), QVariant::fromValue(&languages)}});
        engine.load(QUrl(QStringLiteral("qrc:/bosun/qml/Bosun.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        QObject *window = engine.rootObjects().constFirst();
        // The window must be able to call what it is given (the daemon's start, the backend's edits).
        QVERIFY(qvariant_cast<QObject *>(window->property("daemon")) != nullptr);
        QVERIFY(qvariant_cast<QObject *>(window->property("backend")) != nullptr);
        QCOMPARE(window->property("tabs").toList().size(), 2 + 6); // themes, new tab, keyboard, wallpapers, widgets, gaming, screensavers
        auto *firmware = window->findChild<QObject *>("firmwareVersion");
        auto *firmwareWarning = window->findChild<QObject *>("firmwareWarning");
        QVERIFY(firmware);
        QVERIFY(firmwareWarning);
        QVERIFY(window->findChild<QObject *>("firmwareStatusBar"));
        auto *home = qobject_cast<galleond::HomeWidgets *>(qvariant_cast<QObject *>(backend.property("homeWidgets")));
        QVERIFY(home);
        home->setFirmwareVersion("3.06.005");
        QTRY_VERIFY(firmware->property("text").toString().contains("3.06.005"));
        QCOMPARE(firmwareWarning->property("visible").toBool(), false); // keyboard tab is not selected
        home->setFirmwareVersion("3.06.006");
        QTRY_VERIFY(firmware->property("text").toString().contains("3.06.006"));
        QCOMPARE(window->property("keys").toList().size(), 12);
        QVERIFY(window->property("selected").toMap().isEmpty()); // nothing chosen yet
        window->setProperty("selectedKey", 0);
        QCOMPARE(window->property("selected").toMap().value("label").toString(), QStringLiteral("Play"));
        // Unrelated backend notifications must not discard an unfinished shortcut.
        auto *editor = window->findChild<QObject *>("keyEditor");
        QVERIFY(editor);
        QVERIFY(QMetaObject::invokeMethod(editor, "change", Q_ARG(QVariant, QVariant("label")),
                                        Q_ARG(QVariant, QVariant("Unfinished shortcut"))));
        backend.reload();
        QVERIFY(QMetaObject::invokeMethod(&backend, "changed"));
        QCoreApplication::processEvents();
        QCOMPARE(editor->property("spec").toMap().value("label").toString(), QStringLiteral("Unfinished shortcut"));
        QCOMPARE(window->property("selectedKey").toInt(), 0);
        auto *languageMenu = window->findChild<QObject *>("languageMenu");
        QVERIFY(languageMenu);
        languageMenu->setProperty("expanded", true);
        QVERIFY(QMetaObject::invokeMethod(languageMenu, "choose", Q_ARG(QVariant, QStringLiteral("fr"))));
        QCOMPARE(languageMenu->property("expanded").toBool(), false);
        QCOMPARE(window->property("tabs").toList().at(3).toMap().value("title").toString(), QStringLiteral("Clavier"));
        QCOMPARE(window->property("tabs").toList().first().toMap().value("title").toString(), QStringLiteral("Control"));
        QCOMPARE(editor->property("spec").toMap().value("label").toString(), QStringLiteral("Unfinished shortcut"));
        QCOMPARE(backend.mediaCommands().first().toMap().value("label").toString(), QStringLiteral("Lecture / pause"));
        languages.setPreference("en");
        window->setProperty("selectedKey", 1);
        QVERIFY(editor->property("spec").toMap().value("label").toString() != QStringLiteral("Unfinished shortcut"));
        // Another tab forgets the selection; a fixed tab has no keys.
        window->setProperty("currentTab", 1);
        QCOMPARE(window->property("selectedKey").toInt(), -1);
        QCOMPARE(window->property("keys").toList().size(), 12);
        window->setProperty("currentTab", 5);
        QVERIFY(window->property("keys").toList().isEmpty());
        const auto tabs = window->property("tabs").toList();
        for (int i = 0; i < tabs.size(); ++i) {
            const auto kind = tabs[i].toMap().value("kind").toString();
            if (kind != "widgets" && kind != "gaming") continue;
            window->setProperty("currentTab", i);
            auto *widgets = window->findChild<QQuickItem *>(kind == "gaming" ? "gamingPage" : "widgetsPage");
            QVERIFY(widgets);
            QVERIFY(widgets->isVisible());
            if (kind == "gaming") {
                const auto *mode = window->findChild<QQuickItem *>("gameModePage");
                QVERIFY(mode && widgets->isAncestorOf(mode));
                const auto *show = window->findChild<QQuickItem *>("showGamingOnPad");
                QVERIFY(show && !show->isVisible());
                const auto *pad = window->findChild<QQuickItem *>("gamingPadButtons");
                QVERIFY(pad && !pad->isVisible());
                const auto *launch = window->findChild<QQuickItem *>("steamFpsLaunchCommand");
                QVERIFY(launch);
                QVERIFY(launch->property("text").toString().contains("mangohud %command%"));
                const auto *pol = window->findChild<QQuickItem *>("polFpsLaunchCommand");
                QVERIFY(pol && pol->property("text").toString().contains("export BEFORE_WINE="));
                QVERIFY(!pol->property("text").toString().contains("%command%"));
                const auto *flatpak = window->findChild<QQuickItem *>("flatpakFpsAccess");
                QVERIFY(flatpak && !flatpak->isVisible());
                auto *manualHelp = window->findChild<QQuickItem *>("manualLauncherHelp");
                auto *toggleHelp = window->findChild<QQuickItem *>("toggleManualLauncherHelp");
                QVERIFY(manualHelp && toggleHelp);
                QVERIFY(!manualHelp->isVisible());
                QVERIFY(toggleHelp->property("text").toString().startsWith("▶"));
                QVERIFY(QMetaObject::invokeMethod(toggleHelp, "clicked"));
                QVERIFY(manualHelp->isVisible());
                QVERIFY(flatpak->isVisible());
                QVERIFY(toggleHelp->property("text").toString().startsWith("▼"));
                QVERIFY(QMetaObject::invokeMethod(toggleHelp, "clicked"));
                QVERIFY(!manualHelp->isVisible());
                const auto *refresh = window->findChild<QQuickItem *>("configureMangoHudButton");
                QVERIFY(refresh);
            }
            QTest::qWait(100);
            const auto screenshot = qobject_cast<QQuickWindow *>(window)->grabWindow();
            QVERIFY(!screenshot.isNull());
            auto *other = window->findChild<QQuickItem *>(kind == "gaming" ? "widgetsPage" : "gamingPage");
            QVERIFY(other);
            QVERIFY(!other->isVisible());
            const auto output = qEnvironmentVariable(kind == "gaming" ? "GALLEON_GAMING_SCREENSHOT" : "GALLEON_WIDGETS_SCREENSHOT");
            if (!output.isEmpty()) QVERIFY(screenshot.save(output));
            const auto helpOutput = qEnvironmentVariable("GALLEON_GAMING_HELP_SCREENSHOT");
            if (kind == "gaming" && !helpOutput.isEmpty()) {
                auto *help = window->findChild<QQuickItem *>("linuxLauncherHelp");
                auto *contentItem = qvariant_cast<QQuickItem *>(widgets->property("contentItem"));
                QVERIFY(help && contentItem);
                widgets->setProperty("contentY", help->mapToItem(contentItem, QPointF()).y());
                QTest::qWait(100);
                QVERIFY(qobject_cast<QQuickWindow *>(window)->grabWindow().save(helpOutput));
                widgets->setProperty("contentY", 0);
            }

        }
        // A window that is not shown has no picture, but a shown one grabs: the pictures load.
        window->setProperty("currentTab", 0);
        auto *quick = qobject_cast<QQuickWindow *>(window);
        QVERIFY(quick);
        QVERIFY(!quick->grabWindow().isNull());
    }

    // ---- sharing themes (.gctheme) ----

    void aThemeExportedFromOneWindowCanBeInstalledInAnother()
    {
        const QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.filePath("source")));
        QVERIFY(QDir().mkpath(dir.filePath("target")));
        const QString source = dir.filePath("source/pages.json");
        QVERIFY(writeFile(source, R"({"version": 1, "home": "mine",
            "themes": [{"name": "Mine", "page": "mine", "color": "#336699"}],
            "pages": {"mine": {"keys": [{"key": 0, "label": "Next", "action": {"type": "media", "command": "next"}}]}}})"));
        bosun::BosunBackend from(source, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        const QVariantMap exported = from.exportTheme(0, dir.filePath("mine"), {{"author", "A. Person"}, {"license", "CC-BY-SA-4.0"}});
        QVERIFY2(exported.value("ok").toBool(), qPrintable(exported.value("errors").toStringList().join(';')));
        QCOMPARE(exported.value("file").toString(), dir.filePath("mine.gctheme")); // the extension is added

        const QString target = dir.filePath("target/pages.json");
        QVERIFY(writeFile(target, twoThemes()));
        bosun::BosunBackend into(target, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs"),
                                          .wallpaperRoot = dir.filePath("wallpapers")});
        const QSignalSpy installed(&into, &bosun::BosunBackend::themeInstalled);
        const QVariantMap result = into.importTheme(QUrl::fromLocalFile(exported.value("file").toString()).toString(), false);
        QVERIFY2(result.value("ok").toBool(), qPrintable(result.value("errors").toStringList().join(';')));
        QCOMPARE(result.value("themeIndex").toInt(), 2);
        QCOMPARE(into.themes().size(), 3);
        QCOMPARE(installed.size(), 1);
        QCOMPARE(installed.constFirst().at(0).toInt(), 2);
        QCOMPARE(installed.constFirst().at(1).toString(), QStringLiteral("mine"));
    }

    void aFileThatIsNotAThemeLeavesTheWindowAsItWas()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        const QString bad = dir.filePath("bad.gctheme");
        QVERIFY(writeFile(bad, "this is not a theme"));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        const QSignalSpy installed(&backend, &bosun::BosunBackend::themeInstalled);
        const QVariantMap result = backend.importTheme(bad, false);
        QVERIFY(!result.value("ok").toBool());
        QVERIFY(!result.value("errors").toStringList().isEmpty());
        QCOMPARE(backend.themes().size(), 2);
        QCOMPARE(installed.size(), 0);
    }

    void aThemeWithoutALicenseIsNotExported()
    {
        const QTemporaryDir dir;
        const QString file = dir.filePath("pages.json");
        QVERIFY(writeFile(file, twoThemes()));
        bosun::BosunBackend backend(file, {.ffprobe = "/nonexistent", .ffmpeg = "/nonexistent", .thumbnailFolder = dir.filePath("thumbs")});
        const QVariantMap result = backend.exportTheme(1, dir.filePath("music"), {{"author", "A. Person"}});
        QVERIFY(!result.value("ok").toBool());
        QVERIFY(!QFile::exists(dir.filePath("music.gctheme")));
    }
};

} // namespace

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    const QGuiApplication app(argc, argv);
    TestBosun test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_bosun.moc"
