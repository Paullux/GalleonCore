// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputActivity.h"

#ifdef GALLEON_WAYLAND_IDLE
#include "WaylandInputActivity.h"
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#endif

#include <memory>
#include <utility>

namespace galleond {

namespace {

#ifdef Q_OS_WIN

IdleReader windowsReader()
{
    return [](std::function<void(std::optional<qint64>)> done) {
        LASTINPUTINFO info{};
        info.cbSize = sizeof(info);
        if (!GetLastInputInfo(&info)) {
            done(std::nullopt);
            return;
        }
        done(static_cast<qint64>(GetTickCount() - info.dwTime)); // milliseconds since the last input (wraps correctly in 32 bits)
    };
}

#else

// The places where a Linux session says its idle time: KDE (Plasma) and GNOME (Mutter). They are tried in turn, and the first that answers is kept.
struct IdleSource
{
    const char *service;
    const char *path;
    const char *interface;
    const char *method;
};

constexpr IdleSource Sources[] = {                                                             // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
    {"org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", "GetSessionIdleTime"},
    {"org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver", "GetSessionIdleTime"},
    {"org.gnome.Mutter.IdleMonitor", "/org/gnome/Mutter/IdleMonitor/Core", "org.gnome.Mutter.IdleMonitor", "GetIdletime"},
};

IdleReader linuxReader()
{
    auto chosen = std::make_shared<int>(-1); // the source that worked last time
    return [chosen](std::function<void(std::optional<qint64>)> done) {
        const int first = *chosen >= 0 ? *chosen : 0;
        const auto ask = [chosen, done = std::move(done)](auto &&self, int index, int tried) -> void {
            if (tried >= static_cast<int>(std::size(Sources))) {
                done(std::nullopt);
                return;
            }
            const IdleSource &source = Sources[index];                                           // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
            const QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(source.service), QString::fromLatin1(source.path),
                                                               QString::fromLatin1(source.interface), QString::fromLatin1(source.method));
            auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 300), nullptr);
            QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [=](QDBusPendingCallWatcher *finished) {
                const QDBusMessage reply = finished->reply();
                std::optional<qint64> idle;
                if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
                    bool ok = false;
                    const qulonglong value = reply.arguments().constFirst().toULongLong(&ok);
                    if (ok)
                        idle = static_cast<qint64>(value);
                }
                finished->deleteLater();
                if (idle) {
                    *chosen = index;
                    done(idle);
                } else {
                    self(self, (index + 1) % static_cast<int>(std::size(Sources)), tried + 1);
                }
            });
        };
        ask(ask, first, 0);
    };
}

#endif

} // namespace

IdleReader systemIdleReader()
{
#ifdef Q_OS_WIN
    return windowsReader();
#else
    return linuxReader();
#endif
}

InputActivityWatcher::InputActivityWatcher(IdleReader reader, int intervalMs, QObject *parent)
    : InputActivitySource(parent), m_reader(std::move(reader))
{
    m_timer.setInterval(intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &InputActivityWatcher::poll);
}

void InputActivityWatcher::start()
{
    if (m_timer.isActive())
        return;
    m_detector.reset();
    m_waiting = false;
    ++m_generation;
    m_timer.start();
    poll(); // the first reading is the baseline
}

void InputActivityWatcher::stop()
{
    m_timer.stop();
    m_waiting = false;
    ++m_generation; // a reading still on its way is not for any watch
}

void InputActivityWatcher::poll()
{
    if (m_waiting || !m_reader)
        return;
    m_waiting = true;
    const int generation = m_generation;
    m_reader([this, generation](std::optional<qint64> idle) {
        if (generation != m_generation)
            return; // stopped (or stopped and started again) since
        m_waiting = false;
        if (!idle) {
            if (!m_reportedMissing) {
                m_reportedMissing = true;
                Q_EMIT unavailable();
            }
            return;
        }
        if (m_detector.onReading(*idle)) {
            stop();
            Q_EMIT activity();
        }
    });
}

namespace {

// The way that this system has: the compositor of a Wayland session when it has the idle notification (KDE Plasma), else the idle time that is read (GNOME,
// KDE on X11, Windows). Which one is chosen at each start(), so that a compositor that answers late is still used.
class SystemInputActivity final : public InputActivitySource
{
public:
    SystemInputActivity()
    {
        forward(m_polling);
#ifdef GALLEON_WAYLAND_IDLE
        if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
            m_wayland = WaylandInputActivity::connectToSession();
            if (m_wayland)
                forward(*m_wayland);
        }
#endif
    }

    void start() override
    {
        if (m_active != nullptr)
            return;
        m_active = &m_polling;
#ifdef GALLEON_WAYLAND_IDLE
        if (m_wayland && m_wayland->supported())
            m_active = m_wayland.get();
#endif
        m_active->start();
    }

    void stop() override
    {
        if (m_active == nullptr)
            return;
        m_active->stop();
        m_active = nullptr;
    }

    [[nodiscard]] bool watching() const override { return m_active != nullptr && m_active->watching(); }

private:
    void forward(InputActivitySource &source)
    {
        connect(&source, &InputActivitySource::activity, this, [this] {
            m_active = nullptr; // the watch that said it is over
            Q_EMIT activity();
        });
        connect(&source, &InputActivitySource::unavailable, this, &InputActivitySource::unavailable);
    }

    InputActivityWatcher m_polling;
#ifdef GALLEON_WAYLAND_IDLE
    std::unique_ptr<WaylandInputActivity> m_wayland;
#endif
    InputActivitySource *m_active = nullptr;
};

} // namespace

std::unique_ptr<InputActivitySource> makeInputActivitySource()
{
    return std::make_unique<SystemInputActivity>();
}

} // namespace galleond
