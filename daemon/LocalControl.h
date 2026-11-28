// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <memory>
#include <QObject>
#include <QList>
#include <QString>
#include <optional>

namespace galleond {

enum class LocalAction : unsigned char { Ping, Wallpaper, Stop, Gaming, Theme, KeyboardBrightness, KeyboardBrightnessRead, LightingEnable, LightingColor, LightingLed, LightingLeds, KeySignal, KeySignalClear, KeySignalClearAll, NightStart, NightEnd, ScreensaverTest, Profile, ProfileStep};
struct LocalCommand
{
    LocalAction action = LocalAction::Ping;
    QString wallpaper;
    QString page{}; // NOLINT(readability-redundant-member-init): braced initializers elsewhere leave it out. Theme: the name of the page of the theme to show
    int brightness = 0; // KeyboardBrightness: 0 to 1000, as galleon::keyboard::BrightnessMax; the controller applies its own policy
    bool on = false;    // LightingEnable: switch the live lighting on or off
    int red = 0;        // LightingColor: one colour for every key, 0 to 255 each
    int green = 0;
    int blue = 0;
    QList<int> ledValues{}; // NOLINT(readability-redundant-member-init): braced initializers elsewhere leave it out. LightingLeds: one colour PER LED, as groups of four numbers (LED 0 to 145, red, green, blue), 1 to 146 groups
    int led = 0;        // LightingLed: ONE lit LED, 0 to 145 (the live frame), with red/green/blue; every other LED is black
    // KeySignal, KeySignalClear, KeySignalClearAll (other programs lighting single keys, KeySignalsService.h): who asks (the name it gives itself, 1 to 64 plain
    // characters), the key by its name ("F3", "Left Control": the shape is checked here, the name by the service), the effect ("steady" or "pulse") and how
    // long (1 to 86400 seconds), with red/green/blue. A signal is the program's own by that name: this endpoint cannot tell programs apart, so nothing here is
    // a defence between programs of the same person; the end of every signal is mandatory, and the person's choice in Bosun comes first.
    QString client{}; // NOLINT(readability-redundant-member-init)
    QString key{};    // NOLINT(readability-redundant-member-init)
    QString effect{}; // NOLINT(readability-redundant-member-init)
    double seconds = 0;
    // NightStart ("Lights off until morning", NightController.h): the time of the end, hour 0 to 23 and minute 0 to 59 (the controller finds the next such time, within 24 hours).
    int hour = 0;
    int minute = 0;
    // ScreensaverTest (the "Test" button of a screensaver video): the video as the pages file lists it. The daemon plays only a video of its own list, never a file it is given.
    QString video{}; // NOLINT(readability-redundant-member-init)
    // Profile: the name of the profile to go to (the shape is checked here, the file by the controller). ProfileStep: how many places on in the list of profiles
    // (-16 to 16, not 0), going round. The profile is the person's choice: nothing here reads the active window or a key press.
    QString profile{}; // NOLINT(readability-redundant-member-init)
    int steps = 0;
};

// One bounded JSON object per connection; the server accepts only this user.
[[nodiscard]] std::optional<LocalCommand> parseLocalCommand(const QByteArray &frame);
[[nodiscard]] QByteArray encodeLocalCommand(const LocalCommand &command);
[[nodiscard]] QString localControlName();

class LocalControl final : public QObject
{
    Q_OBJECT
public:
    explicit LocalControl(QObject *parent = nullptr);
    [[nodiscard]] bool listen(const QString &name);
    [[nodiscard]] QString errorString() const;
Q_SIGNALS:
    void wallpaperRequested(const QString &name);
    void stopRequested();
    void gamingRequested();
    void themeRequested(const QString &page);
    void profileRequested(const QString &name);
    void profileStepRequested(int steps);
    void keyboardBrightnessRequested(int value);
    void keyboardBrightnessReadRequested();
    void lightingEnableRequested(bool on);
    void lightingColorRequested(int red, int green, int blue);
    void lightingLedRequested(int led, int red, int green, int blue);
    void lightingLedsRequested(const QList<int> &values);
    // There is no reply on this endpoint: "ok" says the request was well formed, not that a key was lit (the person may not have allowed it, the lighting may be off).
    void keySignalRequested(const QString &client, const QString &key, int red, int green, int blue, const QString &effect, double seconds);
    void keySignalClearRequested(const QString &client, const QString &key);
    void keySignalClearAllRequested(const QString &client);
    // The veille tab of Bosun on Windows. No reply either: a night that was taken shows in night-mode.json, which Bosun reads; a refused test (a game runs, the module is not there) shows nothing.
    void nightStartRequested(int hour, int minute);
    void nightEndRequested();
    void screensaverTestRequested(const QString &video);
private:
    void acceptConnections();
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
    QString m_error;
    int m_connections = 0;
};

// Event-loop driven and bounded by a deadline, so Bosun never blocks on IPC.
void sendLocalCommand(const QString &name, const LocalCommand &command, QObject *owner);

} // namespace galleond
