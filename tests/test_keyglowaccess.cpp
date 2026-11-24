// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The choice "the keys glow under the fingers" (key-glow.json): the reactive glow reads the key presses, so it is off unless the person made the choice in
// Bosun, and the file is read like any file from disk, as untrusted. The shape is the one shared with key-signals.json (AllowFile.h).
#include "KeyGlowAccess.h"
#include "KeySignalsAccess.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

class TestKeyGlowAccess : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutAFileTheGlowIsOff()
    {
        const QTemporaryDir dir;
        QVERIFY(!keyGlowAllowed(dir.filePath("key-glow.json")));
        QVERIFY(!readKeyGlowAccess(dir.filePath("key-glow.json")).has_value());
    }

    void onlyAFileThatSaysSoSwitchesItOn()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        QVERIFY(writeKeyGlowAccess(path, true));
        QVERIFY(keyGlowAllowed(path));
        QVERIFY(writeKeyGlowAccess(path, false));
        QVERIFY(!keyGlowAllowed(path));
    }

    void aDamagedOrForeignFileMeansOff_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("cut") << QByteArray(R"({"version":1,"allow":tru)");
        QTest::newRow("a string") << QByteArray(R"({"version":1,"allow":"yes"})");
        QTest::newRow("another version") << QByteArray(R"({"version":2,"allow":true})");
        QTest::newRow("an extra key") << QByteArray(R"({"version":1,"allow":true,"colour":"cyan"})");
        QTest::newRow("too large") << (QByteArray(R"({"version":1,"allow":true,"pad":")") + QByteArray(400, 'x') + QByteArray(R"("})"));
        QTest::newRow("empty") << QByteArray();
    }
    void aDamagedOrForeignFileMeansOff()
    {
        QFETCH(const QByteArray, bytes);
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-glow.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        file.close();
        QVERIFY(!keyGlowAllowed(path));
    }

    void theGlowAndTheSignalsHaveTheirOwnFilesAndOneDoesNotSwitchTheOtherOn()
    {
        QVERIFY(keyGlowAccessPath().endsWith("/galleoncore/key-glow.json"));
        QVERIFY(keySignalsAccessPath().endsWith("/galleoncore/key-signals.json"));
        QVERIFY(keyGlowAccessPath() != keySignalsAccessPath());
        const QTemporaryDir dir;
        QVERIFY(writeKeySignalsAccess(dir.filePath("key-signals.json"), true));
        QVERIFY(!keyGlowAllowed(dir.filePath("key-glow.json"))); // another file: nothing was said about the glow
    }
};

QTEST_APPLESS_MAIN(TestKeyGlowAccess)
#include "test_keyglowaccess.moc"
