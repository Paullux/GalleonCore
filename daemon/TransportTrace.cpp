// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TransportTrace.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QStandardPaths>

#include <algorithm>

namespace galleond {

QString traceLine(std::int64_t milliseconds, const char *direction, std::span<const std::uint8_t> report)
{
    QString bytes;
    const std::size_t shown = std::min(report.size(), TraceBytesPerReport);
    for (std::size_t i = 0; i < shown; ++i)
        bytes += QStringLiteral("%1 ").arg(report[i], 2, 16, QLatin1Char('0'));
    return QStringLiteral("%1 ms %2 %3 bytes: %4%5")
        .arg(milliseconds, 7)
        .arg(QString::fromLatin1(direction))
        .arg(report.size())
        .arg(bytes.trimmed())
        .arg(report.size() > shown ? QStringLiteral(" ...") : QString());
}

TraceSink::TraceSink(Write write, std::size_t limit) : m_write(std::move(write)), m_limit(limit) {}

void TraceSink::line(const QString &text)
{
    const std::scoped_lock lock(m_mutex);
    if (m_full)
        return;
    const std::size_t size = static_cast<std::size_t>(text.toUtf8().size()) + 1;
    if (m_written + size > m_limit) {
        m_full = true;
        m_write(QStringLiteral("the log is full (%1 bytes): nothing more is written").arg(m_limit));
        return;
    }
    m_written += size;
    m_write(text);
}

TracedTransport::TracedTransport(std::unique_ptr<galleon::Transport> inner, std::shared_ptr<TraceSink> sink, Clock clock)
    : m_inner(std::move(inner)), m_sink(std::move(sink)), m_clock(std::move(clock))
{
}

bool TracedTransport::open()
{
    const bool opened = m_inner->open();
    m_sink->line(QStringLiteral("%1 ms open: %2").arg(m_clock(), 7).arg(opened ? QStringLiteral("yes") : QStringLiteral("no")));
    return opened;
}

void TracedTransport::close()
{
    m_inner->close();
    m_sink->line(QStringLiteral("%1 ms closed").arg(m_clock(), 7));
}

bool TracedTransport::sendFeatureReport(std::span<const std::uint8_t> report)
{
    const bool sent = m_inner->sendFeatureReport(report);
    m_sink->line(traceLine(m_clock(), sent ? "feature >" : "feature > FAILED", report));
    return sent;
}

bool TracedTransport::write(std::span<const std::uint8_t> report)
{
    const bool written = m_inner->write(report);
    m_sink->line(traceLine(m_clock(), written ? ">" : "> FAILED", report));
    return written;
}

int TracedTransport::read(std::span<std::uint8_t> buffer)
{
    const int count = m_inner->read(buffer);
    if (count > 0)
        m_sink->line(traceLine(m_clock(), "<", buffer.first(static_cast<std::size_t>(count))));
    else if (count < 0)
        m_sink->line(QStringLiteral("%1 ms < the device is gone (read returned %2)").arg(m_clock(), 7).arg(count));
    return count;
}

std::shared_ptr<TraceSink> openLightingTrace()
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/GalleonCore/logs");
    if (!QDir().mkpath(folder))
        return nullptr;
    const QString path = QStringLiteral("%1/lighting-%2-%3.log")
                             .arg(folder, QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss")))
                             .arg(QCoreApplication::applicationPid());
    auto file = std::make_shared<QFile>(path);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Text))
        return nullptr;
    return std::make_shared<TraceSink>([file](const QString &text) {
        file->write(text.toUtf8() + '\n');
        file->flush();
    });
}

} // namespace galleond
