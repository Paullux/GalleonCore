// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProcessWait.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstdint>
#include <poll.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace watchdog {

#ifdef _WIN32

WaitResult waitForExit(std::uint32_t pid)
{
    // SYNCHRONIZE is enough to wait: nothing else is asked of the process.
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (process == nullptr)
        return GetLastError() == ERROR_INVALID_PARAMETER ? WaitResult::AlreadyGone : WaitResult::CannotWatch;
    const DWORD waited = WaitForSingleObject(process, INFINITE);
    CloseHandle(process);
    return waited == WAIT_OBJECT_0 ? WaitResult::Exited : WaitResult::CannotWatch;
}

#else

WaitResult waitForExit(std::uint32_t pid)
{
#ifdef SYS_pidfd_open
    const long fd = syscall(SYS_pidfd_open, static_cast<pid_t>(pid), 0);
    if (fd < 0)
        return errno == ESRCH ? WaitResult::AlreadyGone : WaitResult::CannotWatch;
    pollfd watched{.fd = static_cast<int>(fd), .events = POLLIN, .revents = 0};
    int ready = poll(&watched, 1, -1);
    while (ready < 0 && errno == EINTR)
        ready = poll(&watched, 1, -1);
    close(static_cast<int>(fd));
    return ready > 0 ? WaitResult::Exited : WaitResult::CannotWatch;
#else
    static_cast<void>(pid);
    return WaitResult::CannotWatch;
#endif
}

#endif

} // namespace watchdog
