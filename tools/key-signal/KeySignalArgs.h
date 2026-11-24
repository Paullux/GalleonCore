// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace keysignal {

// What galleon-key-signal was asked to do. Pure: the arguments are read and checked here, nothing is sent (docs/KEY_SIGNALS.md describes the requests).
enum class Verb : unsigned char { Light, Clear, ClearAll, Help };

struct Arguments
{
    Verb verb = Verb::Light;
    QString key;     // the name of the physical key as KeyIdentity.h names it ("F3"): its place on a QWERTY keyboard
    int red = 0;
    int green = 0;
    int blue = 0;
    bool pulse = false;
    int seconds = 0; // 1 to 86400
    // Windows only: the endpoint cannot tell programs apart, so a signal belongs to a name that the program gives itself (docs/KEY_SIGNALS.md).
    QString client = QStringLiteral("galleon-key-signal");
};

inline constexpr int LongestSeconds = 24 * 60 * 60;

struct ParseResult
{
    std::optional<Arguments> arguments;
    QString error; // for a person to read, when there are no arguments
};

// `args` are the arguments after the name of the program. `windows` says which system the tool runs on: on Linux a signal belongs to the D-Bus connection that asked for it
// and goes when that connection closes, so the tool holds it until its time is over or it is stopped (Ctrl-C), and there is no `--clear` (another run cannot take it away);
// on Windows the request is sent and the tool ends, and `--clear`, `--clear-all` and `--client` exist.
[[nodiscard]] ParseResult parseArguments(const QStringList &args, bool windows);

// The text of --help.
[[nodiscard]] QString usage(bool windows);

} // namespace keysignal
