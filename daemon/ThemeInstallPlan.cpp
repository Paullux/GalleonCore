// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ThemeInstallPlan.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace galleond {

namespace {

constexpr int MaxIconFileNameLength = 64; // PagesConfig's rule for a picture's file name

// A file name made unique by a number before its extension (the extension says what the file is).
QString uniqueFileName(const QString &name, const QStringList &taken, int maxLength)
{
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    const QString extension = dot > 0 ? name.mid(dot) : QString();
    const QString stem = dot > 0 ? name.left(dot) : name;
    for (int n = 1; n < 1000; ++n) {
        const QString suffix = n == 1 ? QString() : QLatin1Char('-') + QString::number(n);
        const int room = maxLength - static_cast<int>(extension.size()) - static_cast<int>(suffix.size());
        const QString candidate = stem.left(room) + suffix + extension;
        if (!taken.contains(candidate))
            return candidate;
    }
    return name.left(maxLength);
}

struct Renamer
{
    QString page;
    QStringList taken; // icon file names in use, grows as names are given out
    QHash<QString, QString> given; // old name -> new name
    std::vector<ZipEntry> files;
    const std::vector<ZipEntry> *sources = nullptr;

    QString rename(const QString &name)
    {
        const auto known = given.constFind(name);
        if (known != given.constEnd())
            return known.value();
        const QString fresh = uniqueFileName(page + QLatin1Char('-') + name, taken, MaxIconFileNameLength);
        taken.push_back(fresh);
        given.insert(name, fresh);
        for (const ZipEntry &source : *sources) {
            if (source.name == name)
                files.push_back({fresh, source.data});
        }
        return fresh;
    }
};

// A picture name that is a file of the package (it has an extension) is renamed;
// one that is an icon theme name stays as it is.
QJsonValue renamedIcon(const QJsonValue &icon, Renamer &renamer)
{
    const auto one = [&renamer](const QString &name) { return name.contains(QLatin1Char('.')) ? renamer.rename(name) : name; };
    if (icon.isString())
        return one(icon.toString());
    if (!icon.isObject())
        return icon;
    QJsonObject states = icon.toObject();
    for (auto it = states.begin(); it != states.end(); ++it)
        it.value() = one(it.value().toString());
    return states;
}

QString chooseDesktop(const QString &first, const QStringList &alternatives, const ThemeInstallContext &context, QStringList &warnings)
{
    QStringList candidates{first};
    candidates.append(alternatives);
    if (context.resolveDesktop) {
        for (const QString &candidate : std::as_const(candidates)) {
            const QString resolved = context.resolveDesktop(candidate);
            if (!resolved.isEmpty())
                return resolved;
        }
    }
    warnings.push_back(QStringLiteral("%1 does not seem to be installed here").arg(first));
    return first;
}

QString hex(std::uint32_t rgb)
{
    return QStringLiteral("#%1").arg(rgb & 0xFFFFFFU, 6, 16, QLatin1Char('0'));
}

} // namespace

QString uniqueName(const QString &base, const QStringList &taken, int maxLength, const QString &separator)
{
    if (!taken.contains(base.left(maxLength)))
        return base.left(maxLength);
    for (int n = 2; n < 1000; ++n) {
        const QString suffix = separator + QString::number(n);
        const QString candidate = base.left(maxLength - static_cast<int>(suffix.size())) + suffix;
        if (!taken.contains(candidate))
            return candidate;
    }
    return base.left(maxLength); // unreachable in practice: the caller checks the result against `taken` again
}

ThemeInstallPlan planThemeInstall(const ThemePackage &package, const ThemeInstallContext &context)
{
    ThemeInstallPlan plan;
    if (context.existingThemeCount >= MaxThemes) {
        plan.errors.push_back(QStringLiteral("there are already %1 themes: remove one first").arg(MaxThemes));
        return plan;
    }
    plan.page = uniqueName(package.meta.id, context.existingPages, 32, QStringLiteral("-"));
    if (context.existingPages.contains(plan.page)) {
        plan.errors.push_back(QStringLiteral("no free page name for this theme"));
        return plan;
    }
    const QString name = package.config.themes.empty() ? package.meta.id : package.config.themes.front().name;
    plan.themeName = uniqueName(name, context.existingThemeNames, MaxLabelLength, QStringLiteral(" "));
    plan.color = hex(package.config.themes.empty() ? 0 : package.config.themes.front().rgb);

    Renamer renamer;
    renamer.page = plan.page;
    renamer.taken = context.existingIconFiles;
    renamer.sources = &package.icons;

    QJsonArray keys;
    for (int i = 0; i < package.keysJson.size(); ++i) {
        QJsonObject key = package.keysJson.at(i).toObject();
        if (key.contains(QStringLiteral("icon")))
            key.insert(QStringLiteral("icon"), renamedIcon(key.value(QStringLiteral("icon")), renamer));
        QJsonObject action = key.value(QStringLiteral("action")).toObject();
        const QString type = action.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("app")) {
            action.insert(QStringLiteral("desktop"),
                          chooseDesktop(action.value(QStringLiteral("desktop")).toString(), package.appAlternatives.value(i), context, plan.warnings));
        } else if (type == QLatin1String("page") && action.value(QStringLiteral("page")).toString() == package.meta.id) {
            action.insert(QStringLiteral("page"), plan.page); // its own page, whatever the page ends up named
        }
        key.insert(QStringLiteral("action"), action);
        keys.append(key);
    }
    plan.pageObject = QJsonObject{{QStringLiteral("keys"), keys}};
    plan.iconFiles = renamer.files;
    if (context.applyDials)
        plan.dials = package.dialsJson;

    if (!package.wallpapers.empty()) {
        plan.wallpaperStyle = uniqueName(plan.page, context.existingWallpaperStyles, 40, QStringLiteral("-"));
        int n = 1;
        for (const ZipEntry &wallpaper : package.wallpapers) {
            plan.wallpaperFiles.push_back({QStringLiteral("%1-%2.jpg").arg(plan.wallpaperStyle).arg(n, 2, 10, QLatin1Char('0')), wallpaper.data});
            ++n;
        }
        plan.wallpaper = plan.wallpaperStyle + QLatin1Char('/') + plan.wallpaperFiles.front().name;
    }
    return plan;
}

} // namespace galleond
