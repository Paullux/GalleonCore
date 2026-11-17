// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RingSettingsApplier.h"

#include "PagesController.h"
#include "RingSettings.h"

#include <QDebug>
#include <QFileInfo>

#include <utility>

namespace galleond {

RingSettingsApplier::RingSettingsApplier(PagesController &pages, QString path, QObject *parent)
    : QObject(parent), m_pages(pages), m_path(std::move(path)), m_watcher(m_path)
{
    connect(&m_watcher, &ReplacedFileWatcher::maybeChanged, this, &RingSettingsApplier::onFileMaybeChanged);
    onFileMaybeChanged(); // the file as it is at the start
}

void RingSettingsApplier::onFileMaybeChanged()
{
    if (!QFileInfo::exists(m_path))
        return; // no file: the rings as they always were (a file that was removed leaves what was applied)
    QString why;
    const auto settings = readRingSettings(m_path, &why);
    if (!settings) {
        qWarning().noquote() << "galleond: the settings of the dial rings are not used:" << why;
        return;
    }
    m_pages.setRingSettings(*settings);
}

} // namespace galleond
