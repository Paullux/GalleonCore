// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

#include <StoredZip.h>
#include <ThemePackage.h>

#include <QByteArray>

#include <cstddef>
#include <cstdint>

// A .gctheme file comes from someone else: no input may crash the zip reader or
// the theme validator, and a result is either a package or errors, never both
// nor neither. A package that was accepted must also survive being written back
// as an archive and read again.
// Run: ./fuzz_theme_package -max_total_time=60
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const QByteArray bytes(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    const galleond::ThemeParseResult result = galleond::parseThemePackage(bytes);
    if (result.package.has_value() != result.errors.isEmpty())
        __builtin_trap();

    const galleond::ZipReadResult archive = galleond::readStoredZip(bytes);
    if (archive.errors.isEmpty()) {
        const auto again = galleond::writeStoredZip(archive.entries);
        if (!again.has_value() || galleond::readStoredZip(*again).entries != archive.entries)
            __builtin_trap();
    }
    return 0;
}
