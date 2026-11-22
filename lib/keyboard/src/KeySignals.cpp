// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "galleon/keyboard/KeySignals.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace galleon::keyboard {
namespace {

constexpr std::uint8_t scaled(std::uint8_t channel, double share)
{
    return static_cast<std::uint8_t>(std::clamp(std::lround(static_cast<double>(channel) * share), 0L, 255L));
}

} // namespace

double pulseShare(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0)
        return 1.0;
    const double breath = 0.5 + 0.5 * std::cos(2.0 * std::numbers::pi * seconds / PulsePeriodSeconds); // 1 at the start, 0 half a period later
    return PulseLowestShare + (1.0 - PulseLowestShare) * breath;
}

bool isValidClientName(const std::string &name)
{
    return !name.empty() && name.size() <= MaxClientNameBytes
           && std::all_of(name.begin(), name.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) <= 0x7E; });
}

SignalBoard::Result SignalBoard::set(const std::string &client, std::uint16_t led, Rgb colour, SignalEffect effect, double now, double seconds)
{
    if (!isValidClientName(client))
        return Result::BadClient;
    if (led >= LiveLedCount)
        return Result::BadLed;
    if (!std::isfinite(now) || !std::isfinite(seconds) || seconds < MinSignalSeconds || seconds > MaxSignalSeconds)
        return Result::BadSeconds;
    expire(now);
    const auto same = [&](const Entry &entry) { return entry.client == client && entry.led == led; };
    const auto existing = std::ranges::find_if(m_signals, same);
    if (existing == m_signals.end()) {
        const auto ofClient = std::ranges::count_if(m_signals, [&](const Entry &entry) { return entry.client == client; });
        if (ofClient == 0 && clientCount() >= static_cast<std::size_t>(MaxSignalClients))
            return Result::TooManyClients;
        if (ofClient >= MaxSignalsPerClient || m_signals.size() >= static_cast<std::size_t>(MaxSignalsTotal))
            return Result::TooManySignals;
    }
    const Entry entry{client, led, colour, effect, now, now + seconds, ++m_counter};
    if (existing == m_signals.end())
        m_signals.push_back(entry);
    else
        *existing = entry;
    return Result::Ok;
}

bool SignalBoard::clear(const std::string &client, std::uint16_t led)
{
    return std::erase_if(m_signals, [&](const Entry &entry) { return entry.client == client && entry.led == led; }) > 0;
}

bool SignalBoard::clear(const std::string &client)
{
    return std::erase_if(m_signals, [&](const Entry &entry) { return entry.client == client; }) > 0;
}

void SignalBoard::clearAll()
{
    m_signals.clear();
}

void SignalBoard::expire(double now)
{
    std::erase_if(m_signals, [now](const Entry &entry) { return entry.end <= now; });
}

std::size_t SignalBoard::clientCount() const
{
    std::vector<const std::string *> seen;
    for (const Entry &entry : m_signals) {
        if (std::ranges::none_of(seen, [&](const std::string *name) { return *name == entry.client; }))
            seen.push_back(&entry.client);
    }
    return seen.size();
}

std::vector<LedColor> SignalBoard::overlay(double now) const
{
    std::vector<const Entry *> winners; // one per LED: the one asked last
    for (const Entry &entry : m_signals) {
        if (entry.end <= now)
            continue;
        const auto at = std::ranges::find_if(winners, [&](const Entry *other) { return other->led == entry.led; });
        if (at == winners.end())
            winners.push_back(&entry);
        else if ((*at)->order < entry.order)
            *at = &entry;
    }
    std::ranges::sort(winners, [](const Entry *a, const Entry *b) { return a->led < b->led; });
    std::vector<LedColor> colours;
    colours.reserve(winners.size());
    for (const Entry *entry : winners) {
        const double share = entry->effect == SignalEffect::Pulse ? pulseShare(now - entry->start) : 1.0;
        colours.push_back({entry->led, scaled(entry->colour.red, share), scaled(entry->colour.green, share), scaled(entry->colour.blue, share)});
    }
    return colours;
}

LiveFrame withSignals(LiveFrame frame, std::span<const LedColor> signals, int level)
{
    const double share = static_cast<double>(std::clamp(level, 0, LevelFull)) / LevelFull;
    for (const LedColor &signal : signals) {
        if (signal.led >= LiveLedCount)
            continue; // never beyond the frame
        const std::size_t at = 2 + 3 * static_cast<std::size_t>(signal.led);
        frame.at(at) = scaled(signal.red, share);
        frame.at(at + 1) = scaled(signal.green, share);
        frame.at(at + 2) = scaled(signal.blue, share);
    }
    return frame;
}

} // namespace galleon::keyboard
