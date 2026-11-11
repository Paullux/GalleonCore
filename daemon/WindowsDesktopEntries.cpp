// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DesktopEntries.h"
#include "InitialPages.h"
#include "SteamLibrary.h"
#include "WellKnownApps.h"
#include <QFile>

#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QRegularExpression>
#include <array>
#include <cwchar>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <winver.h>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

namespace galleond {
namespace {
constexpr int MaxApps = 4096;
class ComScope final {
public:
    ComScope() = default;
    ~ComScope() { if (SUCCEEDED(m_status)) CoUninitialize(); }
    ComScope(const ComScope &) = delete;
    ComScope &operator=(const ComScope &) = delete;
    ComScope(ComScope &&) = delete;
    ComScope &operator=(ComScope &&) = delete;
private:
    HRESULT m_status = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
};

struct ShortcutInfo
{
    QString target;
    QString arguments;
    QString icon;
};

struct VersionStrings
{
    QString productName;
    QString companyName;
};

// ProductName/CompanyName from the target's own version resource, set by its
// vendor at build time: unlike a shortcut's display name, neither the user's
// language nor a renamed shortcut changes them. Empty fields when the file
// carries no version info at all (not every executable does).
VersionStrings versionStrings(const QString &exePath)
{
    VersionStrings result;
    const std::wstring path = QDir::toNativeSeparators(exePath).toStdWString();
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0)
        return result;
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path.c_str(), handle, size, data.data()))
        return result;
    struct LangCodePage
    {
        WORD language;
        WORD codePage;
    };
    LangCodePage *translations = nullptr;
    UINT translationsBytes = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void **>(&translations), &translationsBytes)
        || translationsBytes < sizeof(LangCodePage))
        return result;
    const LangCodePage lcp = translations[0];
    const auto queryString = [&](const wchar_t *name) -> QString {
        std::array<wchar_t, 64> subBlock{};
        swprintf(subBlock.data(), subBlock.size(), L"\\StringFileInfo\\%04x%04x\\%ls", lcp.language, lcp.codePage, name);
        void *value = nullptr;
        UINT length = 0;
        if (!VerQueryValueW(data.data(), subBlock.data(), &value, &length) || length == 0)
            return {};
        return QString::fromWCharArray(static_cast<const wchar_t *>(value), static_cast<int>(length) - 1); // drop the trailing NUL
    };
    result.productName = queryString(L"ProductName");
    result.companyName = queryString(L"CompanyName");
    return result;
}

QString expandedPath(const wchar_t *path)
{
    std::array<wchar_t, 32768> expanded{};
    const DWORD length = ExpandEnvironmentStringsW(path, expanded.data(), static_cast<DWORD>(expanded.size()));
    if (length == 0 || length > expanded.size())
        return {};
    return QDir::fromNativeSeparators(QString::fromWCharArray(expanded.data()));
}

ShortcutInfo shortcutInfo(const QString &file, bool includeIcon = true)
{
    const ComScope apartment;
    Microsoft::WRL::ComPtr<IShellLinkW> link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link.GetAddressOf()))))
        return {};
    Microsoft::WRL::ComPtr<IPersistFile> persist;
    if (FAILED(link.As(&persist)) || FAILED(persist->Load(QDir::toNativeSeparators(file).toStdWString().c_str(), STGM_READ)))
        return {};
    std::array<wchar_t, 32768> target{};
    if (FAILED(link->GetPath(target.data(), static_cast<int>(target.size()), nullptr, SLGP_RAWPATH)))
        return {};
    // Shell links retain Chrome/Edge profile and app IDs; never execute or resolve them here.
    // https://learn.microsoft.com/windows/win32/api/shobjidl_core/nn-shobjidl_core-ishelllinkw
    std::array<wchar_t, 32768> arguments{};
    if (FAILED(link->GetArguments(arguments.data(), static_cast<int>(arguments.size()))))
        return {};
    const QString args = QString::fromWCharArray(arguments.data());
    // GetArguments can truncate silently. Reject a full buffer rather than merging distinct apps.
    if (args.size() >= static_cast<qsizetype>(arguments.size() - 1))
        return {};
    ShortcutInfo result{expandedPath(target.data()), args, {}};
    if (!includeIcon)
        return result;
    std::array<wchar_t, 32768> icon{};
    int iconIndex = 0;
    if (SUCCEEDED(link->GetIconLocation(icon.data(), static_cast<int>(icon.size()), &iconIndex)) && iconIndex == 0) {
        const QString path = expandedPath(icon.data());
        const QFileInfo info(path);
        // A local ICO is data, not a shortcut's potentially remote/custom Shell icon handler.
        if (!path.startsWith(QStringLiteral("//")) && info.isAbsolute() && info.isFile()
            && !info.isSymbolicLink() && info.suffix().compare(QStringLiteral("ico"), Qt::CaseInsensitive) == 0
            && info.size() > 0 && info.size() <= 4 * 1024 * 1024)
            result.icon = path;
    }
    return result;
}

QString cachedIcon(const QString &file)
{
    const ComScope apartment;
    const QFileInfo info(file);
    const QByteArray identity = file.toUtf8() + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    const QString root = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/galleoncore/application-icons");
    const QString output = root + QLatin1Char('/') + QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex()) + QStringLiteral(".png");
    if (QFileInfo::exists(output))
        return output;
    SHFILEINFOW shellInfo{};
    if (!SHGetFileInfoW(QDir::toNativeSeparators(file).toStdWString().c_str(), 0, &shellInfo, static_cast<UINT>(sizeof(shellInfo)), SHGFI_ICON | SHGFI_LARGEICON)) {
        qWarning() << "galleond: Shell icon lookup failed" << file << GetLastError();
        return {};
    }
    const QImage image = QImage::fromHICON(shellInfo.hIcon);
    DestroyIcon(shellInfo.hIcon);
    if (image.isNull() || !QDir().mkpath(root)) {
        qWarning() << "galleond: Shell icon conversion/cache failed" << file << image.size() << root << GetLastError();
        return {};
    }
    QSaveFile saved(output);
    if (!saved.open(QIODevice::WriteOnly) || !image.save(&saved, "PNG") || !saved.commit()) {
        qWarning() << "galleond: icon cache write failed" << output << saved.errorString();
        return {};
    }
    return output;
}

bool usefulShortcut(const QString &name, const QString &target)
{
    const QString executable = QFileInfo(target).fileName().toLower();
    if (target.isEmpty() || target.startsWith(QStringLiteral("//")) || !QFileInfo(target).isAbsolute()
        || !target.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive) || !QFileInfo(target).isFile())
        return false;
    if (executable == QStringLiteral("wsl.exe") || executable == QStringLiteral("wslg.exe")
        || executable.startsWith(QStringLiteral("unins")) || executable.contains(QStringLiteral("uninstall")))
        return false;
    static const QRegularExpression unwanted(QStringLiteral("^(uninstall|désinstaller|desinstaller|remove|help|aide)(?:\\b|$)"), QRegularExpression::CaseInsensitiveOption);
    return !unwanted.match(name).hasMatch();
}

QStringList startMenuRoots()
{
    QStringList result;
    for (const QString &location : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        if (QFileInfo(location).isDir())
            result << QDir::cleanPath(location);
    }
    for (const KNOWNFOLDERID *id : {&FOLDERID_Programs, &FOLDERID_CommonPrograms, &FOLDERID_Desktop, &FOLDERID_PublicDesktop}) {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DEFAULT, nullptr, &path))) {
            result << QDir::fromNativeSeparators(QString::fromWCharArray(path));
            CoTaskMemFree(path);
        }
    }
    result.removeDuplicates();
    return result;
}

bool allowedFile(const QFileInfo &info)
{
    const QString suffix = info.suffix().toLower();
    return info.isAbsolute() && info.isFile() && !info.isSymbolicLink()
        && (suffix == QStringLiteral("lnk") || suffix == QStringLiteral("exe") || suffix == QStringLiteral("url"));
}

// The executable a candidate reference ultimately starts, for signature matching:
// a shortcut's own target, or the reference itself when it already is one.
QString targetExecutable(const QString &reference)
{
    if (QFileInfo(reference).suffix().compare(QStringLiteral("lnk"), Qt::CaseInsensitive) == 0)
        return shortcutInfo(reference, false).target;
    if (QFileInfo(reference).suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0)
        return reference;
    return {}; // a .url (Steam) is never a well-known application
}

bool matchesWellKnown(const QString &reference, const WellKnownApp &known)
{
    const QString target = targetExecutable(reference);
    if (target.isEmpty())
        return false;
    const VersionStrings info = versionStrings(target);
    return WellKnownApps::windowsSignatureMatches(QFileInfo(target).fileName(), info.productName, info.companyName, known);
}

// Every reference resolveWellKnown should try, in the same order list() scans them.
QStringList wellKnownCandidates()
{
    QStringList candidates;
    for (const QString &root : startMenuRoots()) {
        QDirIterator files(root, {QStringLiteral("*.lnk"), QStringLiteral("*.exe")}, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext())
            candidates.push_back(files.next());
    }
    for (const QString &hive : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
        for (const QSettings::Format format : {QSettings::NativeFormat, QSettings::Registry32Format}) {
            QSettings registered(hive + QStringLiteral("\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths"), format);
            for (const QString &group : registered.childGroups()) {
                QString path = registered.value(group + QStringLiteral("/.")).toString().trimmed();
                if (path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"')))
                    path = path.mid(1, path.size() - 2);
                if (!path.isEmpty())
                    candidates.push_back(path);
            }
        }
    }
    return candidates;
}
}

bool DesktopEntries::isValidReference(const QString &desktop)
{
    if (desktop.isEmpty() || desktop.size() > 4096 || desktop.contains(QChar::Null))
        return false;
    if (WellKnownApps::find(desktop))
        return true;
    const QFileInfo info(desktop);
    const QString suffix = info.suffix().toLower();
    return info.isAbsolute() && (suffix == QStringLiteral("lnk") || suffix == QStringLiteral("exe") || suffix == QStringLiteral("url"));
}

QString DesktopEntries::wellKnownIdFor(const QString &desktop)
{
    if (!isValidReference(desktop) || WellKnownApps::find(desktop))
        return {};
    const QString target = targetExecutable(desktop);
    if (target.isEmpty())
        return {};
    const VersionStrings info = versionStrings(target);
    const QString baseName = QFileInfo(target).fileName();
    for (const QString &id : WellKnownApps::knownIds()) {
        if (const WellKnownApp *app = WellKnownApps::find(id);
            app && WellKnownApps::windowsSignatureMatches(baseName, info.productName, info.companyName, *app))
            return id;
    }
    return {};
}

DesktopApp DesktopEntries::read(const QString &file, const QString &id)
{
    return read(file, id, true);
}

DesktopApp DesktopEntries::read(const QString &file, const QString &id, bool includeIcons)
{
    // Never resolve network targets or remote shortcut icon handlers during discovery.
    if (QDir::fromNativeSeparators(file).startsWith(QStringLiteral("//")))
        return {};
    const QFileInfo info(file);
    if (!allowedFile(info))
        return {};
    DesktopApp app;
    app.file = info.absoluteFilePath();
    app.id = id;
    app.name = info.completeBaseName();
    if (info.suffix().compare(QStringLiteral("url"), Qt::CaseInsensitive) == 0) {
        QFile shortcut(file);
        if (!shortcut.open(QIODevice::ReadOnly))
            return {};
        app.steamAppId = SteamLibrary::shortcutAppId(shortcut.read(8193));
        if (app.steamAppId <= 0)
            return {};
        app.categories = {QStringLiteral("Game")};
        if (includeIcons)
            app.iconPath = SteamLibrary::coverImage(app.steamAppId);
        return app;
    }
    const ShortcutInfo shortcut = info.suffix().compare(QStringLiteral("lnk"), Qt::CaseInsensitive) == 0
        ? shortcutInfo(file, includeIcons) : ShortcutInfo{app.file, {}, {}};
    if (!usefulShortcut(app.name, shortcut.target))
        return {};
    if (includeIcons) {
        if (!shortcut.icon.isEmpty())
            app.iconPath = cachedIcon(shortcut.icon);
        if (app.iconPath.isEmpty())
            app.iconPath = cachedIcon(shortcut.target);
    }
    app.categories = windowsAppCategories(app.name, info.absolutePath());
    return app;
}

DesktopApp DesktopEntries::lookup(const QString &desktop)
{
    if (!isValidReference(desktop))
        return {};
    if (const WellKnownApp *known = WellKnownApps::find(desktop)) {
        // A theme names an application portably: there is no ID to look up
        // directly, so every candidate list() would scan is tried in turn.
        for (const QString &candidate : wellKnownCandidates()) {
            if (!matchesWellKnown(candidate, *known))
                continue;
            if (DesktopApp app = read(candidate, candidate); app.isValid())
                return app;
        }
        return {};
    }
    return read(desktop, desktop);
}

QList<DesktopApp> DesktopEntries::list()
{
    return list(true);
}

QList<DesktopApp> DesktopEntries::list(bool includeIcons)
{
    QList<DesktopApp> apps;
    for (const QString &root : startMenuRoots()) {
        QDirIterator files(root, {QStringLiteral("*.lnk"), QStringLiteral("*.url")}, QDir::Files,
                           QDirIterator::Subdirectories);
        while (files.hasNext() && apps.size() < MaxApps) {
            const QString path = files.next();
            DesktopApp app = read(path, path, includeIcons);
            if (app.isValid())
                apps.append(std::move(app));
        }
        if (apps.size() >= MaxApps)
            break;
    }
    // App Paths includes per-user and machine installations without a Start Menu shortcut (Chrome, etc.).
    for (const QString &hive : {QStringLiteral("HKEY_CURRENT_USER"), QStringLiteral("HKEY_LOCAL_MACHINE")}) {
        for (const QSettings::Format format : {QSettings::NativeFormat, QSettings::Registry32Format}) {
            QSettings registered(hive + QStringLiteral("\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths"), format);
            for (const QString &group : registered.childGroups()) {
                if (apps.size() >= MaxApps)
                    break;
                QString path = registered.value(group + QStringLiteral("/.")).toString().trimmed();
                if (path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"')))
                    path = path.mid(1, path.size() - 2);
                DesktopApp app = read(path, path, includeIcons);
                if (app.isValid())
                    apps.append(std::move(app));
            }
        }
    }
    QSet<QString> seen;
    apps.removeIf([&seen](const DesktopApp &app) {
        if (app.steamAppId > 0) {
            const QString identity = QStringLiteral("steam:%1").arg(app.steamAppId);
            if (seen.contains(identity))
                return true;
            seen.insert(identity);
            return false;
        }
        const ShortcutInfo shortcut = app.file.endsWith(QStringLiteral(".lnk"), Qt::CaseInsensitive)
            ? shortcutInfo(app.file, false) : ShortcutInfo{app.file, {}, {}};
        // Same browser + distinct app/profile arguments are distinct launchers.
        const QString identity = shortcut.target.toCaseFolded() + QChar(QChar::Null) + shortcut.arguments;
        if (seen.contains(identity))
            return true;
        seen.insert(identity);
        return false;
    });
    std::ranges::sort(apps, [](const DesktopApp &left, const DesktopApp &right) {
        return QString::localeAwareCompare(left.name, right.name) < 0;
    });
    return apps;
}

QList<DesktopApp> DesktopEntries::listCategory(const QString &category)
{
    QList<DesktopApp> apps = list();
    apps.removeIf([&category](const DesktopApp &app) { return !app.categories.contains(category); });
    return apps;
}

bool DesktopEntries::launch(const DesktopApp &app)
{
    if (!isValidReference(app.file) || !allowedFile(QFileInfo(app.file)))
        return false;
    if (QFileInfo(app.file).suffix().compare(QStringLiteral("url"), Qt::CaseInsensitive) == 0) {
        QFile shortcut(app.file);
        if (!shortcut.open(QIODevice::ReadOnly))
            return false;
        const int id = SteamLibrary::shortcutAppId(shortcut.read(8193));
        return id > 0 && QDesktopServices::openUrl(QUrl(QStringLiteral("steam://rungameid/%1").arg(id)));
    }
    if (QFileInfo(app.file).suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0)
        return QProcess::startDetached(app.file, {});
    return QDesktopServices::openUrl(QUrl::fromLocalFile(app.file));
}
} // namespace galleond
