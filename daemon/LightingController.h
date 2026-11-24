// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "LightingEnvironment.h"
#include "LightingRestore.h"

#include <galleon/Transport.h>
#include <galleon/keyboard/LivePolicy.h>
#include <galleon/keyboard/Appearance.h>
#include <galleon/keyboard/KeyMap.h>
#include <galleon/keyboard/KeySignals.h>
#include <galleon/keyboard/ReactiveGlow.h>
#include <galleon/keyboard/LiveScene.h>

#include <QList>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>

#include <functional>
#include <utility>
#include <memory>
#include <optional>
#include <vector>

namespace galleond {

class LightingWorker;

// The live per-key lighting of the keyboard (part 1b1c:2b0c) as a component of galleond: EXPERIMENTAL and OFF. It is
// the keyboard half of the daemon, like KeyboardController, and shares nothing with the Stream Deck half
// (ScreenController, PagesController, TileRenderer, MprisController): it never calls them and they never call it.
// Exposed on D-Bus as org.galleoncore.Lighting (Linux).
//
// What is decided here (the library, galleon::keyboard::LiveLink, holds the protocol and the safety rules):
// - It does nothing until Enable(true). It does not switch itself on: whoever starts galleond may call Enable(true) at the start, with every refusal
//   below, only when the person asked for it AND galleond last ended properly with the lighting on (LightingRestore.h). The controller itself only
//   reports how it stands, through the hook of setRememberHook: it never reads that decision.
// - Enable(true) is REFUSED, and the keyboard is not touched, unless (1) the system tells us about sleep in time, and
//   (2) the watchdog process is running. The watchdog is started for galleond's own process ID before the keyboard is
//   opened. If it ends while the lighting is on, the lighting is stopped and the keys given back.
// - The blocking exchanges with the keyboard run on one worker thread, never in the device loop that sends the Stream
//   Deck's keep-alive. The worker owns its transport; the keys are given back, on that thread, before it ends.
// - The keys are given back on Enable(false), at exit (the destructor waits for it), before the machine sleeps, and
//   when the link stops by itself (two failures in a row, the keyboard gone): it then stays off until asked again.
// - While it is on, brightness requests of KeyboardController are paused by whoever wires the two (main.cpp).
// - After a sleep it takes the keys again, going through the 3 s of listening of the link.
// Nothing is saved to the keyboard's own memory and nothing is sent that LiveCommandGuard does not allow.
// The local time as hours, 0 to 24 (what the sky follows).
[[nodiscard]] double systemDayHours();

// Says that an appearance was accepted (what the keys will show, and what the sky needs to know: whether it follows the sun and from where). It is a
// separate object ON PURPOSE: the controller is put on the D-Bus with all its signals, and an appearance may hold a place (a latitude and a longitude,
// personal data), which must never be offered to the bus. Nothing of this object is ever registered there.
class AppearanceAnnouncer : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

Q_SIGNALS:
    void appearanceSet(const galleon::keyboard::Appearance &appearance);
};

class LightingController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.galleoncore.Lighting")

public:
    using Opener = std::function<std::unique_ptr<galleon::Transport>()>;
    using Clock = std::function<galleon::keyboard::LiveClock::time_point()>;

    // `watchdog` and `power` must outlive this object. `clock` is for tests; the default is the steady clock.
    LightingController(Opener opener, WatchdogGuard &watchdog, PowerEvents &power, Clock clock = {}, QObject *parent = nullptr);
    ~LightingController() override;

    // "off", "starting", "listening", "taking", "holding", "yielded", "sleeping".
    [[nodiscard]] QString state() const { return m_state; }
    // True from an accepted Enable(true) until the keys have been given back.
    [[nodiscard]] bool active() const { return m_active; }

    // Says how the lighting stands, for the memory that decides whether it is put back at the next start (LightingRestore.h): Running when an
    // Enable(true) was accepted; CleanExit when galleond ends (the destructor) with the lighting on and the keys confirmed back; Off for every other end
    // (switched off, failed, frozen, the watchdog ended, the release not confirmed). Called from the thread of the controller; it must be quick.
    using RememberHook = std::function<void(LightingMemory)>;
    void setRememberHook(RememberHook hook) { m_remember = std::move(hook); }

    // Says whether the computer is shutting down or the session is ending right now (Windows: SessionEnding.h). At the end of a session the system stops the
    // watchdog program together with everything else, and galleond sees it die before it is stopped itself: that is the end of a normal shutdown and not a failure of
    // the watchdog, so the lighting is stopped the same way (the keys are given back) but is remembered as a clean exit, and the next start puts it back. Without
    // this check, or while the session is not ending, a dead watchdog stays a failure (remembered as off). Called from the thread of the controller; it must be quick.
    using SessionEndingCheck = std::function<bool()>;
    void setSessionEndingCheck(SessionEndingCheck check) { m_sessionEnding = std::move(check); }
    // The system may announce the end of the session a moment AFTER the watchdog is stopped (seen on a real restart: the check was still false at the instant the
    // watchdog died). So when the watchdog dies and the check says no, the keys are still given back at once and the end is first remembered as a failure, and the
    // check is looked at again every `intervalMs`, `attempts` times: the first time it says yes the end is remembered as clean (only if the keys were confirmed back).
    // For tests; the defaults are 250 ms and 12 attempts (3 s).
    void setSessionEndGrace(int intervalMs, int attempts)
    {
        m_graceIntervalMs = intervalMs;
        m_graceAttempts = attempts;
    }

    // After waking up the keyboard may not be back on the bus yet (USB devices often take a moment): opening it is tried
    // again every ResumeRetryMs, at most ResumeAttempts times, before the lighting gives up and goes off. For tests.
    static constexpr int ResumeRetryMs = 1000;
    static constexpr int ResumeAttempts = 30;
    void setResumeRetry(int intervalMs, int attempts)
    {
        m_resumeRetryMs = intervalMs;
        m_resumeAttempts = attempts;
    }

    // The appearance of the lighting (a base, retouches, a level): played from now on while the lighting is on, and the first thing shown when it is
    // switched on. The scene of a tool (SetColor, SetLed, SetLeds) is let go. False, and nothing changed, for an appearance that is not valid. Where the
    // LEDs are: the positions built into galleond; a test may give others.
    bool setAppearance(const galleon::keyboard::Appearance &appearance);
    void setPositions(std::vector<galleon::keyboard::LedPosition> positions) { m_positions = std::move(positions); }
    [[nodiscard]] const galleon::keyboard::Appearance &appearance() const { return m_appearance; }
    // Says each time an appearance is accepted (for the sky, which needs the place). Not a signal of the controller: see AppearanceAnnouncer.
    [[nodiscard]] AppearanceAnnouncer &announcer() { return m_announcer; }
    // A level that takes the place of the appearance's own (the curve of the day, LightingLevelScheduler), or none to go back to it. Kept while the
    // lighting is off and over later appearances; the rainbow does not restart. False, and nothing changed, for a level outside 0 to 1000.
    bool setLevelOverride(std::optional<int> level);
    // Where the sky gets the hour of the day (0 to 24, the clock of the computer): a test gives another.
    void setDayHours(std::function<double()> dayHours) { m_dayHours = std::move(dayHours); }
    // When the sky changes (the real sunrise and sunset, or the fixed schedule of the mockup): called from the thread of the lighting, so it must be
    // safe to call from there. Given before the lighting is switched on.
    void setSkyDay(std::function<galleon::keyboard::SkyDay()> skyDay) { m_skyDay = std::move(skyDay); }
    // The hidden green rain (the easter egg) takes the keys' picture while it runs, and the appearance is shown again when it stops. It only starts when the
    // lighting is already on (the person switched it on: nothing is lit for it), is let go when the lighting is switched off, and is never saved.
    // False, and nothing done, when the lighting is off.
    bool setMatrixRain(bool on);
    [[nodiscard]] bool matrixRain() const { return m_matrix; }
    [[nodiscard]] int effectiveLevel() const { return m_night ? 0 : m_levelOverride.value_or(m_appearance.level); }

    // "Lights off until morning" (NightController.h) for the keyboard: while it lasts the keys show black WHATEVER the appearance, the level override (the curve of
    // the day) or a tool says, and they stay HELD by galleond with black pictures, as the lesson of Aaryush1 on #111 asks (stopping the program would let the keyboard
    // fall back to its own profile, which is lit). Only while the live lighting is on: with it off the keyboard shows its own profile, as always, and nothing here
    // switches it on. A lighting switched on during the night starts dark. The keyboard's own brightness property is never touched (it is kept inside the keyboard
    // across a replug). A tool's scene (SetColor, SetLed, SetLeds) is not shown during the night. Ending the night shows the appearance again.
    void setNight(bool night);
    [[nodiscard]] bool night() const { return m_night; }

    // The lighting put back when galleond starts (LightingRestore.h): the same as Enable(true), with every refusal, but a keyboard that is not on the bus yet
    // is waited for, the way it is after waking up (opening is tried again every ResumeRetryMs, at most ResumeAttempts times, then it stays off). A refusal is
    // returned at once and not retried: it will not change by waiting. By hand (Enable) nothing is waited for.
    QString restoreAtStart();

    // Key signals: a program asks for ONE key to be lit (KeySignals.h), so that another program can say "mail" or "meeting" on a key without ever touching
    // the keyboard. Declarative and write-only (AGENTS.md, security and privacy by design): a program says what to light and for how long, and never learns
    // anything about the keyboard, not even what it lit; there is no way to read a signal back, to list them or to tell which key was pressed. Only while the
    // live lighting is on (nothing is switched on for a signal), drawn over the appearance at the brightness in force (black at night), always with an end
    // (1 second to 24 hours), bounded in number, and let go when the lighting stops. `key` is the name of KeyIdentity.h ("F3", "Left Control"), the place of
    // the key on a QWERTY keyboard; a key whose LED has not been measured (the key map built into galleond) is refused, nothing is guessed.
    // "" when accepted, else why not (for a person to read). Not on the bus yet: this is the core the bus and the local IPC will call.
    QString setSignal(const QString &client, const QString &key, int red, int green, int blue, galleon::keyboard::SignalEffect effect, double seconds);
    // What a program asked for is taken away: one key, or all of its keys when `key` is empty. False when there was nothing to take away.
    bool clearSignals(const QString &client, const QString &key = {});
    // The reactive glow (ReactiveGlow.h): the key that goes down glows under the finger, then fades. It reads the key presses (KeyPressSource.h), so it is OFF
    // until the person chose it in Bosun (KeyGlowDriver.h decides when it runs). THESE ARE NOT SLOTS AND NOT SIGNALS, ON PURPOSE: nothing about the glow is on
    // the bus, and no program can ask which key was pressed or whether the glow is on (a test guards the names). Only while the live lighting is on.
    void setKeyGlow(bool on);
    [[nodiscard]] bool keyGlow() const { return m_glowOn; }
    // The colour of the glow, the person's choice (cyan until then). Kept while the lighting is off and handed to each worker when it starts.
    void setKeyGlowColour(galleon::keyboard::Rgb colour);
    [[nodiscard]] galleon::keyboard::Rgb keyGlowColour() const { return m_glowColour; }
    // A key went down on this LED (0 to 145). Ignored unless the glow is on and the lighting holds the keyboard. Nothing is kept here: the worker keeps the LED
    // and its time, bounded, for the length of the fade.
    void touchKeyGlow(std::uint16_t led);
    // The LED of a key, from the key map built into galleond (a key whose light was not measured has none): what the glow needs to turn a usage into a LED.
    [[nodiscard]] std::optional<std::uint16_t> ledForUsage(galleon::keyboard::HidUsage usage) const { return m_keyMap.ledForKey(usage); }
    // For tests: the key map used to find the LED of a key (the measured one built into galleond by default).
    void setKeyMap(galleon::keyboard::KeyMap map) { m_keyMap = map; }

public Q_SLOTS:
    // "" when accepted, else why not. Enable(false) gives the keys back (and waits for it) and is always accepted.
    QString Enable(bool on);
    // One colour for every key, 0 to 255 each; false when the lighting is off or a value is out of range.
    bool SetColor(int red, int green, int blue);
    // ONE lit LED (0 to 145, as in the live frame) with every other LED black, for the calibration of the key map: it shows
    // which key a LED belongs to. False when the lighting is off or a value is out of range. SetColor goes back to one colour
    // for every key. Output only: nothing here reads a key.
    bool SetLed(int led, int red, int green, int blue);
    // One colour PER LED, for the keyboard drawing of Bosun and the effects: `values` is a flat list of groups of four numbers, LED (0 to
    // 145), red, green, blue (0 to 255 each), at most one group per LED of the frame; every LED not listed is black. False when the
    // lighting is off, the list is empty, its length is not a multiple of four, it holds more than 146 groups or a value is out of
    // range: nothing is changed then. Output only.
    bool SetLeds(const QList<int> &values);
    [[nodiscard]] QString State() const { return m_state; }

Q_SIGNALS:
    void stateChanged(const QString &state, const QString &detail);
    void activeChanged(bool active);

private:
    bool showScene(const galleon::keyboard::LiveScene &scene);
    void startWorker();
    void sendSignals();
    [[nodiscard]] double signalSeconds() const;
    [[nodiscard]] galleon::keyboard::Appearance shownAppearance() const; // the appearance with the level in force
    bool stopWorker();
    void finish(const QString &detail, bool cleanExit = false);
    void remember(LightingMemory memory) const;
    void setState(const QString &state, const QString &detail);
    void onWorkerState(const QString &state, const QString &detail);

    Opener m_opener;
    RememberHook m_remember;
    SessionEndingCheck m_sessionEnding;
    QTimer m_graceTimer; // after a watchdog that died with no end of session announced yet: looks again
    int m_graceIntervalMs = 250;
    int m_graceAttempts = 12;
    int m_graceLeft = 0;
    bool m_releasedConfirmed = false; // the last finish() saw the keys confirmed back
    Clock m_clock;
    WatchdogGuard &m_watchdog;
    PowerEvents &m_power;
    QThread m_thread;
    std::unique_ptr<LightingWorker> m_worker; // moved to m_thread; handed over to it to be deleted there
    QString m_state = QStringLiteral("off");
    bool m_active = false;
    bool m_sleeping = false;
    QTimer m_resumeTimer; // single shot: the next try to open the keyboard after waking
    int m_resumeTriesLeft = 0;
    int m_resumeRetryMs = ResumeRetryMs;
    int m_resumeAttempts = ResumeAttempts;
    galleon::keyboard::Appearance m_appearance; // dark until something is asked for
    std::optional<int> m_levelOverride;
    AppearanceAnnouncer m_announcer; // not a child, and never put on the bus
    bool m_matrix = false; // the hidden rain is showing
    bool m_night = false;  // the keys are dark until the night ends
    std::function<double()> m_dayHours = [] { return systemDayHours(); };
    std::function<galleon::keyboard::SkyDay()> m_skyDay = [] { return galleon::keyboard::SkyDay{}; };
    std::optional<galleon::keyboard::LiveScene> m_tool; // what a tool shows over it, if one does
    std::vector<galleon::keyboard::LedPosition> m_positions;
    galleon::keyboard::SignalBoard m_signals; // only here; the worker gets copies
    galleon::keyboard::Rgb m_glowColour = galleon::keyboard::DefaultGlowColour;
    bool m_glowOn = false; // the glow is on (chosen in Bosun and running): handed to each worker when it starts
    galleon::keyboard::KeyMap m_keyMap;
};

} // namespace galleond
