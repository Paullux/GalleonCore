// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileStore.h"
#include "Profiles.h"

#include <PagesConfig.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

namespace {

bool put(const QString &path, const QByteArray &content)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

QByteArray get(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<missing>");
}

} // namespace

class TestProfileStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aDeletedFileNameKeepsTheNameAndTheTimeAndNeverReusesOne()
    {
        QCOMPARE(deletedProfileFileName("gaming", "20261128-180000", 1), QStringLiteral("gaming-20261128-180000.json"));
        QCOMPARE(deletedProfileFileName("gaming", "20261128-180000", 2), QStringLiteral("gaming-20261128-180000-2.json"));
        QVERIFY(deletedProfileFileName("a", "s", 1) != deletedProfileFileName("a", "s", 2));
    }

    void aProfileIsACopyOfAnotherOne()
    {
        const QTemporaryDir dir;
        QVERIFY(put(dir.path() + "/pages.json", "base"));
        QVERIFY(put(dir.path() + "/profiles/work.json", "work"));
        const ProfileResult fromDefault = createProfile(dir.path(), "gaming", "default", "builtin");
        QVERIFY2(fromDefault.ok, qPrintable(fromDefault.error));
        QCOMPARE(get(dir.path() + "/profiles/gaming.json"), QByteArray("base"));
        QVERIFY(createProfile(dir.path(), "desk", "work", {}).ok);
        QCOMPARE(get(dir.path() + "/profiles/desk.json"), QByteArray("work"));
        QCOMPARE(listProfiles(dir.path()), (QStringList{"default", "desk", "gaming", "work"}));
        // The copy is the person's own. On Linux that is not writable by the group or the others (the pages file must be, to be trusted). On Windows
        // Qt gives the permissions of the owner for everyone (the access is decided by the ACL, which is the business of WindowsFileTrust), so only
        // the owner's right is looked at there.
        const QFileDevice::Permissions mode = QFile::permissions(dir.path() + "/profiles/gaming.json");
#ifndef Q_OS_WIN
        QVERIFY(!(mode & (QFileDevice::WriteGroup | QFileDevice::WriteOther)));
#endif
        QVERIFY(mode & QFileDevice::WriteOwner);
        QCOMPARE(get(dir.path() + "/pages.json"), QByteArray("base")); // the original is not touched
    }

    void theBaseProfileWithoutAFileIsCopiedFromTheBuiltInPages()
    {
        const QTemporaryDir dir;
        QVERIFY(createProfile(dir.path(), "first", "default", "builtin").ok);
        QCOMPARE(get(dir.path() + "/profiles/first.json"), QByteArray("builtin"));
        QVERIFY(!createProfile(dir.path(), "second", "default", {}).ok); // nothing to copy and no built-in given
        QVERIFY(!QFileInfo::exists(dir.path() + "/profiles/second.json"));
        QVERIFY(!createProfile(dir.path(), "third", "nope", "builtin").ok); // only the base profile may be missing
    }

    void aProfileThatCannotBeMadeIsNotMade()
    {
        const QTemporaryDir dir;
        QVERIFY(put(dir.path() + "/pages.json", "base"));
        QVERIFY(put(dir.path() + "/profiles/work.json", "work"));
        for (const QString &bad : {QStringLiteral("default"), QStringLiteral("../pages"), QStringLiteral("Gaming"), QStringLiteral("a/b"), QStringLiteral(""),
                                   QStringLiteral(".deleted"), QStringLiteral("work") /* taken */}) {
            const ProfileResult result = createProfile(dir.path(), bad, "default", "builtin");
            QVERIFY2(!result.ok, qPrintable(bad));
            QVERIFY(!result.error.isEmpty());
        }
        QCOMPARE(get(dir.path() + "/profiles/work.json"), QByteArray("work")); // a taken name does not overwrite
        QCOMPARE(get(dir.path() + "/pages.json"), QByteArray("base"));
        QVERIFY(!createProfile(dir.path(), "copy", "../pages", "builtin").ok); // a source that is not a valid name is not read
        QVERIFY(!QDir(dir.path() + "/profiles").exists("../pages.json") || QFileInfo::exists(dir.path() + "/pages.json"));
        QVERIFY(put(dir.path() + "/profiles/big.json", QByteArray(MaxPagesFileBytes + 1, ' ')));
        QVERIFY(!createProfile(dir.path(), "toobig", "big", {}).ok); // larger than a pages file may be
    }

    // The window says why in the language of the person: it needs the reason as a code, not only as English text.
    void aRefusalSaysWhyByItsCode()
    {
        const QTemporaryDir dir;
        QVERIFY(put(dir.path() + "/pages.json", "base"));
        QVERIFY(put(dir.path() + "/profiles/work.json", "work"));
        QVERIFY(put(dir.path() + "/profiles/big.json", QByteArray(MaxPagesFileBytes + 1, ' ')));
        QCOMPARE(createProfile(dir.path(), "fresh", "default", {}).code, ProfileError::None);
        QCOMPARE(createProfile(dir.path(), "Bad Name", "default", {}).code, ProfileError::BadName);
        QCOMPARE(createProfile(dir.path(), "default", "work", {}).code, ProfileError::BadName);
        QCOMPARE(createProfile(dir.path(), "work", "default", {}).code, ProfileError::NameTaken);
        QCOMPARE(createProfile(dir.path(), "next", "nope", {}).code, ProfileError::Missing);
        QCOMPARE(createProfile(dir.path(), "next", "big", {}).code, ProfileError::TooLarge);
        QCOMPARE(renameProfile(dir.path(), "default", "other").code, ProfileError::BaseProfile);
        QCOMPARE(renameProfile(dir.path(), "nope", "other").code, ProfileError::Missing);
        QCOMPARE(renameProfile(dir.path(), "work", "fresh").code, ProfileError::NameTaken);
        QCOMPARE(renameProfile(dir.path(), "work", "Bad").code, ProfileError::BadName);
        QCOMPARE(renameProfile(dir.path(), "work", "desk").code, ProfileError::None);
        QCOMPARE(deleteProfile(dir.path(), "default", "s").code, ProfileError::BaseProfile);
        QCOMPARE(deleteProfile(dir.path(), "nope", "s").code, ProfileError::Missing);
        QCOMPARE(deleteProfile(dir.path(), "desk", "s").code, ProfileError::None);
        // Every refusal also has its English text for the logs, and a success has none.
        QVERIFY(!createProfile(dir.path(), "fresh", "default", {}).error.isEmpty());
        QVERIFY(createProfile(dir.path(), "another", "default", {}).error.isEmpty());
    }

    void theNumberOfProfilesIsBounded()
    {
        const QTemporaryDir dir;
        QVERIFY(put(dir.path() + "/pages.json", "base"));
        for (int i = 0; i < MaxProfiles - 1; ++i)
            QVERIFY(createProfile(dir.path(), QStringLiteral("p%1").arg(i, 2, 10, QLatin1Char('0')), "default", {}).ok);
        QCOMPARE(listProfiles(dir.path()).size(), MaxProfiles);
        const ProfileResult over = createProfile(dir.path(), "one-more", "default", {});
        QVERIFY(!over.ok);
        QVERIFY(!QFileInfo::exists(dir.path() + "/profiles/one-more.json"));
    }

    void aProfileIsRenamedWithoutLosingItsContent()
    {
        const QTemporaryDir dir;
        QVERIFY(put(dir.path() + "/profiles/old.json", "content"));
        QVERIFY(put(dir.path() + "/profiles/taken.json", "other"));
        QVERIFY(renameProfile(dir.path(), "old", "new").ok);
        QCOMPARE(get(dir.path() + "/profiles/new.json"), QByteArray("content"));
        QVERIFY(!QFileInfo::exists(dir.path() + "/profiles/old.json"));
        QVERIFY(!renameProfile(dir.path(), "new", "taken").ok); // a taken name is never overwritten
        QCOMPARE(get(dir.path() + "/profiles/taken.json"), QByteArray("other"));
        QVERIFY(!renameProfile(dir.path(), "new", "new").ok);
        for (const QString &bad : {QStringLiteral("default"), QStringLiteral("../x"), QStringLiteral("Up"), QStringLiteral(""), QStringLiteral("a.b")}) {
            QVERIFY2(!renameProfile(dir.path(), "new", bad).ok, qPrintable(bad));
            QVERIFY2(!renameProfile(dir.path(), bad, "fresh").ok, qPrintable(bad));
        }
        QVERIFY(!renameProfile(dir.path(), "missing", "fresh").ok);
        QCOMPARE(get(dir.path() + "/profiles/new.json"), QByteArray("content"));
    }

    void theBaseProfileCannotBeRenamedOrDeleted()
    {
        const QTemporaryDir dir;
        QVERIFY(put(dir.path() + "/pages.json", "base"));
        QVERIFY(!renameProfile(dir.path(), "default", "other").ok);
        QVERIFY(!deleteProfile(dir.path(), "default", "s").ok);
        QCOMPARE(get(dir.path() + "/pages.json"), QByteArray("base"));
    }

    void aDeletedProfileIsMovedAndNeverErased()
    {
        const QTemporaryDir dir;
        QVERIFY(put(dir.path() + "/profiles/gaming.json", "first"));
        QVERIFY(deleteProfile(dir.path(), "gaming", "20261128-180000").ok);
        QVERIFY(!QFileInfo::exists(dir.path() + "/profiles/gaming.json"));
        QCOMPARE(get(dir.path() + "/profiles/.deleted/gaming-20261128-180000.json"), QByteArray("first"));
        QCOMPARE(listProfiles(dir.path()), QStringList{"default"}); // a deleted profile is no longer listed

        // The same name made and deleted again in the same second: the first one is kept, not replaced.
        QVERIFY(put(dir.path() + "/profiles/gaming.json", "second"));
        QVERIFY(deleteProfile(dir.path(), "gaming", "20261128-180000").ok);
        QCOMPARE(get(dir.path() + "/profiles/.deleted/gaming-20261128-180000.json"), QByteArray("first"));
        QCOMPARE(get(dir.path() + "/profiles/.deleted/gaming-20261128-180000-2.json"), QByteArray("second"));
        QVERIFY(!deleteProfile(dir.path(), "gaming", "20261128-180001").ok); // nothing left to delete
        for (const QString &bad : {QStringLiteral("../pages"), QStringLiteral("Up"), QStringLiteral("")})
            QVERIFY2(!deleteProfile(dir.path(), bad, "s").ok, qPrintable(bad));
    }
};

QTEST_GUILESS_MAIN(TestProfileStore)
#include "test_profilestore.moc"
