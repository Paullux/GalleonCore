// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HidrawKeyPressSource.h"

#include <galleon/keyboard/KeyReport.h>

#include <QDebug>

#include <array>
#include <cerrno>
#include <iterator>
#include <string>
#include <dirent.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace galleond {
namespace {

constexpr unsigned KeyboardVendor = 0x1b1c;
constexpr unsigned KeyboardProduct = 0x2b0c;
constexpr std::size_t ReadBufferSize = 64; // the longest report of the keyboard is 18 bytes (KeyReportSize): a longer one is cut and refused by its length

bool isTheKeyboardsKeyNode(int fd)
{
    hidraw_devinfo info{};
    if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0 || static_cast<unsigned>(info.vendor) != KeyboardVendor || static_cast<unsigned>(info.product) != KeyboardProduct)
        return false;
    int size = 0;
    if (ioctl(fd, HIDIOCGRDESCSIZE, &size) < 0 || size < 4 || size > HID_MAX_DESCRIPTOR_SIZE)
        return false;
    hidraw_report_descriptor descriptor{};
    descriptor.size = static_cast<unsigned>(size);
    if (ioctl(fd, HIDIOCGRDESC, &descriptor) < 0)
        return false;
    return looksLikeKeyboardDescriptor(std::span<const std::uint8_t>(std::data(descriptor.value), static_cast<std::size_t>(size)));
}

} // namespace

bool looksLikeKeyboardDescriptor(std::span<const std::uint8_t> descriptor)
{
    return descriptor.size() >= 4 && descriptor[0] == 0x05 && descriptor[1] == 0x01 && descriptor[2] == 0x09 && descriptor[3] == 0x06;
}

int HidrawKeyPressSource::openKeyboardNode()
{
    DIR *dir = opendir("/dev");
    if (!dir)
        return -1;
    int found = -1;
    while (found < 0) {
        const dirent *entry = readdir(dir);
        if (!entry)
            break;
        const std::string name(std::data(entry->d_name));
        if (!name.starts_with("hidraw"))
            continue;
        const std::string path = "/dev/" + name;
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC); // read-only: nothing is ever written to the keyboard from here
        if (fd < 0)
            continue;
        if (isTheKeyboardsKeyNode(fd))
            found = fd;
        else
            close(fd);
    }
    closedir(dir);
    return found;
}

HidrawKeyPressSource::Opener HidrawKeyPressSource::defaultOpener()
{
    return [] { return openKeyboardNode(); };
}

HidrawKeyPressSource::HidrawKeyPressSource(Opener opener, QObject *parent) : KeyPressSource(parent), m_opener(std::move(opener)) {}

HidrawKeyPressSource::~HidrawKeyPressSource()
{
    release();
}

void HidrawKeyPressSource::start()
{
    if (watching())
        return;
    m_fd = m_opener ? m_opener() : -1;
    if (m_fd < 0) {
        Q_EMIT unavailable();
        return;
    }
    m_warnedShape = false;
    m_notifier = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, [this] { onReadable(); });
}

void HidrawKeyPressSource::stop()
{
    release();
}

void HidrawKeyPressSource::release()
{
    m_notifier.reset();
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

void HidrawKeyPressSource::fail()
{
    release();
    Q_EMIT unavailable();
}

void HidrawKeyPressSource::onReadable()
{
    std::array<std::uint8_t, ReadBufferSize> buffer{};
    for (int turn = 0; turn < ReportsPerWakeup && m_fd >= 0; ++turn) {
        const ssize_t got = read(m_fd, buffer.data(), buffer.size());
        if (got < 0 && (errno == EAGAIN || errno == EINTR))
            break; // nothing more for now
        if (got <= 0) {
            buffer.fill(0);
            fail(); // the keyboard is gone, or the node cannot be read
            return;
        }
        const std::span<const std::uint8_t> report(buffer.data(), static_cast<std::size_t>(got));
        if (const auto usages = galleon::keyboard::keysDown(report)) {
            for (const auto usage : *usages) {
                if (m_fd < 0)
                    break; // a slot connected to keyDown stopped the source: the rest of this report is not said
                Q_EMIT keyDown(usage);
            }
        } else if (!galleon::keyboard::isMediaKeyReport(report) && !m_warnedShape) {
            m_warnedShape = true;
            // Its ID and length only: never a byte of what was read.
            qWarning("Key source: a report of another shape was dropped (ID %u, %zu bytes)", static_cast<unsigned>(report[0]), report.size());
        }
        buffer.fill(0); // nothing of the report stays in memory once it has been used
    }
}

} // namespace galleond
