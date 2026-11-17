// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ReplacedFileWatcher.h"

#include <QObject>
#include <QString>

namespace galleond {

class PagesController;

// Applies dial-rings.json (RingSettings.h) to the rings of the module: read at the start and whenever the file changes. Like the other files that
// the settings window writes and galleond applies (lighting-appearance.json), the file is watched by the folder that holds it, and what comes of it
// is untrusted:
//  - no file (or one that never was good) means the rings as they always were: lit, full level;
//  - a file that becomes invalid is IGNORED and what is shown stays (a half-written or damaged file must not make the rings flash dark), and the
//    reason is logged (never the content).
// The file carries nothing about the keyboard and nothing about key presses. The controller does not know this object.
class RingSettingsApplier : public QObject
{
    Q_OBJECT

public:
    RingSettingsApplier(PagesController &pages, QString path, QObject *parent = nullptr);

public Q_SLOTS:
    // The folder or the file changed (or a test says so): read the file again.
    void onFileMaybeChanged();

private:
    PagesController &m_pages;
    QString m_path;
    ReplacedFileWatcher m_watcher;
};

} // namespace galleond
