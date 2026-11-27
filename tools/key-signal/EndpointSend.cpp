// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EndpointSend.h"

#include <QLocalSocket>

namespace keysignal {

QList<galleond::LocalCommand> commandsOf(const Arguments &arguments)
{
    galleond::LocalCommand base{galleond::LocalAction::KeySignal, {}, {}};
    base.client = arguments.client;
    switch (arguments.verb) {
    case Verb::ClearAll:
        base.action = galleond::LocalAction::KeySignalClearAll;
        return {base};
    case Verb::Clear:
        base.action = galleond::LocalAction::KeySignalClear;
        break;
    case Verb::Light:
    case Verb::Help:
        base.red = arguments.red;
        base.green = arguments.green;
        base.blue = arguments.blue;
        base.effect = arguments.pulse ? QStringLiteral("pulse") : QStringLiteral("steady");
        base.seconds = arguments.seconds;
        break;
    }
    QList<galleond::LocalCommand> commands;
    for (const QString &key : arguments.keys) {
        galleond::LocalCommand command = base;
        command.key = key;
        commands.append(command);
    }
    return commands;
}

QString sendToEndpoint(const QString &endpoint, const galleond::LocalCommand &command, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(endpoint);
    if (!socket.waitForConnected(timeoutMs))
        return QStringLiteral("galleond cannot be reached (is it running?)");
    socket.write(galleond::encodeLocalCommand(command));
    if (!socket.waitForBytesWritten(timeoutMs))
        return QStringLiteral("the request could not be written");
    if (!socket.waitForReadyRead(timeoutMs))
        return QStringLiteral("galleond did not answer");
    if (socket.readLine(64).trimmed() != QByteArrayLiteral("{\"ok\":true}"))
        return QStringLiteral("galleond did not accept the request");
    return {};
}

QString sendAllToEndpoint(const QString &endpoint, const QList<galleond::LocalCommand> &commands, int timeoutMs)
{
    for (qsizetype sent = 0; sent < commands.size(); ++sent) {
        const galleond::LocalCommand &command = commands.at(sent);
        QString reason = sendToEndpoint(endpoint, command, timeoutMs);
        if (reason.isEmpty())
            continue;
        if (commands.size() == 1)
            return reason;
        return QStringLiteral("%1 (for %2; %3 of %4 requests had been taken before it)").arg(reason, command.key.isEmpty() ? QStringLiteral("the request") : command.key).arg(sent).arg(commands.size());
    }
    return {};
}

} // namespace keysignal
