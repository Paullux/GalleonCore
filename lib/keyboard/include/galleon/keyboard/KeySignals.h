// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "galleon/keyboard/Appearance.h"
#include "galleon/keyboard/LiveFrame.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// "Signals": a program other than galleond asks for ONE key to be lit, in a colour, steady or pulsing, for a limited time ("F3 pulses amber until
// cleared"), and galleond is still the only writer of the keyboard. Asked for by Aaryush1 on #111, whose own helper writes the keyboard today and so
// cannot run beside galleond. This file is the pure model: the board of what is asked, and how it is drawn over a frame. Nothing is sent, read or
// saved here, and the time is an argument.
//
// What the privacy rules of AGENTS.md ("Security and privacy by design") make of this:
//   - It is a DECLARATIVE, WRITE-ONLY model. A client says "this key, this colour, this effect, this long". It never learns which keys were pressed,
//     and nothing here reads a key press. There is no function that returns what a key shows or what any client asked (rule 8: no per-key state that
//     lets another program reconstruct what was typed).
//   - It is BOUNDED: a few clients, a few lit each, a time limit on every signal, so that a program that crashes or misbehaves cannot light the
//     keyboard for ever or grow the board without limit (ROADMAP.md "Maintaining architecture": long-lived state must be bounded).
//   - A key is a LED of the live frame here (0 to 145); turning a key name into its LED is the caller's (the key map of galleond).
namespace galleon::keyboard {

enum class SignalEffect : std::uint8_t {
    Steady, // the colour as it is
    Pulse   // the colour breathes between full and a fifth of it, one breath every PulsePeriodSeconds, from full at the moment it was asked for
};

inline constexpr int MaxSignalClients = 8;
inline constexpr int MaxSignalsPerClient = 16;
inline constexpr int MaxSignalsTotal = 64;
inline constexpr std::size_t MaxClientNameBytes = 64;
inline constexpr double MinSignalSeconds = 1.0;
inline constexpr double MaxSignalSeconds = 24.0 * 60.0 * 60.0;
inline constexpr double PulsePeriodSeconds = 1.6;
inline constexpr double PulseLowestShare = 0.2;

// How much of the colour a pulse shows `seconds` after it was asked for: 1 at the start, PulseLowestShare half a period later, back to 1 after a period. Pure.
[[nodiscard]] double pulseShare(double seconds);

// A name that identifies a client: 1 to MaxClientNameBytes bytes, printable ASCII only (no control character, no byte beyond 0x7E). Pure.
[[nodiscard]] bool isValidClientName(const std::string &name);

class SignalBoard
{
public:
    enum class Result : std::uint8_t {
        Ok,
        BadClient,      // not a valid name
        BadLed,         // beyond the 146 LEDs of the frame
        BadSeconds,     // not a number from MinSignalSeconds to MaxSignalSeconds
        TooManyClients, // a new client would be the ninth
        TooManySignals  // the client has MaxSignalsPerClient, or the board MaxSignalsTotal
    };

    // Lights `led` for `seconds` from `now`. The same client asking again for the same LED replaces its signal (and gives it the new end and a new
    // start of the pulse); another client asking for a LED that a client already lights is allowed: the one asked LAST is the one shown. A refused
    // request changes nothing. Signals that have ended by `now` are dropped first, so they never count against the limits.
    [[nodiscard]] Result set(const std::string &client, std::uint16_t led, Rgb colour, SignalEffect effect, double now, double seconds);

    // Takes away one signal of a client, or all of them. False when there was nothing to take away.
    bool clear(const std::string &client, std::uint16_t led);
    bool clear(const std::string &client);
    void clearAll();

    // Drops the lit that have ended by `now`.
    void expire(double now);

    [[nodiscard]] bool empty() const { return m_signals.empty(); }
    [[nodiscard]] std::size_t size() const { return m_signals.size(); }
    [[nodiscard]] std::size_t clientCount() const;

    // The colour of each LED that carries a signal at `now` (the one asked last where there are several), already moved by its effect. Signals that
    // have ended are left out. At most one entry per LED, by increasing LED.
    [[nodiscard]] std::vector<LedColor> overlay(double now) const;

    friend bool operator==(const SignalBoard &, const SignalBoard &) = default;

private:
    struct Entry
    {
        std::string client;
        std::uint16_t led = 0;
        Rgb colour;
        SignalEffect effect = SignalEffect::Steady;
        double start = 0;
        double end = 0;
        std::uint64_t order = 0; // the later it was asked for, the higher

        friend bool operator==(const Entry &, const Entry &) = default;
    };
    std::vector<Entry> m_signals;
    std::uint64_t m_counter = 0;
};

// The frame with the lit drawn over it: each LED of `lit` replaces the frame's colour, scaled by `level` (0 to LevelFull: the brightness of the
// lighting, so that a signal is as dim as the keyboard is asked to be, and dark when the level is 0, as in the night). Pure.
[[nodiscard]] LiveFrame withSignals(LiveFrame frame, std::span<const LedColor> lit, int level);

} // namespace galleon::keyboard
