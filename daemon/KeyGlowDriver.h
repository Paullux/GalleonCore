// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KeyPressSource.h"

#include <galleon/keyboard/KeyIdentity.h>

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <optional>

namespace galleond {

// What the glow needs of the lighting, and nothing more: whether the live lighting is on, to turn the glow on and off, to say that a key went down on a LED, and the
// LED of a key. The lighting controller is adapted to it in main.cpp (LightingController.h: none of this is on the bus); a test gives a fake.
class KeyGlowTarget
{
public:
    virtual ~KeyGlowTarget() = default;
    [[nodiscard]] virtual bool lightingOn() const = 0;
    virtual void setGlow(bool on) = 0;
    virtual void touch(std::uint16_t led) = 0;
    [[nodiscard]] virtual std::optional<std::uint16_t> ledForUsage(galleon::keyboard::HidUsage usage) const = 0;
};

// Decides WHEN the keys are watched, for the reactive glow, and carries what the source says to the lighting. It is the one place that starts the source of key
// presses (KeyPressSource.h), under the rules of AGENTS.md ("Security and privacy by design"):
//   - the source is watched ONLY while (1) the person chose the effect (the file written by Bosun, read again every second, so that unticking the box stops it
//     within the second) AND (2) the live lighting is on. Either one going away stops it, and the glow is switched off with it. Nothing starts it otherwise.
//   - what the source says (the usage of a key that went down) goes to the lighting at once, as the LED of that key, and is not kept here: no key, no order, no
//     count, no history, no log. A key whose light was not measured has no LED and makes no glow.
//   - while the source is watched, `watching(true)` is said (main.cpp pauses the diagnostic trace of the lighting then, since a frame that lights a key at a given
//     moment would reveal the press); `watching(false)` is said a little after it stopped, once the last glowing frame has gone.
//   - when the source cannot read (no node, no access, the keyboard gone) it is tried again a few seconds later, only while it is still wanted.
class KeyGlowDriver : public QObject
{
    Q_OBJECT

public:
    using Allowed = std::function<bool()>;
    using WatchingHook = std::function<void(bool)>;

    static constexpr int PollMs = 1000;           // how often the choice is read again
    static constexpr int RetryAfterFailureMs = 5000; // before a source that could not read is tried again
    static constexpr int TraceResumeMs = 2000;    // the diagnostic trace resumes this long after the watch ended (a frame may still be in flight)

    // `source` and `target` must outlive the driver. `allowed` reads the choice of the person (the file), and returns false for anything but a clear yes.
    KeyGlowDriver(KeyPressSource &source, KeyGlowTarget &target, Allowed allowed, QObject *parent = nullptr);
    ~KeyGlowDriver() override;

    void setWatchingHook(WatchingHook hook) { m_hook = std::move(hook); }
    // For tests.
    void setTimes(int pollMs, int retryAfterFailureMs, int traceResumeMs)
    {
        m_pollMs = pollMs;
        m_retryMs = retryAfterFailureMs;
        m_traceResumeMs = traceResumeMs;
        m_poll.setInterval(pollMs);
    }

    [[nodiscard]] bool watching() const { return m_source.watching(); }

    // The decision, pure: the keys are watched only when the person chose the effect AND the live lighting is on.
    [[nodiscard]] static bool shouldWatch(bool chosen, bool lightingOn) { return chosen && lightingOn; }

    // Looks again at the choice and at the lighting and starts or stops the watch. Called every PollMs, and by whoever sees the lighting go on or off.
    void refresh();

private:
    void startWatching();
    void stopWatching();
    void onKeyDown(quint16 usage);
    void onUnavailable();
    void say(bool watching);

    KeyPressSource &m_source;
    KeyGlowTarget &m_target;
    Allowed m_allowed;
    WatchingHook m_hook;
    QTimer m_poll;
    QTimer m_resume; // single shot: the trace resumes after the watch ended
    int m_pollMs = PollMs;
    int m_retryMs = RetryAfterFailureMs;
    int m_traceResumeMs = TraceResumeMs;
    qint64 m_retryNotBefore = 0; // milliseconds of m_clock below which a failed source is not tried again
    QElapsedTimer m_clock;
    bool m_said = false; // what the hook was last told
};

} // namespace galleond
