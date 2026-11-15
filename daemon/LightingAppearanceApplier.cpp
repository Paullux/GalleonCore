// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "LightingAppearanceApplier.h"

#include "AppearanceFile.h"
#include "LightingController.h"

#include <QDebug>

#include <utility>

namespace galleond {

LightingAppearanceApplier::LightingAppearanceApplier(LightingController &controller, QString path, QObject *parent)
    : QObject(parent), m_controller(controller), m_path(std::move(path)), m_watcher(m_path)
{
    connect(&controller, &LightingController::activeChanged, this, &LightingAppearanceApplier::onActiveChanged);
    connect(&m_watcher, &ReplacedFileWatcher::maybeChanged, this, &LightingAppearanceApplier::onFileMaybeChanged);
}

bool LightingAppearanceApplier::applyFile()
{
    QString why;
    const auto appearance = readAppearance(m_path, &why);
    if (!appearance) {
        qWarning().noquote() << "galleond: lighting: the appearance file is not used:" << why;
        return false;
    }
    return m_controller.setAppearance(*appearance);
}

void LightingAppearanceApplier::onActiveChanged(bool active)
{
    if (!active)
        return;
    // Switched on: the file if there is a good one, else dark. Not what was shown the last time the lighting was on.
    if (!applyFile())
        static_cast<void>(m_controller.setAppearance(galleon::keyboard::Appearance{}));
}

void LightingAppearanceApplier::onFileMaybeChanged()
{
    if (!m_controller.active())
        return; // read when the lighting is switched on
    static_cast<void>(applyFile()); // an invalid file leaves what is shown as it is
}

} // namespace galleond
