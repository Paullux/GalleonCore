// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A minimal reader and writer for ZIP archives whose entries are STORED (not
// compressed). This is the container of a `.gc.theme` file: icons and
// wallpapers are already JPEG or PNG, so compressing them again gains nothing,
// and refusing compression removes the whole class of decompression bombs and
// the need for a decompressor. Any ordinary zip tool opens the result.
//
// An archive is external data. Everything is validated before a byte is used:
// the end-of-archive record, every offset and length against the real size,
// both copies of each header against each other, the CRC-32 of each entry, the
// names (no path can leave the archive's own folder), the counts and the sizes.
// A single problem refuses the archive as a whole. Nothing here throws.

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace galleond {

struct ZipEntry
{
    QString name; // "theme.json", "icons/mute.png"
    QByteArray data;

    friend bool operator==(const ZipEntry &, const ZipEntry &) = default;
};

inline constexpr qsizetype ZipMiB = qsizetype{1024} * 1024;
inline constexpr qsizetype MaxZipArchiveBytes = 24 * ZipMiB;
inline constexpr qsizetype MaxZipEntryBytes = 8 * ZipMiB;
inline constexpr qsizetype MaxZipTotalBytes = 20 * ZipMiB;
inline constexpr int MaxZipEntries = 64;
inline constexpr int MaxZipNameLength = 100;

struct ZipReadResult
{
    std::vector<ZipEntry> entries; // empty when `errors` is not
    QStringList errors;            // one line each
};

// Reads an archive made of stored entries. Directory entries are skipped.
[[nodiscard]] ZipReadResult readStoredZip(const QByteArray &archive);

// An entry name an archive may hold: slash-separated parts of letters, digits,
// '.', '_' and '-', none of them "." or "..", at most MaxZipNameLength long.
[[nodiscard]] bool isValidZipEntryName(const QString &name);

// Writes the entries as a stored archive. Returns nothing (and fills `error`)
// when a name is not valid, a name repeats (also ignoring case), or a limit is
// exceeded: the writer keeps to the rules the reader enforces.
[[nodiscard]] std::optional<QByteArray> writeStoredZip(const std::vector<ZipEntry> &entries, QString *error = nullptr);

// CRC-32 (IEEE, as zip uses it).
[[nodiscard]] quint32 zipCrc32(QByteArrayView data);

} // namespace galleond
