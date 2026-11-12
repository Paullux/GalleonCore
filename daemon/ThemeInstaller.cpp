// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeInstaller.h"

#include "DesktopEntries.h"
#include "InitialPages.h"
#include "PagesConfig.h"
#include "PagesEditor.h"
#include "ThemeInstallPlan.h"
#include "ThemePackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace galleond {

namespace {

ThemeInstallResult failure(const QStringList &errors)
{
    ThemeInstallResult result;
    result.errors = errors;
    return result;
}

// The theme file is read whole but only up to the limit, and only a regular file.
bool readThemeFile(const QString &path, QByteArray &out, QString &problem)
{
    const QFileInfo info(path);
    if (!info.isFile()) {
        problem = QStringLiteral("%1 is not a file").arg(path);
        return false;
    }
    if (info.size() > MaxZipArchiveBytes) {
        problem = QStringLiteral("the file is larger than %1 MiB").arg(MaxZipArchiveBytes / ZipMiB);
        return false;
    }
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        problem = QStringLiteral("cannot read %1: %2").arg(path, in.errorString());
        return false;
    }
    out = in.read(MaxZipArchiveBytes + 1);
    return true;
}

PagesConfig currentConfig(const QString &pagesFile)
{
    QByteArray bytes;
    QFile in(pagesFile);
    if (in.open(QIODevice::ReadOnly))
        bytes = in.read(MaxPagesFileBytes + 1);
    else
        bytes = preparePagesJson(initialPagesJson());
    const PagesParseResult parsed = parsePagesConfig(bytes);
    return parsed.config.value_or(PagesConfig{});
}

ThemeInstallContext contextFor(const ThemeInstallOptions &options)
{
    const PagesConfig config = currentConfig(options.pagesFile);
    ThemeInstallContext context;
    context.existingPages = config.pages.keys();
    for (const Theme &theme : config.themes)
        context.existingThemeNames.push_back(theme.name);
    context.existingThemeCount = static_cast<int>(config.themes.size());
    context.existingIconFiles = QDir(QFileInfo(options.pagesFile).absolutePath()).entryList(QDir::Files);
    context.existingWallpaperStyles = QDir(options.wallpaperRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    context.resolveDesktop = [](const QString &id) -> QString {
        const DesktopApp app = DesktopEntries::lookup(id);
        return app.isValid() ? app.id : QString();
    };
    context.applyDials = options.applyDials;
    return context;
}

// Creates a new file: an existing one (whatever the case on a case-insensitive volume) is never replaced,
// so a failed install can take away only what it made.
bool writeFile(const QString &path, const QByteArray &data, QString &problem)
{
    if (QFileInfo::exists(path)) {
        problem = QStringLiteral("%1 already exists: it is not replaced").arg(path);
        return false;
    }
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        problem = QStringLiteral("cannot write %1: %2").arg(path, out.errorString());
        return false;
    }
    out.write(data);
    out.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    if (!out.commit()) {
        problem = QStringLiteral("cannot write %1: %2").arg(path, out.errorString());
        return false;
    }
    return true;
}

// Writes the pictures of a plan; `written` lists the paths made, to take away on failure.
bool writePictures(const ThemeInstallPlan &plan, const ThemeInstallOptions &options, QStringList &written, QString &problem)
{
    const QString iconFolder = QFileInfo(options.pagesFile).absolutePath();
    QDir().mkpath(iconFolder);
    for (const ZipEntry &icon : plan.iconFiles) {
        const QString path = QDir(iconFolder).filePath(icon.name);
        if (!writeFile(path, icon.data, problem))
            return false;
        written.push_back(path);
    }
    if (plan.wallpaperFiles.empty())
        return true;
    const QString folder = QDir(options.wallpaperRoot).filePath(plan.wallpaperStyle);
    QDir().mkpath(folder);
    for (const ZipEntry &wallpaper : plan.wallpaperFiles) {
        const QString path = QDir(folder).filePath(wallpaper.name);
        if (!writeFile(path, wallpaper.data, problem))
            return false;
        written.push_back(path);
    }
    return true;
}

void takeAway(const QStringList &paths, const ThemeInstallPlan &plan, const ThemeInstallOptions &options)
{
    for (const QString &path : paths)
        QFile::remove(path);
    if (!plan.wallpaperStyle.isEmpty())
        QDir(options.wallpaperRoot).rmdir(plan.wallpaperStyle); // only if empty
}

} // namespace

ThemeInstallResult installTheme(const ThemeInstallOptions &options)
{
    QByteArray bytes;
    QString problem;
    if (!readThemeFile(options.themeFile, bytes, problem))
        return failure({problem});
    const ThemeParseResult parsed = parseThemePackage(bytes);
    if (!parsed.package)
        return failure(parsed.errors);

    const ThemeInstallPlan plan = planThemeInstall(*parsed.package, contextFor(options));
    if (!plan.ok())
        return failure(plan.errors);

    QStringList written;
    if (!writePictures(plan, options, written, problem)) {
        takeAway(written, plan, options);
        return failure({problem});
    }
    int themeIndex = -1;
    const EditResult edited = PagesEditor::installTheme(options.pagesFile, plan, themeIndex);
    if (!edited.ok) {
        takeAway(written, plan, options);
        return failure(edited.errors);
    }
    ThemeInstallResult result;
    result.ok = true;
    result.warnings = plan.warnings;
    result.page = plan.page;
    result.themeName = plan.themeName;
    result.themeIndex = themeIndex;
    return result;
}

} // namespace galleond
