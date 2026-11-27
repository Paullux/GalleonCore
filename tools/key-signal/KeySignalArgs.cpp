// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeySignalArgs.h"

#include <QRegularExpression>

#include <algorithm>
#include <map>

namespace keysignal {
namespace {

// 1 to `longest` plain printable ASCII characters.
bool plainText(const QString &text, int longest)
{
    if (text.isEmpty() || text.size() > longest)
        return false;
    return std::ranges::all_of(text, [](QChar c) { return c.unicode() >= 0x20 && c.unicode() <= 0x7E; });
}

// A whole number written with digits only (no sign, no space, no decimal point), within bounds.
std::optional<int> wholeNumber(const QString &text, int low, int high)
{
    static const QRegularExpression digits(QStringLiteral("^[0-9]{1,6}$"));
    if (!digits.match(text).hasMatch())
        return std::nullopt;
    const int value = text.toInt();
    if (value < low || value > high)
        return std::nullopt;
    return value;
}

ParseResult failure(const QString &error)
{
    return {std::nullopt, error};
}

// The name without the plain spaces at its ends, and nothing else taken off: a tab or a line break around a name is not a space, and the name is then refused.
QString withoutSpacesAround(const QString &text)
{
    qsizetype first = 0;
    qsizetype last = text.size();
    while (first < last && text.at(first) == QLatin1Char(' '))
        ++first;
    while (last > first && text.at(last - 1) == QLatin1Char(' '))
        --last;
    return text.mid(first, last - first);
}

// "F3", or several names separated by commas ("F3, F4,Left Control"): the spaces around a name are not part of it. Each name is 1 to 32 plain characters, an empty
// name ("F3,,F4", a comma at the end) is refused, a name given twice (whatever its case) is refused, and there are at most MaxKeysPerCall of them. `error` says why not.
std::optional<QStringList> keyNames(const QString &text, QString *error)
{
    QStringList names;
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() > MaxKeysPerCall) {
        *error = QStringLiteral("--key names at most %1 keys").arg(MaxKeysPerCall);
        return std::nullopt;
    }
    for (const QString &part : parts) {
        const QString name = withoutSpacesAround(part);
        if (!plainText(name, 32)) {
            *error = parts.size() > 1 ? QStringLiteral("--key is the name of a key, or several separated by commas: each is 1 to 32 plain characters, none empty")
                                      : QStringLiteral("--key is the name of a key: 1 to 32 plain characters");
            return std::nullopt;
        }
        const bool again = std::ranges::any_of(names, [&name](const QString &known) { return known.compare(name, Qt::CaseInsensitive) == 0; });
        if (again) {
            *error = QStringLiteral("--key names %1 twice").arg(name);
            return std::nullopt;
        }
        names.append(name);
    }
    return names;
}

} // namespace

QString usage(bool windows)
{
    QString text = QStringLiteral(
        "galleon-key-signal: lights one key, or several, of the Galleon 100 SD the way another program does (see docs/KEY_SIGNALS.md).\n"
        "It needs the box \"Other programs may light keys\" ticked in Bosun and the live lighting on.\n\n"
        "  galleon-key-signal --key F3 --color #ff7800 --effect pulse --seconds 600\n"
        "  galleon-key-signal --key F3 --red 255 --green 120 --blue 0 --seconds 60\n"
        "  galleon-key-signal --key F3,F4,F5 --color #ff7800 --seconds 60\n\n"
        "Options:\n"
        "  --key NAME        the key, by its place on a QWERTY keyboard (F3, Left Control, W for the key engraved Z on an AZERTY one);\n"
        "                    several keys together, separated by commas (F3,F4,F5), up to 16, all with the same colour, effect and time\n"
        "  --color #rrggbb   its colour; or --red, --green and --blue, each 0 to 255 (all three)\n"
        "  --effect steady|pulse   steady (default) or breathing (a cycle of about 1.6 s)\n"
        "  --seconds N       how long, 1 to 86400 (required)\n"
        "  --help            this text\n");
    if (windows) {
        text += QStringLiteral(
            "  --clear           takes away the signal of each key of --key (same --client)\n"
            "  --clear-all       takes away every signal of --client\n"
            "  --client NAME     the name the signals belong to (default galleon-key-signal), 1 to 64 plain characters\n\n"
            "On Windows the request is sent and the tool ends: the endpoint gives no reply about the lighting, so a refusal (the box not ticked,\n"
            "the live lighting off) shows nothing. The signal lasts until its end or a clear.\n");
    } else {
        text += QStringLiteral(
            "\nOn Linux a signal belongs to the connection that asked for it: the tool stays running while the key is lit, and takes the signal\n"
            "away when its time is over or when it is stopped (Ctrl-C, kill). There is no clear verb: stop the tool.\n"
            "Exit status: 0 when it ended as asked, 1 for a wrong command line, 2 when galleond refused or could not be reached.\n");
    }
    return text;
}

ParseResult parseArguments(const QStringList &args, bool windows)
{
    Arguments result;
    std::map<QString, QString> values; // option -> value, each once
    bool help = false;
    bool clear = false;
    bool clearAll = false;
    static const QStringList withValue{QStringLiteral("--key"), QStringLiteral("--color"), QStringLiteral("--red"), QStringLiteral("--green"),
                                       QStringLiteral("--blue"), QStringLiteral("--effect"), QStringLiteral("--seconds"), QStringLiteral("--client")};
    for (qsizetype i = 0; i < args.size(); ++i) {
        QString option = args.at(i);
        QString value;
        bool hasValue = false;
        if (option.startsWith(QStringLiteral("--")) && option.contains(QLatin1Char('='))) {
            value = option.mid(option.indexOf(QLatin1Char('=')) + 1);
            option = option.left(option.indexOf(QLatin1Char('=')));
            hasValue = true;
        }
        if (option == QStringLiteral("--help") || option == QStringLiteral("-h")) {
            if (hasValue)
                return failure(QStringLiteral("--help takes no value"));
            help = true;
        } else if (option == QStringLiteral("--clear") || option == QStringLiteral("--clear-all")) {
            if (!windows)
                return failure(QStringLiteral("%1 does not exist on Linux: a signal belongs to the connection that asked for it, so stop the tool that lights it (Ctrl-C)").arg(option));
            if (hasValue)
                return failure(QStringLiteral("%1 takes no value").arg(option));
            (option == QStringLiteral("--clear") ? clear : clearAll) = true;
        } else if (withValue.contains(option)) {
            if (option == QStringLiteral("--client") && !windows)
                return failure(QStringLiteral("--client is only used on Windows: on Linux the bus tells the programs apart"));
            if (!hasValue) {
                if (i + 1 >= args.size())
                    return failure(QStringLiteral("%1 needs a value").arg(option));
                value = args.at(++i);
            }
            if (values.contains(option))
                return failure(QStringLiteral("%1 is given twice").arg(option));
            values[option] = value;
        } else {
            return failure(QStringLiteral("unknown option: %1 (try --help)").arg(option));
        }
    }
    if (help && (clear || clearAll || !values.empty()))
        return failure(QStringLiteral("--help stands alone"));
    if (help) {
        result.verb = Verb::Help;
        return {result, {}};
    }
    if (clear && clearAll)
        return failure(QStringLiteral("--clear and --clear-all are exclusive"));

    if (const auto client = values.find(QStringLiteral("--client")); client != values.end()) {
        if (!plainText(client->second, 64))
            return failure(QStringLiteral("--client is 1 to 64 plain characters"));
        result.client = client->second;
    }
    if (clearAll) {
        for (const auto &[option, ignored] : values) {
            if (option != QStringLiteral("--client"))
                return failure(QStringLiteral("--clear-all takes only --client"));
        }
        result.verb = Verb::ClearAll;
        return {result, {}};
    }
    const auto key = values.find(QStringLiteral("--key"));
    if (key == values.end())
        return failure(QStringLiteral("--key is needed (try --help)"));
    QString keyError;
    const auto keys = keyNames(key->second, &keyError);
    if (!keys)
        return failure(keyError);
    result.keys = *keys;
    if (clear) {
        for (const auto &[option, ignored] : values) {
            if (option != QStringLiteral("--client") && option != QStringLiteral("--key"))
                return failure(QStringLiteral("--clear takes only --key and --client"));
        }
        result.verb = Verb::Clear;
        return {result, {}};
    }

    // Light: a colour, an effect, a time.
    const bool hasColor = values.contains(QStringLiteral("--color"));
    const bool anyChannel = values.contains(QStringLiteral("--red")) || values.contains(QStringLiteral("--green")) || values.contains(QStringLiteral("--blue"));
    if (hasColor && anyChannel)
        return failure(QStringLiteral("--color and --red/--green/--blue are exclusive"));
    if (hasColor) {
        static const QRegularExpression hex(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        const QString color = values[QStringLiteral("--color")];
        if (!hex.match(color).hasMatch())
            return failure(QStringLiteral("--color is #rrggbb"));
        result.red = color.mid(1, 2).toInt(nullptr, 16);
        result.green = color.mid(3, 2).toInt(nullptr, 16);
        result.blue = color.mid(5, 2).toInt(nullptr, 16);
    } else {
        for (const char *name : {"--red", "--green", "--blue"}) {
            if (!values.contains(QString::fromLatin1(name)))
                return failure(QStringLiteral("a colour is needed: --color #rrggbb, or all of --red, --green and --blue"));
        }
        const auto red = wholeNumber(values[QStringLiteral("--red")], 0, 255);
        const auto green = wholeNumber(values[QStringLiteral("--green")], 0, 255);
        const auto blue = wholeNumber(values[QStringLiteral("--blue")], 0, 255);
        if (!red || !green || !blue)
            return failure(QStringLiteral("--red, --green and --blue are whole numbers from 0 to 255"));
        result.red = *red;
        result.green = *green;
        result.blue = *blue;
    }
    if (const auto effect = values.find(QStringLiteral("--effect")); effect != values.end()) {
        if (effect->second != QStringLiteral("steady") && effect->second != QStringLiteral("pulse"))
            return failure(QStringLiteral("--effect is steady or pulse"));
        result.pulse = effect->second == QStringLiteral("pulse");
    }
    const auto seconds = values.find(QStringLiteral("--seconds"));
    if (seconds == values.end())
        return failure(QStringLiteral("--seconds is needed: a signal always has an end (1 to 86400)"));
    const auto length = wholeNumber(seconds->second, 1, LongestSeconds);
    if (!length)
        return failure(QStringLiteral("--seconds is a whole number from 1 to 86400"));
    result.seconds = *length;
    return {result, {}};
}

} // namespace keysignal
