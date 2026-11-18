// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WaylandInputActivity.h"

// Generated at build time by wayland-scanner from ext-idle-notify-v1.xml (the wayland-protocols package): the protocol is not copied here.
#include "ext-idle-notify-v1-client-protocol.h"

#include <wayland-client.h>

#include <QDebug>
#include <QSocketNotifier>

#include <algorithm>
#include <string_view>
#include <utility>

namespace galleond {

namespace {

template <auto Destroy>
struct Destroyer
{
    void operator()(auto *object) const
    {
        if (object != nullptr)
            Destroy(object);
    }
};

using Display = std::unique_ptr<wl_display, Destroyer<&wl_display_disconnect>>;
using Registry = std::unique_ptr<wl_registry, Destroyer<&wl_registry_destroy>>;
using Seat = std::unique_ptr<wl_seat, Destroyer<&wl_seat_destroy>>;
using Callback = std::unique_ptr<wl_callback, Destroyer<&wl_callback_destroy>>;
using Notifier = std::unique_ptr<ext_idle_notifier_v1, Destroyer<&ext_idle_notifier_v1_destroy>>;
using Notification = std::unique_ptr<ext_idle_notification_v1, Destroyer<&ext_idle_notification_v1_destroy>>;

} // namespace

// Lifetime: one owner (the WaylandInputActivity), on the thread of the event loop. The callbacks of libwayland come from dispatching in readable() and get
// back to this object through the `data` pointer that was given to each listener; every proxy is destroyed before the display (members are destroyed
// in reverse order), and the notification first of all when the watch ends.
class WaylandInputActivity::Impl
{
public:
    Impl(WaylandInputActivity &owner, Display display) : m_owner(owner), m_display(std::move(display))
    {
        m_registry.reset(wl_display_get_registry(m_display.get()));
        wl_registry_add_listener(m_registry.get(), &RegistryListener, this);
        m_sync.reset(wl_display_sync(m_display.get())); // answered when the compositor has said every global that it has
        wl_callback_add_listener(m_sync.get(), &SyncListener, this);
        wl_display_flush(m_display.get());
        m_socket = std::make_unique<QSocketNotifier>(wl_display_get_fd(m_display.get()), QSocketNotifier::Read);
        QObject::connect(m_socket.get(), &QSocketNotifier::activated, &m_owner, [this] { readable(); });
    }

    ~Impl() { disarm(); }

    void start()
    {
        if (m_watching || m_failed)
            return;
        m_watching = true;
        if (!m_known)
            return; // armed when the globals are known
        if (!supported()) {
            m_watching = false;
            reportUnavailable();
            return;
        }
        arm();
    }

    void stop()
    {
        m_watching = false;
        disarm();
    }

    [[nodiscard]] bool watching() const { return m_watching; }
    [[nodiscard]] bool supported() const { return !m_failed && (!m_known || (m_notifier && m_seat)); }

private:
    static Impl &from(void *data) { return *static_cast<Impl *>(data); }

    static void onGlobal(void *data, wl_registry *registry, std::uint32_t name, const char *interface, std::uint32_t version)
    {
        Impl &self = from(data);
        const std::string_view wanted = interface != nullptr ? interface : "";
        if (wanted == wl_seat_interface.name && !self.m_seat) {
            self.m_seat.reset(static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, std::min<std::uint32_t>(version, 1))));
        } else if (wanted == ext_idle_notifier_v1_interface.name && !self.m_notifier) {
            self.m_notifierVersion = std::min<std::uint32_t>(version, static_cast<std::uint32_t>(ext_idle_notifier_v1_interface.version));
            self.m_notifier.reset(static_cast<ext_idle_notifier_v1 *>(
                wl_registry_bind(registry, name, &ext_idle_notifier_v1_interface, self.m_notifierVersion)));
        }
    }

    static void onGlobalRemove(void *, wl_registry *, std::uint32_t) {}

    static void onSynced(void *data, wl_callback *, std::uint32_t)
    {
        Impl &self = from(data);
        self.m_known = true;
        self.m_sync.reset();
        if (!self.m_watching)
            return;
        if (self.supported())
            self.arm();
        else {
            self.m_watching = false;
            self.reportUnavailable();
        }
    }

    static void onIdled(void *data, ext_idle_notification_v1 *) { from(data).m_idled = true; }

    // Used again after having been idle: that is the action. The watch is over (the notification is destroyed before anyone is told).
    static void onResumed(void *data, ext_idle_notification_v1 *)
    {
        Impl &self = from(data);
        if (!self.m_idled || !self.m_watching)
            return;
        self.m_watching = false;
        self.disarm();
        Q_EMIT self.m_owner.activity();
    }

    static constexpr wl_registry_listener RegistryListener{
        .global = &Impl::onGlobal,
        .global_remove = &Impl::onGlobalRemove,
    };
    static constexpr wl_callback_listener SyncListener{.done = &Impl::onSynced};
    static constexpr ext_idle_notification_v1_listener NotificationListener{
        .idled = &Impl::onIdled,
        .resumed = &Impl::onResumed,
    };

    void arm()
    {
        if (m_notification)
            return;
        m_idled = false;
        ext_idle_notification_v1 *notification = nullptr;
#ifdef EXT_IDLE_NOTIFIER_V1_GET_INPUT_IDLE_NOTIFICATION_SINCE_VERSION
        // The version 2 notification is about the input of the person, whatever keeps the screen awake (a video playing, for example).
        if (m_notifierVersion >= EXT_IDLE_NOTIFIER_V1_GET_INPUT_IDLE_NOTIFICATION_SINCE_VERSION)
            notification = ext_idle_notifier_v1_get_input_idle_notification(m_notifier.get(), TimeoutMs, m_seat.get());
#endif
        if (notification == nullptr)
            notification = ext_idle_notifier_v1_get_idle_notification(m_notifier.get(), TimeoutMs, m_seat.get());
        m_notification.reset(notification);
        ext_idle_notification_v1_add_listener(m_notification.get(), &NotificationListener, this);
        wl_display_flush(m_display.get());
    }

    void disarm()
    {
        if (!m_notification)
            return;
        m_notification.reset(); // the compositor watches nothing for us any more
        m_idled = false;
        if (m_display)
            wl_display_flush(m_display.get());
    }

    void readable()
    {
        if (m_failed)
            return;
        while (wl_display_prepare_read(m_display.get()) != 0) {
            if (wl_display_dispatch_pending(m_display.get()) < 0) {
                fail();
                return;
            }
        }
        if (wl_display_read_events(m_display.get()) < 0 || wl_display_dispatch_pending(m_display.get()) < 0) {
            fail();
            return;
        }
        wl_display_flush(m_display.get());
    }

    // The connection to the compositor is lost: nothing more can be said, and it is said once.
    void fail()
    {
        m_failed = true;
        m_socket->setEnabled(false);
        const bool wasWatching = m_watching;
        m_watching = false;
        m_notification.reset(); // the connection is gone: the request to destroy it is dropped by libwayland, the memory is freed
        if (wasWatching)
            reportUnavailable();
    }

    void reportUnavailable()
    {
        if (!m_reported) {
            m_reported = true;
            Q_EMIT m_owner.unavailable();
        }
    }

    WaylandInputActivity &m_owner;
    Display m_display;
    Registry m_registry;
    Callback m_sync;
    Seat m_seat;
    Notifier m_notifier;
    Notification m_notification;
    std::unique_ptr<QSocketNotifier> m_socket;
    std::uint32_t m_notifierVersion = 0;
    bool m_known = false;    // the compositor has said every global
    bool m_watching = false;
    bool m_idled = false;    // the compositor said that the session was idle since this watch began
    bool m_failed = false;
    bool m_reported = false;
};

WaylandInputActivity::WaylandInputActivity(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}

WaylandInputActivity::~WaylandInputActivity() = default;

std::unique_ptr<WaylandInputActivity> WaylandInputActivity::connectToSession(const char *socketName)
{
    Display display(wl_display_connect(socketName));
    if (!display)
        return nullptr;
    // The Impl needs its owner, which needs the Impl: the owner is made first, the Impl is given to it.
    std::unique_ptr<WaylandInputActivity> activity(new WaylandInputActivity(nullptr)); // NOLINT(cppcoreguidelines-owning-memory): private constructor
    activity->m_impl = std::make_unique<Impl>(*activity, std::move(display));
    return activity;
}

void WaylandInputActivity::start()
{
    m_impl->start();
}

void WaylandInputActivity::stop()
{
    m_impl->stop();
}

bool WaylandInputActivity::watching() const
{
    return m_impl->watching();
}

bool WaylandInputActivity::supported() const
{
    return m_impl->supported();
}

} // namespace galleond
