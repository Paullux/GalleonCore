// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

namespace galleond {

// Where the live lighting stands, published by galleond in a small file that Bosun watches: the way back on Windows,
// whose local endpoint has no reply (Linux also has the D-Bus signal). Written by galleond only; read by Bosun, which
// never opens the keyboard. Like keyboard-state.json, the file is untrusted input for its reader: a bounded size,
// exactly the four keys below, a state from the known list and a short plain detail, or nothing at all.
//   {"version":1,"state":"holding","detail":"","time":1783938123}
// `state` is one of the names of LightingController ("off", "starting", "listening", "taking", "holding", "yielded",
// "sleeping"); `detail` says why it is off or yielded (at most MaxLightingDetail characters, no control characters);
// `time` is when galleond wrote it, in seconds since 1970, so a reader can ignore a file of an earlier session.
struct LightingState
{
    QString state;
    QString detail;
    qint64 time = 0;
    friend bool operator==(const LightingState &, const LightingState &) = default;
};

inline constexpr qint64 MaxLightingStateBytes = 4096;
inline constexpr int MaxLightingDetail = 300;

[[nodiscard]] bool isKnownLightingState(const QString &state);
[[nodiscard]] QByteArray encodeLightingState(const LightingState &state);
[[nodiscard]] std::optional<LightingState> parseLightingState(const QByteArray &bytes);
[[nodiscard]] bool isFreshLightingState(const LightingState &state, qint64 notBefore);

// ~/.config/galleoncore/lighting-state.json (the config folder of the platform), next to keyboard-state.json.
[[nodiscard]] QString lightingStatePath();
// Atomic (written aside, then renamed). Creates the folder. False on failure.
[[nodiscard]] bool writeLightingState(const QString &path, const LightingState &state);
// Nothing for a missing, unreadable, oversized or invalid file.
[[nodiscard]] std::optional<LightingState> readLightingState(const QString &path);

} // namespace galleond
