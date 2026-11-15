// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ReplacedFileWatcher.h"

#include <QObject>
#include <QString>

namespace galleond {

class LightingController;

// Applies lighting-appearance.json (AppearanceFile.h) to the live lighting: read when the lighting is switched on, and again whenever the file changes
// while it is on. Like the other files that the settings window writes and galleond applies (brightness-curve.json), the file is watched by the folder
// that holds it, and what comes of it is untrusted:
//  - switched on, a missing or invalid file means the keys are dark (as when there has never been a file), and the reason is logged (never the content);
//  - while on, a file that becomes invalid is IGNORED and what is shown stays (a half-written or damaged file must not make the keys flash dark);
//  - while off, changes are not read: the file is read when the lighting is switched on.
// A file never switches the lighting on, and it carries nothing about key presses. The controller does not know this object; it only listens to the
// controller's signal and calls its setAppearance().
class LightingAppearanceApplier : public QObject
{
    Q_OBJECT

public:
    LightingAppearanceApplier(LightingController &controller, QString path, QObject *parent = nullptr);

public Q_SLOTS:
    // The lighting was switched on or off.
    void onActiveChanged(bool active);
    // The folder or the file changed (or a test says so): read the file again if the lighting is on.
    void onFileMaybeChanged();

private:
    // True when a valid appearance was read from the file and applied.
    bool applyFile();

    LightingController &m_controller;
    QString m_path;
    ReplacedFileWatcher m_watcher;
};

} // namespace galleond
