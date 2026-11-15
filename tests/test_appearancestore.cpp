// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The appearance as Bosun holds it: every change is written to lighting-appearance.json (what galleond reads), and the file is read again
// when the window starts. No keyboard and no daemon are involved.

#include "AppearanceStore.h"

#include <AppearanceFile.h>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace bosun;
using galleon::keyboard::BaseType;

class TestAppearanceStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheKeysAreDark()
    {
        const QTemporaryDir dir;
        const AppearanceStore store(dir.filePath("lighting-appearance.json"));
        QCOMPARE(store.baseType(), QStringLiteral("uniform"));
        QCOMPARE(store.colour(), QColor(0, 0, 0));
        QVERIFY(store.retouches().isEmpty());
        QCOMPARE(store.level(), 1000);
    }

    void aColourIsWrittenToTheFileAtOnce()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        QVERIFY(store.setUniform(10, 20, 30).isEmpty());
        const auto written = galleond::readAppearance(path);
        QVERIFY(written.has_value());
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).base, BaseType::Uniform);
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).colour.green, 20);
    }

    void theRainbowIsWrittenWithItsSpeedAndKeepsTheRetouches()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        QVERIFY(store.retouch(58, 255, 255, 255).isEmpty());
        QVERIFY(store.setRainbow(100).isEmpty());
        const auto written = galleond::readAppearance(path);
        QVERIFY(written.has_value());
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).base, BaseType::Rainbow);
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).rainbowSpeed, 100);
        QCOMPARE(written.value_or(galleon::keyboard::Appearance{}).retouches.size(), std::size_t{1});
    }

    void aSecondRetouchOfTheSameLedReplacesTheFirst()
    {
        const QTemporaryDir dir;
        AppearanceStore store(dir.filePath("lighting-appearance.json"));
        QVERIFY(store.retouch(4, 255, 0, 0).isEmpty());
        QVERIFY(store.retouch(4, 0, 0, 255).isEmpty());
        QCOMPARE(store.retouches().size(), 1);
        QCOMPARE(store.retouches().constFirst().toMap().value("colour").toString(), QStringLiteral("#0000ff"));
    }

    void clearingTheRetouchesKeepsTheBase()
    {
        const QTemporaryDir dir;
        AppearanceStore store(dir.filePath("lighting-appearance.json"));
        QVERIFY(store.setRainbow(0).isEmpty());
        QVERIFY(store.retouch(4, 255, 0, 0).isEmpty());
        QVERIFY(store.clearRetouches().isEmpty());
        QVERIFY(store.retouches().isEmpty());
        QCOMPARE(store.baseType(), QStringLiteral("rainbow"));
    }

    void whatWasWrittenIsReadAgainWhenTheWindowStarts()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        {
            AppearanceStore store(path);
            QVERIFY(store.setRainbow(250).isEmpty());
            QVERIFY(store.retouch(58, 255, 255, 255).isEmpty());
            QVERIFY(store.setLevel(500).isEmpty());
        }
        const AppearanceStore again(path);
        QCOMPARE(again.baseType(), QStringLiteral("rainbow"));
        QCOMPARE(again.rainbowSpeed(), 250);
        QCOMPARE(again.level(), 500);
        QCOMPARE(again.retouches().size(), 1);
    }

    void aDamagedFileGivesDarkKeysInsteadOfAnError()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":1,"base":)");
        file.close();
        const AppearanceStore store(path);
        QCOMPARE(store.baseType(), QStringLiteral("uniform"));
        QCOMPARE(store.colour(), QColor(0, 0, 0));
    }

    void aValueOutOfRangeIsRefusedAndNothingChanges()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("lighting-appearance.json");
        AppearanceStore store(path);
        const QSignalSpy changed(&store, &AppearanceStore::changed);
        QVERIFY(!store.setUniform(256, 0, 0).isEmpty());
        QVERIFY(!store.retouch(146, 0, 0, 0).isEmpty());
        QVERIFY(!store.retouch(-1, 0, 0, 0).isEmpty());
        QVERIFY(!store.setLevel(1001).isEmpty());
        QVERIFY(!store.setRainbow(1001).isEmpty());
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QFile::exists(path));
    }

    void aFileThatCannotBeWrittenStillChangesTheDrawing()
    {
        AppearanceStore store("/proc/nothing/lighting-appearance.json");
        QVERIFY(!store.setUniform(1, 2, 3).isEmpty()); // the reason is given
        QCOMPARE(store.colour(), QColor(1, 2, 3));     // and the drawing follows
    }

    void withoutAPathNothingIsWritten()
    {
        const QTemporaryDir dir;
        AppearanceStore store{QString()};
        QVERIFY(store.setUniform(1, 2, 3).isEmpty());
        QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
    }

    void everyChangeSaysSo()
    {
        const QTemporaryDir dir;
        AppearanceStore store(dir.filePath("lighting-appearance.json"));
        const QSignalSpy changed(&store, &AppearanceStore::changed);
        QVERIFY(store.setUniform(1, 2, 3).isEmpty());
        QVERIFY(store.retouch(5, 1, 1, 1).isEmpty());
        QVERIFY(store.reset().isEmpty());
        QCOMPARE(changed.count(), 3);
        QVERIFY(store.retouches().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAppearanceStore)
#include "test_appearancestore.moc"
