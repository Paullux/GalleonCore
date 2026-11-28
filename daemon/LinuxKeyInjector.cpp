// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LinuxKeyInjector.h"

#include <QDebug>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <span>
#include <string_view>
#include <unistd.h>

namespace galleond {

namespace {

struct KeyMapping
{
    KeyCode key;
    int native;
};

// Linux input-event-codes.h translates the shared whitelist to uinput codes.
static_assert(KEY_F24 - KEY_F13 == 11, "F13 to F24 are consecutive");
constexpr std::array<KeyMapping, AllowedKeyCount> Mapping{{
    {KeyCode::Kp0, KEY_KP0}, {KeyCode::Kp1, KEY_KP1}, {KeyCode::Kp2, KEY_KP2},
    {KeyCode::Kp3, KEY_KP3}, {KeyCode::Kp4, KEY_KP4}, {KeyCode::Kp5, KEY_KP5},
    {KeyCode::Kp6, KEY_KP6}, {KeyCode::Kp7, KEY_KP7}, {KeyCode::Kp8, KEY_KP8},
    {KeyCode::Kp9, KEY_KP9}, {KeyCode::KpDot, KEY_KPDOT}, {KeyCode::KpEnter, KEY_KPENTER},
    {KeyCode::KpPlus, KEY_KPPLUS}, {KeyCode::KpMinus, KEY_KPMINUS}, {KeyCode::KpAsterisk, KEY_KPASTERISK},
    {KeyCode::KpSlash, KEY_KPSLASH}, {KeyCode::NumLock, KEY_NUMLOCK}, {KeyCode::Backspace, KEY_BACKSPACE},
    {KeyCode::F13, KEY_F13}, {KeyCode::F14, KEY_F14}, {KeyCode::F15, KEY_F15}, {KeyCode::F16, KEY_F16},
    {KeyCode::F17, KEY_F17}, {KeyCode::F18, KEY_F18}, {KeyCode::F19, KEY_F19}, {KeyCode::F20, KEY_F20},
    {KeyCode::F21, KEY_F21}, {KeyCode::F22, KEY_F22}, {KeyCode::F23, KEY_F23}, {KeyCode::F24, KEY_F24},
    {KeyCode::F1, KEY_F1}, {KeyCode::F2, KEY_F2}, {KeyCode::F3, KEY_F3}, {KeyCode::F4, KEY_F4},
    {KeyCode::F5, KEY_F5}, {KeyCode::F6, KEY_F6}, {KeyCode::F7, KEY_F7}, {KeyCode::F8, KEY_F8},
    {KeyCode::F9, KEY_F9}, {KeyCode::F10, KEY_F10}, {KeyCode::F11, KEY_F11}, {KeyCode::F12, KEY_F12},
    {KeyCode::Up, KEY_UP}, {KeyCode::Down, KEY_DOWN}, {KeyCode::Left, KEY_LEFT}, {KeyCode::Right, KEY_RIGHT},
}};

// Every key of the whitelist has its entry, in the order of the list: a key added to KeyCode without a native code here fails to build, and
// the virtual keyboard is only ever given the codes of this table.
constexpr bool mappingCoversTheWholeList()
{
    for (std::size_t i = 0; i < Mapping.size(); ++i) {
        if (static_cast<std::size_t>(Mapping.at(i).key) != i + 1)
            return false;
    }
    return true;
}
static_assert(mappingCoversTheWholeList());

} // namespace

UinputKeyboard::~UinputKeyboard()
{
    if (m_fd >= 0) {
        ioctl(m_fd, UI_DEV_DESTROY);
        close(m_fd);
    }
}

void UinputKeyboard::prepare()
{
    if (m_fd >= 0 || m_failed)
        return;
    const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        m_failed = true;
        qWarning() << "galleond: cannot open /dev/uinput (" << std::strerror(errno)
                   << "): key actions do nothing. See tools/udev/71-galleon-uinput.rules";
        return;
    }
    bool ok = ioctl(fd, UI_SET_EVBIT, EV_KEY) == 0;
    for (const KeyMapping &entry : Mapping)
        ok = ioctl(fd, UI_SET_KEYBIT, entry.native) == 0 && ok;
    uinput_setup setup{};
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1b1c; // the Galleon's maker, so that the device is recognisable
    setup.id.product = 0x2b18;
    setup.id.version = 1;
    constexpr std::string_view DeviceName = "GalleonCore virtual keypad"; // the rest of setup.name stays zero
    static_assert(DeviceName.size() < UINPUT_MAX_NAME_SIZE);
    std::ranges::copy(DeviceName, std::span<char>(setup.name).begin());
    ok = ok && ioctl(fd, UI_DEV_SETUP, &setup) == 0 && ioctl(fd, UI_DEV_CREATE) == 0;
    if (!ok) {
        m_failed = true;
        qWarning() << "galleond: cannot create the virtual keyboard (" << std::strerror(errno) << ")";
        close(fd);
        return;
    }
    m_fd = fd;
    qInfo() << "galleond: virtual keypad created";
}

bool UinputKeyboard::key(int code, bool down)
{
    const auto *const mapping = std::ranges::find_if(Mapping, [code](const KeyMapping &entry) {
        return static_cast<int>(entry.key) == code;
    });
    if (mapping == Mapping.end())
        return false;
    prepare();
    if (m_fd < 0)
        return false;

    std::array<input_event, 2> events{};
    events[0].type = EV_KEY;
    events[0].code = static_cast<unsigned short>(mapping->native);
    events[0].value = down ? 1 : 0;
    events[1].type = EV_SYN;
    events[1].code = SYN_REPORT;
    events[1].value = 0;
    return write(m_fd, events.data(), sizeof events) == static_cast<ssize_t>(sizeof events);
}

} // namespace galleond
