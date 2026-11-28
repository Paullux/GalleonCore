// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Profiles.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <limits>

using namespace galleond;

namespace {

void touch(const QString &path, const QByteArray &content = "{}")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

} // namespace

class TestProfiles : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aNameIsDefaultOrAShortPlainWord_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("valid");
        QTest::newRow("default") << "default" << true;
        QTest::newRow("a word") << "gaming" << true;
        QTest::newRow("with a dash and digits") << "work-2" << true;
        QTest::newRow("a digit first") << "3d" << true;
        QTest::newRow("32 characters") << QString(32, 'a') << true;
        QTest::newRow("33 characters") << QString(33, 'a') << false;
        QTest::newRow("empty") << "" << false;
        QTest::newRow("a dash first") << "-x" << false;
        QTest::newRow("a capital") << "Gaming" << false;
        QTest::newRow("a space") << "my profile" << false;
        QTest::newRow("a dot") << "a.b" << false;
        QTest::newRow("the trash folder") << ".deleted" << false;
        QTest::newRow("a path up") << "../pages" << false;
        QTest::newRow("a path down") << "a/b" << false;
        QTest::newRow("a backslash") << "a\\b" << false;
        QTest::newRow("a final newline") << "gaming\n" << false;
        QTest::newRow("an accent") << QString::fromUtf8("jeux\xc3\xa9") << false;
        QTest::newRow("an underscore") << "a_b" << false;
        QTest::newRow("a nul") << QString(QStringLiteral("a")) + QChar(0) + QStringLiteral("b") << false;
    }

    void aNameIsDefaultOrAShortPlainWord()
    {
        QFETCH(const QString, name);
        QFETCH(const bool, valid);
        QCOMPARE(isValidProfileName(name), valid);
        QCOMPARE(profilePath(QStringLiteral("/cfg"), name).isEmpty(), !valid); // no path is built from a name that is not valid
    }

    void theDefaultProfileIsThePagesFileAndTheOthersAreInTheProfilesFolder()
    {
        QCOMPARE(profilePath(QStringLiteral("/cfg"), QStringLiteral("default")), QStringLiteral("/cfg/pages.json"));
        QCOMPARE(profilePath(QStringLiteral("/cfg"), QStringLiteral("gaming")), QStringLiteral("/cfg/profiles/gaming.json"));
        QVERIFY(profilePath({}, QStringLiteral("gaming")).isEmpty());
    }

    void theProfilesAreDefaultFirstThenTheValidNamesSortedOnce()
    {
        const QStringList names = profileNamesFrom({"work", "gaming", "Bad Name", "default", "work", "a.b", "-x", "music"});
        QCOMPARE(names, (QStringList{"default", "gaming", "music", "work"}));
        QCOMPARE(profileNamesFrom({}), QStringList{"default"});
    }

    void theProfilesAreBounded()
    {
        QStringList many;
        for (int i = 0; i < 40; ++i)
            many.append(QStringLiteral("p%1").arg(i, 2, 10, QLatin1Char('0')));
        const QStringList names = profileNamesFrom(many);
        QCOMPARE(names.size(), MaxProfiles);
        QCOMPARE(names.first(), QStringLiteral("default"));
    }

    void theFolderIsListedByItsJsonFilesOnly()
    {
        const QTemporaryDir dir;
        const QString folder = dir.path();
        touch(folder + "/pages.json");
        touch(folder + "/profiles/gaming.json");
        touch(folder + "/profiles/work.json");
        touch(folder + "/profiles/notes.txt");         // not a profile
        touch(folder + "/profiles/Bad Name.json");     // not a valid name
        QDir().mkpath(folder + "/profiles/.deleted");  // the trash
        touch(folder + "/profiles/.deleted/old.json"); // a deleted profile is not one
        QCOMPARE(listProfiles(folder), (QStringList{"default", "gaming", "work"}));
        QCOMPARE(listProfiles(folder + "/none"), QStringList{"default"});
    }

    void steppingGoesRoundBothWays()
    {
        const QStringList names{"default", "gaming", "work"};
        QCOMPARE(steppedProfile(names, "default", 1), QStringLiteral("gaming"));
        QCOMPARE(steppedProfile(names, "work", 1), QStringLiteral("default"));
        QCOMPARE(steppedProfile(names, "default", -1), QStringLiteral("work"));
        QCOMPARE(steppedProfile(names, "gaming", 0), QStringLiteral("gaming"));
        QCOMPARE(steppedProfile(names, "gaming", 7), QStringLiteral("work"));   // 7 = 1 mod 3
        QCOMPARE(steppedProfile(names, "gaming", -100), QStringLiteral("default")); // -100 = -1 mod 3: one place back from "gaming"
        QCOMPARE(steppedProfile(names, "gaming", std::numeric_limits<int>::min()), QStringLiteral("work")); // the smallest step does not overflow (-2 mod 3)
        QCOMPARE(steppedProfile(names, "gaming", std::numeric_limits<int>::max()), QStringLiteral("work")); // and nor does the largest (+1 mod 3)
        QCOMPARE(steppedProfile(names, "vanished", 1), QStringLiteral("gaming")); // an unknown one counts as the first
        QCOMPARE(steppedProfile({}, "x", 3), QStringLiteral("default"));
        QCOMPARE(steppedProfile({"default"}, "default", 5), QStringLiteral("default"));
    }

    void theActiveProfileFileIsStrict()
    {
        QCOMPARE(parseActiveProfile(encodeActiveProfile("gaming")), std::optional<QString>("gaming"));
        QCOMPARE(parseActiveProfile(encodeActiveProfile("default")), std::optional<QString>("default"));
        QString why;
        for (const QByteArray &bad : {QByteArray(), QByteArray("not json"), QByteArray("[]"), QByteArray(R"({"version":1})"),
                                      QByteArray(R"({"version":2,"profile":"gaming"})"), QByteArray(R"({"version":1,"profile":"Gaming"})"),
                                      QByteArray(R"({"version":1,"profile":"../x"})"), QByteArray(R"({"version":1,"profile":7})"),
                                      QByteArray(R"({"version":1,"profile":"gaming","extra":true})"),
                                      QByteArray(R"({"version":1.5,"profile":"gaming"})"), QByteArray(2000, ' ')}) {
            why.clear();
            QVERIFY2(!parseActiveProfile(bad, &why).has_value(), bad.left(60).constData());
            QVERIFY(!why.isEmpty());
        }
    }

    void theActiveProfileIsRememberedAndFoundAgainAtStart()
    {
        const QTemporaryDir dir;
        const QString folder = dir.path();
        QCOMPARE(activeProfileAtStart(folder), QStringLiteral("default")); // nothing remembered yet
        touch(folder + "/profiles/gaming.json");
        QVERIFY(writeActiveProfile(folder, "gaming"));
        QCOMPARE(activeProfileAtStart(folder), QStringLiteral("gaming"));
        QVERIFY(!writeActiveProfile(folder, "../evil")); // a name that is not valid is never written
        QCOMPARE(activeProfileAtStart(folder), QStringLiteral("gaming"));
        QVERIFY(QFile::remove(folder + "/profiles/gaming.json")); // its file is gone: back to the base
        QCOMPARE(activeProfileAtStart(folder), QStringLiteral("default"));
        QVERIFY(writeActiveProfile(folder, "default"));
        QCOMPARE(activeProfileAtStart(folder), QStringLiteral("default")); // default needs no file
        touch(activeProfilePath(folder), "garbage"); // a damaged file is not used
        QCOMPARE(activeProfileAtStart(folder), QStringLiteral("default"));
    }
};

QTEST_GUILESS_MAIN(TestProfiles)
#include "test_profiles.moc"
