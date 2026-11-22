// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The choice "other programs may light single keys": off unless the person made it, and read like any file from disk, as untrusted.
#include "KeySignalsAccess.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace galleond;

class TestKeySignalsAccess : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theChoiceWrittenIsTheChoiceRead()
    {
        QCOMPARE(parseKeySignalsAccess(encodeKeySignalsAccess(true)), std::optional<bool>(true));
        QCOMPARE(parseKeySignalsAccess(encodeKeySignalsAccess(false)), std::optional<bool>(false));
    }

    void aFileThatIsNotExactlyTheExpectedShapeSaysNothing_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("not json") << QByteArray("allow");
        QTest::newRow("an array") << QByteArray("[true]");
        QTest::newRow("a string instead of a boolean") << QByteArray(R"({"version":1,"allow":"yes"})");
        QTest::newRow("a number instead of a boolean") << QByteArray(R"({"version":1,"allow":1})");
        QTest::newRow("another version") << QByteArray(R"({"version":2,"allow":true})");
        QTest::newRow("no version") << QByteArray(R"({"allow":true})");
        QTest::newRow("an extra key") << QByteArray(R"({"version":1,"allow":true,"also":1})");
        QTest::newRow("another key name") << QByteArray(R"({"version":1,"restore":true})");
        QTest::newRow("too large") << (QByteArray(R"({"version":1,"allow":true,"pad":")") + QByteArray(400, 'x') + QByteArray(R"("})"));
    }
    void aFileThatIsNotExactlyTheExpectedShapeSaysNothing()
    {
        QFETCH(const QByteArray, bytes);
        QString why;
        QVERIFY(!parseKeySignalsAccess(bytes, &why).has_value());
        QVERIFY(!why.isEmpty());
    }

    void withoutAFileOrWithADamagedOneTheAnswerIsNo()
    {
        const QTemporaryDir dir;
        QVERIFY(!keySignalsAllowed(dir.filePath("key-signals.json")));
        QFile file(dir.filePath("key-signals.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":1,"allow":tru)");
        file.close();
        QVERIFY(!keySignalsAllowed(dir.filePath("key-signals.json")));
    }

    void onlyAFileThatSaysSoMeansYes()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath("key-signals.json");
        QVERIFY(writeKeySignalsAccess(path, true));
        QVERIFY(keySignalsAllowed(path));
        QVERIFY(writeKeySignalsAccess(path, false));
        QVERIFY(!keySignalsAllowed(path));
    }

    void theFileIsInTheConfigFolderOfGalleoncore()
    {
        QVERIFY(keySignalsAccessPath().endsWith("/galleoncore/key-signals.json"));
    }
};

QTEST_APPLESS_MAIN(TestKeySignalsAccess)
#include "test_keysignalsaccess.moc"
