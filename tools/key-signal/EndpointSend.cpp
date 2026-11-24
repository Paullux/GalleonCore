// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EndpointSend.h"

#include <QLocalSocket>

namespace keysignal {

galleond::LocalCommand commandOf(const Arguments &arguments)
{
    galleond::LocalCommand command{galleond::LocalAction::KeySignal, {}, {}};
    command.client = arguments.client;
    switch (arguments.verb) {
    case Verb::ClearAll:
        command.action = galleond::LocalAction::KeySignalClearAll;
        break;
    case Verb::Clear:
        command.action = galleond::LocalAction::KeySignalClear;
        command.key = arguments.key;
        break;
    case Verb::Light:
    case Verb::Help:
        command.key = arguments.key;
        command.red = arguments.red;
        command.green = arguments.green;
        command.blue = arguments.blue;
        command.effect = arguments.pulse ? QStringLiteral("pulse") : QStringLiteral("steady");
        command.seconds = arguments.seconds;
        break;
    }
    return command;
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

} // namespace keysignal
