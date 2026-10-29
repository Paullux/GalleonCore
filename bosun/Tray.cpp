// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Tray.h"
#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QStringList>
#include <QVariantMap>
#endif

namespace bosun {

Tray::Tray(const QIcon &icon, QObject *parent)
    : QObject(parent)
    , m_icon(icon)
{
    m_icon.setContextMenu(&m_menu);
    connect(&m_icon, &QSystemTrayIcon::messageClicked, this, &Tray::updateNotificationClicked);
    m_icon.setToolTip(QStringLiteral("Galleon Bosun"));
    // A left click shows or hides the window; the menu comes with the right click.
    connect(&m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            Q_EMIT chosen(TrayEntry::Action::ToggleWindow);
    });
}

bool Tray::isAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void Tray::setIcon(const QIcon &icon)
{
    m_icon.setIcon(icon);
}

void Tray::show()
{
    m_icon.show();
}

// Windows uses m_icon; the Linux D-Bus implementation needs no instance state.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void Tray::notifyUpdate(const QString &version, bool simulation)
{
    const QString title = simulation ? tr("GalleonCore update simulation") : tr("GalleonCore update available");
    const QString body = tr("GalleonCore %1 is available. Open Bosun's Updates panel for downloads and release notes.").arg(version);
#ifdef Q_OS_WIN
    if (m_icon.isVisible() && QSystemTrayIcon::supportsMessages())
        m_icon.showMessage(title, body, QSystemTrayIcon::Information);
#else
    // freedesktop Desktop Notifications: independent of the system tray.
    // https://specifications.freedesktop.org/notification/latest-single/
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    message.setArguments({QStringLiteral("Galleon Bosun"), QVariant::fromValue(uint(0)),
                          QStringLiteral("galleon-bosun"), title, body, QStringList{},
                          QVariantMap{{QStringLiteral("desktop-entry"), QStringLiteral("galleon-bosun")}},
                          -1});
    static_cast<void>(QDBusConnection::sessionBus().asyncCall(message));
#endif
}

void Tray::update(bool windowShown, bool daemonRunning)
{
    m_menu.clear();
    for (const TrayEntry &entry : trayEntries(windowShown, daemonRunning)) {
        QAction *item = m_menu.addAction(entry.label);
        const TrayEntry::Action action = entry.action;
        connect(item, &QAction::triggered, this, [this, action] { Q_EMIT chosen(action); });
    }
}

} // namespace bosun
