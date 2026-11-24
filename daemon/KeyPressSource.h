// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QtGlobal>

namespace galleond {

// Something that says which keys of the keyboard (1b1c:2b0c) are down, for the reactive glow, and only while it is asked to watch. It is the ONE place of
// galleond where key presses are seen, so it follows the rules of AGENTS.md ("Security and privacy by design"):
//   - OFF unless started: nothing is read between stop() and start(), and whoever starts it does so only when the person chose the effect (opt-in, state shown
//     in Bosun) AND the live lighting is on. A source never starts itself.
//   - INTERNAL: it is not registered on D-Bus or on the local endpoint, and its signals are never connected to anything that is. The usage of a key that went
//     down is given to the glow at once, and the glow only keeps the LED and a time, bounded (ReactiveGlow.h). Nothing is stored, logged or sent: no order, no
//     count, no history. A report that is not a key report is dropped, with one warning that gives its ID and length and never a byte of it.
//   - NO STATE of its own about the keys: each report says what is down at that moment, and every key that is down is said again with each report (so a key
//     held while another is pressed is said again, and glows again). That is on purpose: remembering what was down before would be a record of the keys.
class KeyPressSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool watching() const = 0;

Q_SIGNALS:
    // A USB HID usage (KeyIdentity.h) that is down in the report just read. Within galleond only.
    void keyDown(quint16 usage);
    // Said once, with no detail, when the source cannot read (no node, no access, the keyboard gone). Watching is over when it is said.
    void unavailable();
};

} // namespace galleond
