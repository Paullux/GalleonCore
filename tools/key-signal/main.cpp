// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// galleon-key-signal: lights one key, or several given together, the way another program does. On Linux it keeps its D-Bus connection for as long as the key stays lit (the signal belongs to it);
// on Windows it sends the request to the local endpoint and ends. It reads no key press and no state of the keyboard. docs/KEY_SIGNALS.md.
#include "KeySignalArgs.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

#ifdef Q_OS_WIN
#include "EndpointSend.h"
#else
#include "HeldSignal.h"

#include <csignal>
#endif

namespace {
#ifndef Q_OS_WIN
volatile std::sig_atomic_t g_stop = 0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): set by the handler of Ctrl-C and of kill, the only thing a handler does
void onStopSignal(int /*signal*/)
{
    g_stop = 1;
}
#endif
} // namespace

int main(int argc, char *argv[])
{
    const QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);
#ifdef Q_OS_WIN
    constexpr bool windows = true;
#else
    constexpr bool windows = false;
#endif
    const keysignal::ParseResult parsed = keysignal::parseArguments(QCoreApplication::arguments().mid(1), windows);
    if (!parsed.arguments) {
        err << "galleon-key-signal: " << parsed.error << "\n";
        return 1;
    }
    const keysignal::Arguments &arguments = *parsed.arguments;
    if (arguments.verb == keysignal::Verb::Help) {
        out << keysignal::usage(windows);
        return 0;
    }
#ifdef Q_OS_WIN
    const QString reason = keysignal::sendAllToEndpoint(galleond::localControlName(), keysignal::commandsOf(arguments));
    if (!reason.isEmpty()) {
        err << "galleon-key-signal: " << reason << "\n";
        return 2;
    }
    out << "Sent. galleond says nothing about the lighting here: look at the keyboard (the box \"Other programs may light keys\" must be ticked and the live lighting on).\n";
    return 0;
#else
    keysignal::HeldSignal held;
    const QString reason = held.light(arguments);
    if (!reason.isEmpty()) {
        err << "galleon-key-signal: " << reason << "\n";
        return 2;
    }
    out << "Lit " << arguments.keys.join(QStringLiteral(", ")) << " for " << arguments.seconds << " s. Ctrl-C or kill takes " << (arguments.keys.size() == 1 ? "it" : "them")
        << " away.\n" << Qt::flush;
    static_cast<void>(std::signal(SIGINT, onStopSignal)); // the previous handlers are not needed
    static_cast<void>(std::signal(SIGTERM, onStopSignal));
    QTimer end;
    end.setSingleShot(true);
    QObject::connect(&end, &QTimer::timeout, &app, &QCoreApplication::quit);
    end.start(arguments.seconds * 1000);
    QTimer watch; // looks at the flag of the handler: nothing else is safe to do in it
    QObject::connect(&watch, &QTimer::timeout, &app, [] {
        if (g_stop != 0)
            QCoreApplication::quit();
    });
    watch.start(100);
    QCoreApplication::exec();
    held.release();
    return 0;
#endif
}
