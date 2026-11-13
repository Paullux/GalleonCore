// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "QuitOnSignals.h"

#include <QCoreApplication>
#include <QSocketNotifier>

#include <array>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>

namespace galleond {
namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): a signal handler can only reach global state
std::array<int, 2> g_pipe{-1, -1};

extern "C" void onSignal(int number)
{
    // Only async-signal-safe calls: write one byte (the signal number) to the pipe.
    const auto byte = static_cast<char>(number);
    if (g_pipe[1] >= 0)
        static_cast<void>(!write(g_pipe[1], &byte, 1));
}

} // namespace

QuitOnSignals::QuitOnSignals(std::initializer_list<int> numbers, QObject *parent) : QObject(parent), m_numbers(numbers)
{
    if (g_pipe[0] >= 0)
        return; // one instance only
    // Non-blocking on both ends: the handler must never wait, and the reader stops when the pipe is empty (a blocking read
    // here would leave the daemon hanging on the signal that was meant to stop it).
    if (pipe2(g_pipe.data(), O_NONBLOCK | O_CLOEXEC) != 0) {
        g_pipe = {-1, -1};
        return;
    }
    auto *notifier = new QSocketNotifier(g_pipe[0], QSocketNotifier::Read, this); // owned by this
    connect(notifier, &QSocketNotifier::activated, this, &QuitOnSignals::onReadable);
    for (const int number : m_numbers) {
        struct sigaction action {};
        action.sa_handler = onSignal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        static_cast<void>(sigaction(number, &action, nullptr));
    }
}

QuitOnSignals::~QuitOnSignals()
{
    for (const int number : m_numbers)
        static_cast<void>(signal(number, SIG_DFL));
    for (int &descriptor : g_pipe) {
        if (descriptor >= 0)
            close(descriptor);
        descriptor = -1;
    }
}

void QuitOnSignals::onReadable()
{
    char byte = 0;
    while (read(g_pipe[0], &byte, 1) == 1) {
        ++m_received;
        Q_EMIT signalReceived(static_cast<unsigned char>(byte));
        QCoreApplication::quit();
    }
}

} // namespace galleond
