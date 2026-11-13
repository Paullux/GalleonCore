// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "KeyboardController.h"

#include <galleon/HidTransport.h>
#include <galleon/keyboard/Brightness.h>
#include <galleon/keyboard/KeyboardLink.h>

#include <QDebug>
#include <QMetaObject>
#include <QRunnable>

#include <chrono>

namespace galleond {
namespace {

constexpr std::uint16_t VendorId = 0x1b1c;
constexpr std::uint16_t KeyboardProductId = 0x2b0c;
constexpr int VendorInterface = 1; // MI_01, usage page 0xFF42 usage 1 (docs/protocol/README.md)
constexpr auto ReportTimeout = std::chrono::milliseconds(500);

} // namespace

KeyboardController::KeyboardController(Opener opener, QObject *parent) : QObject(parent), m_opener(std::move(opener))
{
    m_pool.setMaxThreadCount(1);
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &KeyboardController::startNext);
}

KeyboardController::~KeyboardController()
{
    m_retry.stop();
    m_pool.waitForDone();
}

KeyboardController::Opener KeyboardController::hidOpener()
{
    return [] () -> std::unique_ptr<galleon::Transport> {
        auto transport = std::make_unique<galleon::HidTransport>(VendorId, KeyboardProductId, VendorInterface);
        if (!transport->open())
            return nullptr;
        const QString collection = transport->openedCollection();
        // Interface 1 must be the vendor one: never talk to anything else.
        if (!collection.contains(QStringLiteral("usagePage=0xff42")) || !collection.contains(QStringLiteral("usage=0x0001"))) {
            transport->close();
            return nullptr;
        }
        return transport;
    };
}

std::optional<std::uint16_t> KeyboardController::acceptedBrightness(int value)
{
    if (value < 0 || value % StepSize != 0 || !galleon::keyboard::isValidBrightness(static_cast<std::uint32_t>(value)))
        return std::nullopt;
    return static_cast<std::uint16_t>(value);
}

bool KeyboardController::SetBrightness(int value)
{
    if (!acceptedBrightness(value)) {
        qWarning() << "galleond: keyboard brightness" << value << "refused (steps of 100 from 0 to 1000)";
        return false;
    }
    m_pending = value;
    startNext();
    return true;
}

bool KeyboardController::ReadBrightness()
{
    m_readPending = true;
    startNext();
    return true;
}

void KeyboardController::startNext()
{
    if (m_busy || (!m_pending && !m_readPending))
        return;
    if (m_started) {
        const qint64 remaining = MinIntervalMs - m_sinceStart.elapsed();
        if (remaining > 0) {
            m_retry.start(static_cast<int>(remaining));
            return;
        }
    }
    m_busy = true;
    m_started = true;
    m_sinceStart.restart();
    // A write waiting goes before a read: the read would only show a value that is about to change.
    if (m_pending) {
        const int value = *m_pending;
        m_pending.reset();
        startWrite(value);
    } else {
        m_readPending = false;
        startRead();
    }
}

void KeyboardController::startWrite(int value)
{
    const Opener opener = m_opener;
    m_pool.start(QRunnable::create([this, opener, value] {
        bool ok = false;
        QString reason;
        std::unique_ptr<galleon::Transport> transport = opener();
        if (!transport) {
            reason = QStringLiteral("the keyboard could not be opened (plugged in? rights? another program?)");
        } else {
            const auto result = galleon::keyboard::writeBrightness(*transport, static_cast<std::uint16_t>(value), ReportTimeout);
            ok = result.error == galleon::keyboard::LinkError::None;
            if (!ok)
                reason = QString::fromLatin1(galleon::keyboard::describe(result.error));
            transport->close();
        }
        QMetaObject::invokeMethod(this, [this, value, ok, reason] { finished(value, ok, reason); }, Qt::QueuedConnection);
    }));
}

void KeyboardController::startRead()
{
    const Opener opener = m_opener;
    m_pool.start(QRunnable::create([this, opener] {
        int value = 0;
        bool ok = false;
        QString reason;
        std::unique_ptr<galleon::Transport> transport = opener();
        if (!transport) {
            reason = QStringLiteral("the keyboard could not be opened (plugged in? rights? another program?)");
        } else {
            const auto result = galleon::keyboard::readBrightness(*transport, ReportTimeout);
            ok = result.error == galleon::keyboard::LinkError::None;
            if (ok)
                value = result.value;
            else
                reason = QString::fromLatin1(galleon::keyboard::describe(result.error));
            transport->close();
        }
        QMetaObject::invokeMethod(this, [this, value, ok, reason] { readFinished(value, ok, reason); }, Qt::QueuedConnection);
    }));
}

void KeyboardController::finished(int value, bool ok, const QString &reason)
{
    m_busy = false;
    if (ok)
        qInfo() << "galleond: keyboard brightness set to" << value / 10 << "%";
    else
        qWarning() << "galleond: keyboard brightness" << value << "not set:" << reason;
    Q_EMIT brightnessWritten(value, ok, reason);
    startNext();
}

void KeyboardController::readFinished(int value, bool ok, const QString &reason)
{
    m_busy = false;
    if (ok)
        qInfo() << "galleond: keyboard brightness is" << value / 10.0 << "%";
    else
        qWarning() << "galleond: keyboard brightness not read:" << reason;
    Q_EMIT brightnessRead(value, ok, reason);
    startNext();
}

} // namespace galleond
