// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StoredZip.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cstdint>

// Layout of the records: PKWARE APPNOTE 6.3.x, sections 4.3.7 (local header),
// 4.3.12 (central directory header) and 4.3.16 (end of central directory).

namespace galleond {

namespace {

constexpr quint32 LocalSignature = 0x04034b50;
constexpr quint32 CentralSignature = 0x02014b50;
constexpr quint32 EndSignature = 0x06054b50;
constexpr qsizetype LocalHeaderSize = 30;
constexpr qsizetype CentralHeaderSize = 46;
constexpr qsizetype EndRecordSize = 22;
constexpr quint16 FlagEncrypted = 0x0001;
constexpr quint16 FlagDataDescriptor = 0x0008;
constexpr quint16 FlagUtf8 = 0x0800;
constexpr quint16 MethodStored = 0;
constexpr quint16 VersionNeeded = 20;
// 1980-01-01 00:00: a fixed time keeps the archive byte for byte reproducible.
constexpr quint16 FixedTime = 0;
constexpr quint16 FixedDate = 0x0021;

// A bounds-checked view of the archive: every read says whether it fit.
class Cursor
{
public:
    explicit Cursor(QByteArrayView bytes) : m_bytes(bytes) {}

    [[nodiscard]] bool has(qsizetype offset, qsizetype length) const
    {
        return offset >= 0 && length >= 0 && offset <= m_bytes.size() && length <= m_bytes.size() - offset;
    }
    [[nodiscard]] std::optional<quint16> u16(qsizetype offset) const
    {
        if (!has(offset, 2))
            return std::nullopt;
        return static_cast<quint16>(byteAt(offset) | (byteAt(offset + 1) << 8));
    }
    [[nodiscard]] std::optional<quint32> u32(qsizetype offset) const
    {
        if (!has(offset, 4))
            return std::nullopt;
        return byteAt(offset) | (byteAt(offset + 1) << 8) | (byteAt(offset + 2) << 16) | (byteAt(offset + 3) << 24);
    }
    [[nodiscard]] QByteArrayView slice(qsizetype offset, qsizetype length) const { return m_bytes.sliced(offset, length); }
    [[nodiscard]] qsizetype size() const { return m_bytes.size(); }

private:
    [[nodiscard]] quint32 byteAt(qsizetype offset) const { return static_cast<quint8>(m_bytes[offset]); }
    QByteArrayView m_bytes;
};

const QRegularExpression &namePartPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9._-]+$"));
    return pattern;
}

struct CentralRecord
{
    quint16 flags = 0;
    quint32 crc = 0;
    quint32 size = 0;
    quint32 localOffset = 0;
    QString name;
    QByteArray rawName;
    bool directory = false;
};

void appendU16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>(value & 0xFFU));
    out.append(static_cast<char>((value >> 8) & 0xFFU));
}

void appendU32(QByteArray &out, quint32 value)
{
    appendU16(out, static_cast<quint16>(value & 0xFFFFU));
    appendU16(out, static_cast<quint16>((value >> 16) & 0xFFFFU));
}

std::optional<qsizetype> findEndRecord(const Cursor &cursor)
{
    if (cursor.size() < EndRecordSize)
        return std::nullopt;
    // The record is last, followed only by its comment (up to 65535 bytes).
    const qsizetype lowest = std::max<qsizetype>(0, cursor.size() - EndRecordSize - 65535);
    for (qsizetype at = cursor.size() - EndRecordSize; at >= lowest; --at) {
        if (cursor.u32(at) != EndSignature)
            continue;
        const auto commentLength = cursor.u16(at + 20);
        if (commentLength && at + EndRecordSize + *commentLength == cursor.size())
            return at;
    }
    return std::nullopt;
}

// Reads one central directory record at `at`; advances `at` past it.
bool readCentralRecord(const Cursor &cursor, qsizetype &at, CentralRecord &record, QString &problem)
{
    if (cursor.u32(at) != CentralSignature) {
        problem = QStringLiteral("a directory record is damaged");
        return false;
    }
    const auto flags = cursor.u16(at + 8);
    const auto method = cursor.u16(at + 10);
    const auto crc = cursor.u32(at + 16);
    const auto compressed = cursor.u32(at + 20);
    const auto size = cursor.u32(at + 24);
    const auto nameLength = cursor.u16(at + 28);
    const auto extraLength = cursor.u16(at + 30);
    const auto commentLength = cursor.u16(at + 32);
    const auto offset = cursor.u32(at + 42);
    if (!flags || !method || !crc || !compressed || !size || !nameLength || !extraLength || !commentLength || !offset) {
        problem = QStringLiteral("a directory record is cut short");
        return false;
    }
    const qsizetype total = CentralHeaderSize + *nameLength + *extraLength + *commentLength;
    if (!cursor.has(at, total) || *nameLength == 0) {
        problem = QStringLiteral("a directory record is cut short");
        return false;
    }
    if ((*flags & (FlagEncrypted | FlagDataDescriptor)) != 0) {
        problem = QStringLiteral("encrypted entries and data descriptors are not accepted");
        return false;
    }
    if (*method != MethodStored || *compressed != *size) {
        problem = QStringLiteral("entries must be stored, not compressed: build the file with GalleonCore or with 'zip -0'");
        return false;
    }
    if (*size == 0xFFFFFFFFU || *offset == 0xFFFFFFFFU) {
        problem = QStringLiteral("zip64 archives are not accepted");
        return false;
    }
    record.flags = *flags;
    record.crc = *crc;
    record.size = *size;
    record.localOffset = *offset;
    record.rawName = cursor.slice(at + CentralHeaderSize, *nameLength).toByteArray();
    record.directory = record.rawName.endsWith('/');
    // Names must be plain ASCII, which also makes the UTF-8 flag irrelevant.
    record.name = QString::fromLatin1(record.rawName);
    for (const char c : record.rawName) {
        if (static_cast<quint8>(c) >= 0x80) {
            problem = QStringLiteral("entry names must be plain ASCII");
            return false;
        }
    }
    at += total;
    return true;
}

// Checks the local copy of the header against the central one; returns the data.
bool readEntryData(const Cursor &cursor, const CentralRecord &record, QByteArray &data, QString &problem)
{
    const qsizetype at = record.localOffset;
    if (cursor.u32(at) != LocalSignature) {
        problem = QStringLiteral("%1: the entry header is damaged").arg(record.name);
        return false;
    }
    const auto flags = cursor.u16(at + 6);
    const auto method = cursor.u16(at + 8);
    const auto nameLength = cursor.u16(at + 26);
    const auto extraLength = cursor.u16(at + 28);
    if (!flags || !method || !nameLength || !extraLength || *method != MethodStored || (*flags & (FlagEncrypted | FlagDataDescriptor)) != 0
        || *nameLength != record.rawName.size() || !cursor.has(at, LocalHeaderSize + *nameLength + *extraLength)
        || cursor.slice(at + LocalHeaderSize, *nameLength).toByteArray() != record.rawName) {
        problem = QStringLiteral("%1: the two headers of the entry disagree").arg(record.name);
        return false;
    }
    const qsizetype start = at + LocalHeaderSize + *nameLength + *extraLength;
    if (!cursor.has(start, static_cast<qsizetype>(record.size))) {
        problem = QStringLiteral("%1: the data runs past the end of the file").arg(record.name);
        return false;
    }
    data = cursor.slice(start, static_cast<qsizetype>(record.size)).toByteArray();
    if (zipCrc32(data) != record.crc) {
        problem = QStringLiteral("%1: the checksum does not match").arg(record.name);
        return false;
    }
    return true;
}

} // namespace

quint32 zipCrc32(QByteArrayView data)
{
    static const std::array<quint32, 256> table = [] {
        std::array<quint32, 256> built{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 value = i;
            for (int bit = 0; bit < 8; ++bit)
                value = (value & 1U) != 0 ? (0xEDB88320U ^ (value >> 1)) : (value >> 1);
            built.at(i) = value;
        }
        return built;
    }();
    quint32 crc = 0xFFFFFFFFU;
    for (const char c : data)
        crc = table.at((crc ^ static_cast<quint8>(c)) & 0xFFU) ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFU;
}

bool isValidZipEntryName(const QString &name)
{
    if (name.isEmpty() || name.size() > MaxZipNameLength)
        return false;
    const QStringList parts = name.split(QLatin1Char('/'));
    return std::ranges::all_of(parts, [](const QString &part) {
        return part != QLatin1String(".") && part != QLatin1String("..") && namePartPattern().match(part).hasMatch();
    });
}

ZipReadResult readStoredZip(const QByteArray &archive)
{
    ZipReadResult result;
    const auto fail = [&result](const QString &problem) {
        result.entries.clear();
        result.errors = {problem};
        return result;
    };
    if (archive.size() > MaxZipArchiveBytes)
        return fail(QStringLiteral("the file is larger than %1 MiB").arg(MaxZipArchiveBytes / ZipMiB));
    const Cursor cursor(archive);
    const auto end = findEndRecord(cursor);
    if (!end)
        return fail(QStringLiteral("this is not a zip file"));
    const auto disk = cursor.u16(*end + 4);
    const auto directoryDisk = cursor.u16(*end + 6);
    const auto onDisk = cursor.u16(*end + 8);
    const auto count = cursor.u16(*end + 10);
    const auto directorySize = cursor.u32(*end + 12);
    const auto directoryOffset = cursor.u32(*end + 16);
    if (!disk || !directoryDisk || !onDisk || !count || !directorySize || !directoryOffset)
        return fail(QStringLiteral("this is not a zip file"));
    if (*disk != 0 || *directoryDisk != 0 || *onDisk != *count)
        return fail(QStringLiteral("archives in several parts are not accepted"));
    if (*count == 0xFFFF || *directorySize == 0xFFFFFFFFU || *directoryOffset == 0xFFFFFFFFU)
        return fail(QStringLiteral("zip64 archives are not accepted"));
    if (*count > MaxZipEntries)
        return fail(QStringLiteral("more than %1 entries").arg(MaxZipEntries));
    if (!cursor.has(static_cast<qsizetype>(*directoryOffset), static_cast<qsizetype>(*directorySize))
        || static_cast<qsizetype>(*directoryOffset) + static_cast<qsizetype>(*directorySize) > *end)
        return fail(QStringLiteral("the directory of the archive is outside the file"));

    auto at = static_cast<qsizetype>(*directoryOffset);
    qsizetype total = 0;
    QHash<QString, int> seen;
    for (int i = 0; i < *count; ++i) {
        CentralRecord record;
        QString problem;
        if (!readCentralRecord(cursor, at, record, problem))
            return fail(problem);
        if (record.directory) {
            if (record.size != 0)
                return fail(QStringLiteral("%1: a folder with data").arg(record.name));
            continue;
        }
        if (!isValidZipEntryName(record.name))
            return fail(QStringLiteral("an entry name is not allowed: \"%1\"").arg(record.name.left(MaxZipNameLength)));
        if (seen.contains(record.name.toLower()))
            return fail(QStringLiteral("%1: the name appears twice").arg(record.name));
        seen.insert(record.name.toLower(), 1);
        if (static_cast<qsizetype>(record.size) > MaxZipEntryBytes)
            return fail(QStringLiteral("%1: larger than %2 MiB").arg(record.name).arg(MaxZipEntryBytes / ZipMiB));
        total += static_cast<qsizetype>(record.size);
        if (total > MaxZipTotalBytes)
            return fail(QStringLiteral("the entries together are larger than %1 MiB").arg(MaxZipTotalBytes / ZipMiB));
        ZipEntry entry;
        entry.name = record.name;
        if (!readEntryData(cursor, record, entry.data, problem))
            return fail(problem);
        result.entries.push_back(std::move(entry));
    }
    if (at != static_cast<qsizetype>(*directoryOffset) + static_cast<qsizetype>(*directorySize))
        return fail(QStringLiteral("the directory of the archive has the wrong size"));
    return result;
}

std::optional<QByteArray> writeStoredZip(const std::vector<ZipEntry> &entries, QString *error)
{
    const auto fail = [error](const QString &problem) -> std::optional<QByteArray> {
        if (error != nullptr)
            *error = problem;
        return std::nullopt;
    };
    if (entries.size() > static_cast<std::size_t>(MaxZipEntries))
        return fail(QStringLiteral("more than %1 entries").arg(MaxZipEntries));
    QHash<QString, int> seen;
    qsizetype total = 0;
    for (const ZipEntry &entry : entries) {
        if (!isValidZipEntryName(entry.name))
            return fail(QStringLiteral("an entry name is not allowed: \"%1\"").arg(entry.name.left(MaxZipNameLength)));
        if (seen.contains(entry.name.toLower()))
            return fail(QStringLiteral("%1: the name appears twice").arg(entry.name));
        seen.insert(entry.name.toLower(), 1);
        if (entry.data.size() > MaxZipEntryBytes)
            return fail(QStringLiteral("%1: larger than %2 MiB").arg(entry.name).arg(MaxZipEntryBytes / ZipMiB));
        total += entry.data.size();
        if (total > MaxZipTotalBytes)
            return fail(QStringLiteral("the entries together are larger than %1 MiB").arg(MaxZipTotalBytes / ZipMiB));
    }

    QByteArray out;
    QByteArray directory;
    for (const ZipEntry &entry : entries) {
        const QByteArray name = entry.name.toLatin1();
        const quint32 crc = zipCrc32(entry.data);
        const auto size = static_cast<quint32>(entry.data.size());
        const auto offset = static_cast<quint32>(out.size());

        appendU32(out, LocalSignature);
        appendU16(out, VersionNeeded);
        appendU16(out, FlagUtf8);
        appendU16(out, MethodStored);
        appendU16(out, FixedTime);
        appendU16(out, FixedDate);
        appendU32(out, crc);
        appendU32(out, size);
        appendU32(out, size);
        appendU16(out, static_cast<quint16>(name.size()));
        appendU16(out, 0);
        out.append(name);
        out.append(entry.data);

        appendU32(directory, CentralSignature);
        appendU16(directory, VersionNeeded);
        appendU16(directory, VersionNeeded);
        appendU16(directory, FlagUtf8);
        appendU16(directory, MethodStored);
        appendU16(directory, FixedTime);
        appendU16(directory, FixedDate);
        appendU32(directory, crc);
        appendU32(directory, size);
        appendU32(directory, size);
        appendU16(directory, static_cast<quint16>(name.size()));
        appendU16(directory, 0); // extra
        appendU16(directory, 0); // comment
        appendU16(directory, 0); // disk
        appendU16(directory, 0); // internal attributes
        appendU32(directory, 0); // external attributes
        appendU32(directory, offset);
        directory.append(name);
    }
    const auto directoryOffset = static_cast<quint32>(out.size());
    out.append(directory);
    appendU32(out, EndSignature);
    appendU16(out, 0);
    appendU16(out, 0);
    appendU16(out, static_cast<quint16>(entries.size()));
    appendU16(out, static_cast<quint16>(entries.size()));
    appendU32(out, static_cast<quint32>(directory.size()));
    appendU32(out, directoryOffset);
    appendU16(out, 0);
    return out;
}

} // namespace galleond
