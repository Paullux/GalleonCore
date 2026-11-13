// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <KeyboardState.h>

#include <ReplacedFileWatcher.h>

#include <QObject>
#include <QString>

#include <optional>

namespace bosun {

// Watches the file galleond writes after each read or write of the keyboard's brightness (KeyboardState.h) and
// says when it holds a new value. This window never talks to the keyboard: this is how the value gets here on
// Windows, where the daemon's local endpoint has no reply.
//
// The file is untrusted input: anything that does not parse is ignored (logged by nobody, shown by nothing), and
// a value stamped before `notBefore` (when this window started) is ignored too, since it describes an earlier
// session and another program may have changed the keyboard since. The file is replaced by a rename on every
// write: galleond::ReplacedFileWatcher deals with that and with a folder that does not exist yet.
class KeyboardStateWatcher : public QObject
{
    Q_OBJECT

public:
    KeyboardStateWatcher(const QString &path, qint64 notBefore, QObject *parent = nullptr);

    // Reads the file now and emits if it holds a fresh value that was not already reported. Public for tests.
    void refresh();

    // The last fresh value that was reported (or found at start), if any.
    [[nodiscard]] std::optional<int> brightness() const;

Q_SIGNALS:
    // The brightness galleond saw, 0 to 1000 as the keyboard reported it (not rounded to a slider stop).
    void brightnessChanged(int value);

private:
    QString m_path;
    qint64 m_notBefore;
    galleond::ReplacedFileWatcher m_watcher;
    std::optional<galleond::KeyboardState> m_last;
};

} // namespace bosun
