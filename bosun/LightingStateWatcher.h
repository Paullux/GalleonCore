// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LightingState.h>

#include <ReplacedFileWatcher.h>

#include <QObject>
#include <QString>

#include <optional>

namespace bosun {

// Watches lighting-state.json (LightingState.h) and says when galleond wrote a new, fresh state of the live lighting.
// This window never talks to the keyboard: this is how the state gets here on Windows, where the daemon's local endpoint
// has no reply. Anything that does not parse is ignored, and so is a state stamped before `notBefore` (when this window
// started): it describes an earlier session. The same watching of a replaced file as for the brightness.
class LightingStateWatcher : public QObject
{
    Q_OBJECT

public:
    LightingStateWatcher(const QString &path, qint64 notBefore, QObject *parent = nullptr);

    // Reads the file now and emits if it holds a fresh state that was not already reported. Public for tests.
    void refresh();
    [[nodiscard]] std::optional<galleond::LightingState> last() const { return m_last; }

Q_SIGNALS:
    void stateChanged(const QString &state, const QString &detail);

private:
    QString m_path;
    qint64 m_notBefore;
    galleond::ReplacedFileWatcher m_watcher;
    std::optional<galleond::LightingState> m_last;
};

} // namespace bosun
