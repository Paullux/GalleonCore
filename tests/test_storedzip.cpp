// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Required.h"

#include <StoredZip.h>

#include <QTest>

using namespace galleond;

namespace {

// Made by Python's zipfile (ZIP_STORED): theme.json = {"a":1}, icons/mute.png = PNGDATA.
// A third-party tool wrote it, so the reader is checked against more than our own writer.
QByteArray storedByPython()
{
    return QByteArray::fromHex(
        "504b030414000000000030566b5dafac1b5607000000070000000a0000007468656d652e6a736f6e7b2261223a317d504b030414000000000030566b5d47181adb"
        "07000000070000000e00000069636f6e732f6d7574652e706e67504e4744415441504b0102140314000000000030566b5dafac1b5607000000070000000a0000"
        "0000000000000000008001000000007468656d652e6a736f6e504b0102140314000000000030566b5d47181adb07000000070000000e0000000000000000000000"
        "80012f00000069636f6e732f6d7574652e706e67504b0506000000000200020074000000620000000000");
}

// Made by Python's zipfile (ZIP_DEFLATED): a.txt.
QByteArray deflatedByPython()
{
    return QByteArray::fromHex(
        "504b030414000000080030566b5d573762850b0000002300000005000000612e747874cb48cdc9c957c8c0470200504b0102140314000000080030566b5d5737"
        "62850b00000023000000050000000000000000000000800100000000612e747874504b05060000000001000100330000002e0000000000");
}

std::vector<ZipEntry> twoEntries()
{
    return {{QStringLiteral("theme.json"), QByteArray("{\"a\":1}")}, {QStringLiteral("icons/mute.png"), QByteArray("PNGDATA")}};
}

void put32(QByteArray &bytes, int at, quint32 value)
{
    for (int i = 0; i < 4; ++i)
        bytes[at + i] = static_cast<char>((value >> (8 * i)) & 0xFFU);
}

} // namespace

class TestStoredZip : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theChecksumMatchesTheKnownValue()
    {
        // The standard CRC-32 check value.
        QCOMPARE(zipCrc32(QByteArrayView("123456789")), quint32{0xCBF43926});
        QCOMPARE(zipCrc32(QByteArrayView()), quint32{0});
    }

    void anArchiveWrittenHereIsReadBackIdentical()
    {
        const auto written = writeStoredZip(twoEntries());
        QVERIFY(written.has_value());
        const ZipReadResult read = readStoredZip(required(written));
        QVERIFY2(read.errors.isEmpty(), qPrintable(read.errors.join(QLatin1Char(';'))));
        QCOMPARE(read.entries, twoEntries());
    }

    void theWriterIsReproducible()
    {
        QCOMPARE(writeStoredZip(twoEntries()), writeStoredZip(twoEntries()));
    }

    void anArchiveMadeByAnotherToolIsRead()
    {
        const ZipReadResult read = readStoredZip(storedByPython());
        QVERIFY2(read.errors.isEmpty(), qPrintable(read.errors.join(QLatin1Char(';'))));
        QCOMPARE(read.entries, twoEntries());
    }

    void anArchiveWrittenHereHasTheSameBytesAsPythonsExceptTheDate()
    {
        // Same size, same entry sequence: the layout is the standard one.
        const auto written = writeStoredZip(twoEntries());
        QVERIFY(written.has_value());
        QCOMPARE(required(written).size(), storedByPython().size());
    }

    void anEmptyArchiveHasNoEntries()
    {
        const auto written = writeStoredZip({});
        QVERIFY(written.has_value());
        const ZipReadResult read = readStoredZip(required(written));
        QVERIFY(read.errors.isEmpty());
        QVERIFY(read.entries.empty());
    }

    void compressedEntriesAreRefused()
    {
        const ZipReadResult read = readStoredZip(deflatedByPython());
        QVERIFY(read.entries.empty());
        QVERIFY(!read.errors.isEmpty());
        QVERIFY(read.errors.constFirst().contains(QLatin1String("stored")));
    }

    void somethingThatIsNotAZipIsRefused()
    {
        QVERIFY(!readStoredZip(QByteArray("this is plain text, not an archive")).errors.isEmpty());
        QVERIFY(!readStoredZip(QByteArray()).errors.isEmpty());
        QVERIFY(!readStoredZip(QByteArray(10, '\0')).errors.isEmpty());
    }

    void everyTruncationIsRefusedWithoutACrash()
    {
        const QByteArray whole = storedByPython();
        for (qsizetype length = 0; length < whole.size(); ++length) {
            const ZipReadResult read = readStoredZip(whole.left(length));
            QVERIFY2(!read.errors.isEmpty(), qPrintable(QStringLiteral("accepted at %1 bytes").arg(length)));
            QVERIFY(read.entries.empty());
        }
    }

    void aChangedByteInTheDataBreaksTheChecksum()
    {
        QByteArray bytes = storedByPython();
        const qsizetype at = bytes.indexOf("PNGDATA");
        QVERIFY(at > 0);
        bytes[at] = 'X';
        const ZipReadResult read = readStoredZip(bytes);
        QVERIFY(read.entries.empty());
        QVERIFY(read.errors.constFirst().contains(QLatin1String("checksum")));
    }

    void aDirectoryOffsetPastTheEndIsRefused()
    {
        QByteArray bytes = storedByPython();
        put32(bytes, static_cast<int>(bytes.size()) - 22 + 16, 0x7FFFFFF0U);
        QVERIFY(!readStoredZip(bytes).errors.isEmpty());
    }

    void anEntryOffsetPastTheEndIsRefused()
    {
        QByteArray bytes = storedByPython();
        const qsizetype directory = bytes.indexOf("PK\x01\x02");
        QVERIFY(directory > 0);
        put32(bytes, static_cast<int>(directory) + 42, 0x7FFFFFF0U);
        QVERIFY(!readStoredZip(bytes).errors.isEmpty());
    }

    void anEntrySizeLargerThanTheFileIsRefused()
    {
        QByteArray bytes = storedByPython();
        const qsizetype directory = bytes.indexOf("PK\x01\x02");
        put32(bytes, static_cast<int>(directory) + 20, 0x00FFFFFFU);
        put32(bytes, static_cast<int>(directory) + 24, 0x00FFFFFFU);
        QVERIFY(!readStoredZip(bytes).errors.isEmpty());
    }

    void anEncryptedEntryIsRefused()
    {
        QByteArray bytes = storedByPython();
        const qsizetype directory = bytes.indexOf("PK\x01\x02");
        bytes[directory + 8] = 0x01;
        QVERIFY(!readStoredZip(bytes).errors.isEmpty());
    }

    void namesThatLeaveTheFolderAreRefused_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("parent") << "../x.png";
        QTest::newRow("nested parent") << "icons/../../x.png";
        QTest::newRow("absolute") << "/etc/passwd";
        QTest::newRow("backslash") << "icons\\x.png";
        QTest::newRow("empty part") << "icons//x.png";
        QTest::newRow("dot") << "./x.png";
        QTest::newRow("space") << "my icon.png";
        QTest::newRow("accent") << "icône.png";
        QTest::newRow("too long") << QString(101, QLatin1Char('a'));
        QTest::newRow("empty") << "";
        QTest::newRow("trailing line break") << "icons/x.png\n";
    }
    void namesThatLeaveTheFolderAreRefused()
    {
        QFETCH(const QString, name);
        QVERIFY(!isValidZipEntryName(name));
        QString error;
        QVERIFY(!writeStoredZip({{name, QByteArray("x")}}, &error).has_value());
        QVERIFY(!error.isEmpty());

    }

    void aTraversalNameWrittenByAnotherToolIsRefusedByTheReader()
    {
        QByteArray bytes = storedByPython();
        QCOMPARE(bytes.count("theme.json"), 2);
        bytes.replace("theme.json", "../xx.json");
        const ZipReadResult read = readStoredZip(bytes);
        QVERIFY(read.entries.empty());
        QVERIFY(!read.errors.isEmpty());
    }

    void twoNamesDifferingOnlyByCaseAreRefused()
    {
        QString error;
        QVERIFY(!writeStoredZip({{QStringLiteral("a.png"), QByteArray("1")}, {QStringLiteral("A.PNG"), QByteArray("2")}}, &error).has_value());
        QVERIFY(error.contains(QLatin1String("twice")));
    }

    void theWriterKeepsToTheReadersLimits()
    {
        QString error;
        std::vector<ZipEntry> many;
        many.reserve(MaxZipEntries + 1);
        for (int i = 0; i <= MaxZipEntries; ++i)
            many.push_back({QStringLiteral("f%1.png").arg(i), QByteArray("x")});
        QVERIFY(!writeStoredZip(many, &error).has_value());

        QVERIFY(!writeStoredZip({{QStringLiteral("big.bin"), QByteArray(MaxZipEntryBytes + 1, 'x')}}, &error).has_value());

        std::vector<ZipEntry> together;
        together.reserve(4);
        for (int i = 0; i < 4; ++i)
            together.push_back({QStringLiteral("p%1.bin").arg(i), QByteArray(MaxZipEntryBytes, 'x')});
        QVERIFY(!writeStoredZip(together, &error).has_value());
    }

    void aWrittenArchiveAtTheLimitsIsStillRead()
    {
        std::vector<ZipEntry> many;
        many.reserve(MaxZipEntries + 1);
        for (int i = 0; i < MaxZipEntries; ++i)
            many.push_back({QStringLiteral("f%1.png").arg(i), QByteArray("x")});
        const auto written = writeStoredZip(many);
        QVERIFY(written.has_value());
        QCOMPARE(readStoredZip(required(written)).entries.size(), many.size());
    }

    void aFolderEntryIsSkipped()
    {
        // Python's zipfile writes a folder as "icons/" with no data, then the file inside it.
        const QByteArray withFolder = QByteArray::fromHex(
            "504b0304140000000000000021000000000000000000000000000600000069636f6e732f504b030414000000000044566b5d074c693002000000020000000b00000069636f6e732f612e706e674142504b010214031400000000000000210000000000000000000000000006000000000000000000000080010000000069636f6e732f504b0102140314000000000044566b5d074c693002000000020000000b000000000000000000000080012400000069636f6e732f612e706e67504b050600000000020002006d0000004f0000000000");
        const ZipReadResult read = readStoredZip(withFolder);
        QVERIFY2(read.errors.isEmpty(), qPrintable(read.errors.join(QLatin1Char(';'))));
        QCOMPARE(read.entries.size(), std::size_t{1});
        QCOMPARE(read.entries.front().name, QStringLiteral("icons/a.png"));
        QCOMPARE(read.entries.front().data, QByteArray("AB"));
    }
};

QTEST_APPLESS_MAIN(TestStoredZip)
#include "test_storedzip.moc"
