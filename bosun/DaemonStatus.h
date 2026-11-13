// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#ifdef Q_OS_WIN
#include <QTimer>
#include <QLocalSocket>
#include "KeyboardStateWatcher.h"
#else
#include <QDBusServiceWatcher>
#endif

namespace bosun {

// Reports whether galleond is available and requests its lifecycle operations.
// Linux uses the session bus and systemd; Windows uses a per-user local endpoint
// and starts the sibling executable. This window never opens the device.
class DaemonStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    // The keyboard's brightness as galleond last reported it, as a stop of the slider (0 to 1000, steps of 100),
    // or -1 while unknown (not read yet, unreadable, or no way back from the daemon on this system).
    Q_PROPERTY(int keyboardBrightness READ keyboardBrightness NOTIFY keyboardBrightnessChanged)
    // The experimental live lighting of galleond (org.galleoncore.Lighting). `lightingSupported` is false where the daemon
    // cannot run it (Windows for now). `lightingState` is "off", "starting", "listening", "taking", "holding", "yielded" or
    // "sleeping"; `lightingDetail` says why when it is off or yielded, or why a request was refused. This window only asks
    // galleond: it never opens the keyboard.
    Q_PROPERTY(bool lightingSupported READ lightingSupported CONSTANT)
    Q_PROPERTY(QString lightingState READ lightingState NOTIFY lightingChanged)
    Q_PROPERTY(QString lightingDetail READ lightingDetail NOTIFY lightingChanged)
    Q_PROPERTY(bool lightingActive READ lightingActive NOTIFY lightingChanged)

public:
    static constexpr const char *BusName = "org.galleoncore.Deck";
    static constexpr const char *ServiceName = "galleond.service";

    // Empty selects systemctl on Linux or the sibling galleond.exe on Windows.
    // Tests can supply a fake program.
    explicit DaemonStatus(QString systemctl = {}, QObject *parent = nullptr);
#ifdef Q_OS_WIN
    // An explicit endpoint lets tests use a private fake daemon.
    DaemonStatus(QString program, QString endpoint, QObject *parent);
#endif

    [[nodiscard]] bool running() const { return m_running; }
    [[nodiscard]] int keyboardBrightness() const { return m_keyboardBrightness; }
    [[nodiscard]] static bool lightingSupported();
    [[nodiscard]] QString lightingState() const { return m_lightingState; }
    [[nodiscard]] QString lightingDetail() const { return m_lightingDetail; }
    // Anything but "off": galleond holds the keyboard's lighting, or is about to, or will again after a sleep.
    [[nodiscard]] bool lightingActive() const { return m_lightingState != QStringLiteral("off"); }

    // Pure: a value read from the keyboard (0 to 1000, possibly set by another program to something that is not a
    // step of 100) as the nearest stop of the slider, or -1 when it is outside 0..1000.
    [[nodiscard]] static int nearestBrightnessStop(int value);

    // The arguments given to systemctl to start or stop the service of the user. Pure.
    [[nodiscard]] static QStringList userCommand(const QString &verb);

    // "" when the request was sent (the answer is the name appearing on the bus), else why not.
    Q_INVOKABLE [[nodiscard]] QString start();
    Q_INVOKABLE [[nodiscard]] QString stop();
    Q_INVOKABLE [[nodiscard]] QString showGaming();
    // Shows the theme with this page name on the module; "" when the request was sent, else why not.
    Q_INVOKABLE [[nodiscard]] QString showTheme(const QString &page);
    // Asks galleond to set the keyboard's own brightness (0 to 1000, steps of 100). The write is made by the
    // daemon, never here; "" when the request was sent, else why not. The result of the write comes later.
    Q_INVOKABLE [[nodiscard]] QString setKeyboardBrightness(int value);
    // Asks galleond to read the value the keyboard holds now; the answer arrives as keyboardBrightnessChanged.
    // Linux: the answer is a D-Bus signal. Windows: the endpoint has no reply, so galleond publishes the value in
    // keyboard-state.json, which this object watches.
    Q_INVOKABLE [[nodiscard]] QString readKeyboardBrightness();
    // Switches the live lighting on or off in galleond. "" when the request was sent, else why not (the daemon is not
    // running, or this system cannot). The daemon's own refusal (no watchdog, no sleep events) comes back as
    // lightingDetail. Switching off gives the keys back before it returns on the daemon's side.
    Q_INVOKABLE [[nodiscard]] QString enableLighting(bool on);
    // One colour for every key, 0 to 255 each; "" when sent, else why not.
    Q_INVOKABLE [[nodiscard]] QString setLightingColor(int red, int green, int blue);
    // Asks galleond for its lighting state (when it appears, or to look again).
    Q_INVOKABLE [[nodiscard]] QString refreshLighting();

Q_SIGNALS:
    void changed();
    void keyboardBrightnessChanged();
    void lightingChanged();
    void commandError(const QString &message);

private Q_SLOTS:
    // Answers of galleond's keyboard object (D-Bus signals; never reached on Windows).
    void onBrightnessRead(int value, bool ok, const QString &reason);
    void onBrightnessWritten(int value, bool ok, const QString &reason);
    void onLightingStateChanged(const QString &state, const QString &detail);

private:
    void setKeyboardBrightnessKnown(int value);
    void setLighting(const QString &state, const QString &detail);
    [[nodiscard]] QString ask(const QString &verb);
    void look();

    QString m_systemctl;
#ifdef Q_OS_WIN
    void setRunning(bool running);
    QString m_endpoint;
    QTimer m_poll;
    QLocalSocket m_probe;
    KeyboardStateWatcher *m_keyboardState = nullptr; // child of this: the way the keyboard's value comes back
#else
    QDBusServiceWatcher m_watcher;
#endif
    bool m_running = false;
    int m_keyboardBrightness = -1;
    QString m_lightingState = QStringLiteral("off");
    QString m_lightingDetail;
};

} // namespace bosun
