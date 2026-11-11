// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilePicker.h"

#include <QFileDialog>
#include <QStandardPaths>

namespace bosun {

QString FilePicker::picture() // NOLINT(readability-convert-member-functions-to-static): QML invokes this QObject method.
{
    return QFileDialog::getOpenFileName(nullptr, tr("Choose a picture"),
                                       QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                       tr("Pictures (*.jpg *.jpeg *.png *.webp *.bmp);;All files (*)"));
}

QStringList FilePicker::pictures() // NOLINT(readability-convert-member-functions-to-static): QML invokes this QObject method.
{
    return QFileDialog::getOpenFileNames(nullptr, tr("Choose wallpapers"),
                                        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                                        tr("Pictures (*.jpg *.jpeg *.png *.webp *.bmp);;All files (*)"));
}

QString FilePicker::video() // NOLINT(readability-convert-member-functions-to-static): QML invokes this QObject method.
{
    return QFileDialog::getOpenFileName(nullptr, tr("Choose a video"),
                                       QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
                                       tr("Videos (*.mp4 *.mkv *.webm *.avi *.mov *.m4v *.mpeg *.mpg *.ogv);;All files (*)"));
}

QString FilePicker::application() // NOLINT(readability-convert-member-functions-to-static): QML invokes this QObject method.
{
#ifdef Q_OS_WIN
    const QString filter = tr("Applications (*.exe *.lnk *.url)");
#else
    const QString filter = tr("Applications (*.desktop)");
#endif
    return QFileDialog::getOpenFileName(nullptr, tr("Choose an application"),
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation), filter);
}

QString FilePicker::theme() // NOLINT(readability-convert-member-functions-to-static): QML invokes this QObject method.
{
    return QFileDialog::getOpenFileName(nullptr, tr("Choose a theme file"),
                                       QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                                       tr("GalleonCore themes (*.gctheme);;All files (*)"));
}

QString FilePicker::themeSave(const QString &name) // NOLINT(readability-convert-member-functions-to-static): QML invokes this QObject method.
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QFileDialog::getSaveFileName(nullptr, tr("Save the theme"), folder + QLatin1Char('/') + name + QStringLiteral(".gctheme"),
                                        tr("GalleonCore themes (*.gctheme)"));
}

} // namespace bosun
