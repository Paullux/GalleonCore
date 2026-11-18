// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The idle notification of a Wayland compositor, tested against a small compositor of our own (libwayland-server) that only has a seat and the idle
// notifier: what is asked of it, what it is told, and that nothing is asked when nothing is watched.
#include "InputActivity.h"
#include "WaylandInputActivity.h"

#include "ext-idle-notify-v1-server-protocol.h"

#include <wayland-server.h>

#include <QFile>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <string>

using galleond::WaylandInputActivity;

namespace {

// What the compositor saw, and what it can say. Single thread: it is driven by the event loop of the test, like the client.
class FakeCompositor
{
public:
    // `idleVersion` 0: no idle notifier at all. `withSeat` false: no seat.
    explicit FakeCompositor(std::uint32_t idleVersion, bool withSeat = true) : m_display(wl_display_create())
    {
        const char *name = wl_display_add_socket_auto(m_display);
        m_socket = name != nullptr ? name : "";
        if (withSeat)
            wl_global_create(m_display, &wl_seat_interface, 1, this, &FakeCompositor::bindSeat);
        if (idleVersion > 0)
            wl_global_create(m_display, &ext_idle_notifier_v1_interface, static_cast<int>(idleVersion), this, &FakeCompositor::bindNotifier);
        wl_event_loop *loop = wl_display_get_event_loop(m_display);
        m_notifier = std::make_unique<QSocketNotifier>(wl_event_loop_get_fd(loop), QSocketNotifier::Read);
        QObject::connect(m_notifier.get(), &QSocketNotifier::activated, [this] { dispatch(); });
    }

    ~FakeCompositor()
    {
        m_notifier.reset();
        wl_display_destroy_clients(m_display);
        wl_display_destroy(m_display);
    }

    FakeCompositor(const FakeCompositor &) = delete;
    FakeCompositor &operator=(const FakeCompositor &) = delete;

    [[nodiscard]] const std::string &socket() const { return m_socket; }
    void dispatch()
    {
        wl_event_loop_dispatch(wl_display_get_event_loop(m_display), 0);
        wl_display_flush_clients(m_display);
    }
    void disconnectClients()
    {
        wl_display_destroy_clients(m_display);
    }
    void sendIdled()
    {
        if (m_notification != nullptr)
            ext_idle_notification_v1_send_idled(m_notification);
        wl_display_flush_clients(m_display);
    }
    void sendResumed()
    {
        if (m_notification != nullptr)
            ext_idle_notification_v1_send_resumed(m_notification);
        wl_display_flush_clients(m_display);
    }

    int created = 0;         // notifications asked for
    int alive = 0;           // notifications that exist now
    bool lastWasInput = false;
    std::uint32_t lastTimeout = 0;

private:
    static void bindSeat(wl_client *client, void * /*data*/, std::uint32_t version, std::uint32_t id)
    {
        wl_resource *resource = wl_resource_create(client, &wl_seat_interface, static_cast<int>(version), id);
        wl_resource_set_implementation(resource, nullptr, nullptr, nullptr);
    }

    static void bindNotifier(wl_client *client, void *data, std::uint32_t version, std::uint32_t id)
    {
        wl_resource *resource = wl_resource_create(client, &ext_idle_notifier_v1_interface, static_cast<int>(version), id);
        static const struct ext_idle_notifier_v1_interface implementation = {
            .destroy = &FakeCompositor::destroyResource,
            .get_idle_notification = &FakeCompositor::getIdle,
            .get_input_idle_notification = &FakeCompositor::getInputIdle,
        };
        wl_resource_set_implementation(resource, &implementation, data, nullptr);
    }

    static void destroyResource(wl_client * /*client*/, wl_resource *resource) { wl_resource_destroy(resource); }

    static void notificationGone(wl_resource *resource)
    {
        auto *self = static_cast<FakeCompositor *>(wl_resource_get_user_data(resource));
        --self->alive;
        if (self->m_notification == resource)
            self->m_notification = nullptr;
    }

    void makeNotification(wl_client *client, wl_resource *notifier, std::uint32_t id, std::uint32_t timeout, bool input)
    {
        wl_resource *resource = wl_resource_create(client, &ext_idle_notification_v1_interface, wl_resource_get_version(notifier), id);
        static const struct ext_idle_notification_v1_interface implementation = {.destroy = &FakeCompositor::destroyResource};
        wl_resource_set_implementation(resource, &implementation, this, &FakeCompositor::notificationGone);
        m_notification = resource;
        ++created;
        ++alive;
        lastWasInput = input;
        lastTimeout = timeout;
    }

    static void getIdle(wl_client *client, wl_resource *notifier, std::uint32_t id, std::uint32_t timeout, wl_resource * /*seat*/)
    {
        static_cast<FakeCompositor *>(wl_resource_get_user_data(notifier))->makeNotification(client, notifier, id, timeout, false);
    }

    static void getInputIdle(wl_client *client, wl_resource *notifier, std::uint32_t id, std::uint32_t timeout, wl_resource * /*seat*/)
    {
        static_cast<FakeCompositor *>(wl_resource_get_user_data(notifier))->makeNotification(client, notifier, id, timeout, true);
    }

    wl_display *m_display;
    std::string m_socket;
    std::unique_ptr<QSocketNotifier> m_notifier;
    wl_resource *m_notification = nullptr;
};

} // namespace

class TestWaylandInput : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_runtime;

private Q_SLOTS:
    void initTestCase()
    {
        // The sockets of the compositor and of the client are in a folder of their own, as a session has its XDG_RUNTIME_DIR.
        QVERIFY(m_runtime.isValid());
        QVERIFY(QFile::setPermissions(m_runtime.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
    }

    void nothingIsAskedOfTheCompositorUntilAWatchBegins()
    {
        const FakeCompositor compositor(2);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTest::qWait(150);
        QVERIFY(source->supported());
        QVERIFY(!source->watching());
        QCOMPARE(compositor.created, 0); // nothing is observed in the meantime
    }

    void aWatchAsksForTheNotificationOfInputAndAnActionAfterBeingIdleIsSeenOnceAndEndsIt()
    {
        FakeCompositor compositor(2);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTest::qWait(150);
        const QSignalSpy activity(source.get(), &galleond::InputActivitySource::activity);
        source->start();
        QVERIFY(source->watching());
        QTRY_COMPARE_WITH_TIMEOUT(compositor.created, 1, 2000);
        QVERIFY(compositor.lastWasInput); // the version 2 one: the input of the person, whatever keeps the screen awake
        QCOMPARE(compositor.lastTimeout, WaylandInputActivity::TimeoutMs);

        compositor.sendIdled();
        QTest::qWait(100);
        QCOMPARE(activity.count(), 0); // idle is not an action
        compositor.sendResumed();
        QTRY_COMPARE_WITH_TIMEOUT(activity.count(), 1, 2000);
        QVERIFY(!source->watching());
        QTRY_COMPARE_WITH_TIMEOUT(compositor.alive, 0, 2000); // the compositor watches nothing for us any more
        compositor.sendResumed();
        QTest::qWait(100);
        QCOMPARE(activity.count(), 1); // once
    }

    void beingUsedWithoutHavingBeenIdleIsNotAnAction()
    {
        FakeCompositor compositor(2);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTest::qWait(150);
        const QSignalSpy activity(source.get(), &galleond::InputActivitySource::activity);
        source->start();
        QTRY_COMPARE_WITH_TIMEOUT(compositor.created, 1, 2000);
        compositor.sendResumed();
        QTest::qWait(150);
        QCOMPARE(activity.count(), 0);
        QVERIFY(source->watching());
    }

    void stoppingDestroysTheNotificationAndWhatFollowsIsNotSeen()
    {
        FakeCompositor compositor(2);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTest::qWait(150);
        const QSignalSpy activity(source.get(), &galleond::InputActivitySource::activity);
        source->start();
        QTRY_COMPARE_WITH_TIMEOUT(compositor.alive, 1, 2000);
        compositor.sendIdled();
        QTest::qWait(50);
        source->stop();
        QVERIFY(!source->watching());
        QTRY_COMPARE_WITH_TIMEOUT(compositor.alive, 0, 2000);
        compositor.sendResumed();
        QTest::qWait(100);
        QCOMPARE(activity.count(), 0);
    }

    void aSecondWatchAfterAnActionWorksTheSame()
    {
        FakeCompositor compositor(2);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTest::qWait(150);
        const QSignalSpy activity(source.get(), &galleond::InputActivitySource::activity);
        for (int round = 1; round <= 2; ++round) {
            source->start();
            QTRY_COMPARE_WITH_TIMEOUT(compositor.created, round, 2000);
            compositor.sendIdled();
            QTest::qWait(50);
            compositor.sendResumed();
            QTRY_COMPARE_WITH_TIMEOUT(activity.count(), round, 2000);
        }
    }

    void startingBeforeTheCompositorHasSaidWhatItHasArmsTheNotificationWhenItHas()
    {
        const FakeCompositor compositor(2);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        source->start(); // the globals are not known yet
        QVERIFY(source->watching());
        QTRY_COMPARE_WITH_TIMEOUT(compositor.created, 1, 2000);
    }

    void aCompositorWithTheFirstVersionIsAskedForTheOrdinaryNotification()
    {
        const FakeCompositor compositor(1);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTest::qWait(150);
        source->start();
        QTRY_COMPARE_WITH_TIMEOUT(compositor.created, 1, 2000);
        QVERIFY(!compositor.lastWasInput);
    }

    void aCompositorWithoutTheIdleNotifierIsKnownAsUnsupportedAndSaysSoOnceWhenAskedToWatch()
    {
        const FakeCompositor compositor(0);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTRY_VERIFY_WITH_TIMEOUT(!source->supported(), 2000);
        const QSignalSpy unavailable(source.get(), &galleond::InputActivitySource::unavailable);
        source->start();
        source->start();
        QCOMPARE(unavailable.count(), 1);
        QVERIFY(!source->watching());
        QCOMPARE(compositor.created, 0);
    }

    void aCompositorWithoutASeatIsUnsupportedToo()
    {
        const FakeCompositor compositor(2, false);
        const auto source = WaylandInputActivity::connectToSession(compositor.socket().c_str());
        QVERIFY(source);
        QTRY_VERIFY_WITH_TIMEOUT(!source->supported(), 2000);
    }

    void aCompositorThatIsLostWhileWatchingSaysUnavailableOnce()
    {
        auto compositor = std::make_unique<FakeCompositor>(2);
        const auto source = WaylandInputActivity::connectToSession(compositor->socket().c_str());
        QVERIFY(source);
        QTest::qWait(150);
        const QSignalSpy unavailable(source.get(), &galleond::InputActivitySource::unavailable);
        source->start();
        QTRY_COMPARE_WITH_TIMEOUT(compositor->alive, 1, 2000);
        compositor->disconnectClients();
        QTRY_COMPARE_WITH_TIMEOUT(unavailable.count(), 1, 2000);
        QVERIFY(!source->watching());
        source->start(); // nothing more can be said
        QTest::qWait(100);
        QCOMPARE(unavailable.count(), 1);
    }

    void theSystemSourceAsksTheCompositorOfTheSessionWhenItHasTheNotificationAndPassesOnTheAction()
    {
        FakeCompositor compositor(2);
        qputenv("WAYLAND_DISPLAY", compositor.socket().c_str());
        const auto source = galleond::makeInputActivitySource();
        QTest::qWait(150);
        const QSignalSpy activity(source.get(), &galleond::InputActivitySource::activity);
        source->start();
        QVERIFY(source->watching());
        QTRY_COMPARE_WITH_TIMEOUT(compositor.created, 1, 2000);
        compositor.sendIdled();
        QTest::qWait(50);
        compositor.sendResumed();
        QTRY_COMPARE_WITH_TIMEOUT(activity.count(), 1, 2000);
        QVERIFY(!source->watching());
        source->stop(); // after the end: nothing to do, nothing wrong
        qunsetenv("WAYLAND_DISPLAY");
    }

    void theSystemSourceReadsTheIdleTimeInsteadWhenTheCompositorHasNoNotification()
    {
        // No notifier in the compositor, and no service that gives the idle time on the bus of the test: the reading says it cannot, once.
        const FakeCompositor compositor(0);
        qputenv("WAYLAND_DISPLAY", compositor.socket().c_str());
        const auto source = galleond::makeInputActivitySource();
        QTest::qWait(150);
        const QSignalSpy unavailable(source.get(), &galleond::InputActivitySource::unavailable);
        source->start();
        QTRY_COMPARE_WITH_TIMEOUT(unavailable.count(), 1, 5000);
        QCOMPARE(compositor.created, 0); // the compositor was not asked
        source->stop();
        qunsetenv("WAYLAND_DISPLAY");
    }

    void thereIsNothingToConnectToWithoutACompositor()
    {
        QVERIFY(!WaylandInputActivity::connectToSession("no-such-compositor"));
    }
};

QTEST_GUILESS_MAIN(TestWaylandInput)
#include "test_waylandinput.moc"
