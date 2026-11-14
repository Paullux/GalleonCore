// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LocalControl.h"
#include <QtTest>
#include <QLocalSocket>
#include <QUuid>
#include <memory>

using namespace galleond;
class TestLocalControl : public QObject
{
    Q_OBJECT
private slots:
    void refusesUnexpectedCommands()
    {
        for (const QByteArray &frame : {QByteArray{}, QByteArray("[]"), QByteArray(R"({"action":"exec"})"),
                 QByteArray(R"({"action":"stop","extra":true})"), QByteArray(R"({"action":"wallpaper","name":1})"),
                 QByteArray(R"({"action":"wallpaper","name":""})"), QByteArray(4097, 'x')})
            QVERIFY(!parseLocalCommand(frame).has_value());
    }
    void gamingCommandSelectsOnlyTheGamingPage()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::Gaming, {}}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::Gaming);
        QVERIFY(!parseLocalCommand(R"({"action":"gaming","page":"arbitrary"})"));
        const QString name = QStringLiteral("galleon-gaming-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::gamingRequested);
        sendLocalCommand(name, {LocalAction::Gaming, {}}, &server);
        QTRY_COMPARE(requested.size(), 1);
    }
    void themeCommandCarriesOnlyAPageName()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::Theme, {}, QStringLiteral("discord-2")}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::Theme);
        QCOMPARE(command.value_or(LocalCommand{}).page, QStringLiteral("discord-2"));
        for (const QByteArray &frame : {QByteArray(R"({"action":"theme"})"), QByteArray(R"({"action":"theme","page":1})"),
                 QByteArray(R"({"action":"theme","page":"../etc"})"), QByteArray(R"({"action":"theme","page":"Upper"})"),
                 QByteArray(R"({"action":"theme","page":""})"), QByteArray(R"({"action":"theme","page":"a","name":"b"})")})
            QVERIFY(!parseLocalCommand(frame).has_value());
        const QString name = QStringLiteral("galleon-theme-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::themeRequested);
        sendLocalCommand(name, {LocalAction::Theme, {}, QStringLiteral("mine")}, &server);
        QTRY_COMPARE(requested.size(), 1);
        QCOMPARE(requested.constFirst().constFirst().toString(), QStringLiteral("mine"));
    }
    void keyboardBrightnessCommandCarriesOnlyAWholeNumberFromZeroToAThousand()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::KeyboardBrightness, {}, {}, 600}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::KeyboardBrightness);
        QCOMPARE(command.value_or(LocalCommand{}).brightness, 600);
        for (const QByteArray &frame : {QByteArray(R"({"action":"keyboard-brightness"})"), QByteArray(R"({"action":"keyboard-brightness","value":"5"})"),
                 QByteArray(R"({"action":"keyboard-brightness","value":-1})"), QByteArray(R"({"action":"keyboard-brightness","value":1001})"),
                 QByteArray(R"({"action":"keyboard-brightness","value":3.5})"), QByteArray(R"({"action":"keyboard-brightness","value":1,"x":2})")})
            QVERIFY(!parseLocalCommand(frame).has_value());
        const QString name = QStringLiteral("galleon-brightness-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::keyboardBrightnessRequested);
        sendLocalCommand(name, {LocalAction::KeyboardBrightness, {}, {}, 300}, &server);
        QTRY_COMPARE(requested.size(), 1);
        QCOMPARE(requested.constFirst().constFirst().toInt(), 300);
    }
    void theLightingCommandsRoundTripThroughTheirEncoding()
    {
        LocalCommand enable{LocalAction::LightingEnable, {}, {}};
        enable.on = true;
        const auto parsed = parseLocalCommand(encodeLocalCommand(enable));
        QVERIFY(parsed);
        QCOMPARE(parsed->action, LocalAction::LightingEnable);
        QVERIFY(parsed->on);
        LocalCommand colour{LocalAction::LightingColor, {}, {}};
        colour.red = 10;
        colour.green = 20;
        colour.blue = 255;
        const auto parsedColour = parseLocalCommand(encodeLocalCommand(colour));
        QVERIFY(parsedColour);
        QCOMPARE(std::tuple(parsedColour->red, parsedColour->green, parsedColour->blue), std::tuple(10, 20, 255));
    }

    void aLightingCommandWithAnythingOutOfShapeIsRefused()
    {
        for (const char *frame : {R"({"action":"lighting-enable"})", R"({"action":"lighting-enable","on":1})", R"({"action":"lighting-enable","on":"true"})",
                                  R"({"action":"lighting-enable","on":true,"x":1})",
                                  R"({"action":"lighting-color","red":1,"green":2})", R"({"action":"lighting-color","red":256,"green":0,"blue":0})",
                                  R"({"action":"lighting-color","red":-1,"green":0,"blue":0})", R"({"action":"lighting-color","red":1.5,"green":0,"blue":0})",
                                  R"({"action":"lighting-color","red":"1","green":0,"blue":0})",
                                  R"({"action":"lighting-color","red":1,"green":2,"blue":3,"extra":4})"})
            QVERIFY2(!parseLocalCommand(frame), frame);
    }

    void aKeyboardBrightnessReadCarriesNothingElse()
    {
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::KeyboardBrightnessRead, {}, {}}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::KeyboardBrightnessRead);
        QVERIFY(!parseLocalCommand(QByteArray(R"({"action":"keyboard-brightness-read","value":5})")).has_value());
        const QString name = QStringLiteral("galleon-brightness-read-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server; QVERIFY(server.listen(name));
        const QSignalSpy requested(&server, &LocalControl::keyboardBrightnessReadRequested);
        sendLocalCommand(name, {LocalAction::KeyboardBrightnessRead, {}, {}}, &server);
        QTRY_COMPARE(requested.size(), 1);
    }
    void roundTripWallpaper()
    {
        const QString name = QString::fromUtf8("personal/caf\xc3\xa9.jpg");
        const auto command = parseLocalCommand(encodeLocalCommand({LocalAction::Wallpaper, name}));
        QVERIFY(command.has_value());
        QCOMPARE(command.value_or(LocalCommand{}).wallpaper, name);
        QCOMPARE(command.value_or(LocalCommand{}).action, LocalAction::Wallpaper);
    }
    void appliesOneCommandAndLeavesSecondInstanceAlone()
    {
        const QString name = QStringLiteral("galleon-test-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server;
        QVERIFY(server.listen(name));
        LocalControl second;
        QVERIFY(!second.listen(name));
        const auto requests = std::make_unique<QSignalSpy>(&server, &LocalControl::wallpaperRequested);
        sendLocalCommand(name, {LocalAction::Wallpaper, QStringLiteral("personal/example.jpg")}, &server);
        QTRY_COMPARE(requests->size(), 1);
        QCOMPARE(requests->first().first().toString(), QStringLiteral("personal/example.jpg"));
    }
    void invalidInputCannotStopDaemon()
    {
        const QString name = QStringLiteral("galleon-test-") + QUuid::createUuid().toString(QUuid::Id128);
        LocalControl server;
        QVERIFY(server.listen(name));
        const auto stops = std::make_unique<QSignalSpy>(&server, &LocalControl::stopRequested);
        QLocalSocket client;
        client.connectToServer(name);
        QVERIFY(client.waitForConnected());
        client.write("{\"action\":\"stop\",\"extra\":true}\n");
        QTRY_COMPARE(client.state(), QLocalSocket::UnconnectedState);
        QCOMPARE(stops->size(), 0);
    }
};
QTEST_GUILESS_MAIN(TestLocalControl)
#include "test_localcontrol.moc"
