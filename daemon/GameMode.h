// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "FpsReader.h"
#include <QVariantMap>
namespace galleond {
// Resolves artwork once per captured process; never downloads game assets.
class GameMode {
public:
    QVariantMap update(const FpsSample &sample, const QVariantMap &config,
                       const QVariantMap &sensors = {});
    static QVariantMap readAmdGpuSensors(const QString &root = QStringLiteral("/sys/class/hwmon"));
    static QVariantMap parseSensors(const QByteArray &json);
    [[nodiscard]] static QString titleFromInstallPath(QString path);
    [[nodiscard]] static QString readableTitle(QString title);
    // MangoHud filenames omit .exe; Wine may expose its loader as /proc/PID/exe.
    [[nodiscard]] static QString capturedExecutable(const QString &process, const QString &executable,
                                                     const QByteArray &commandLine, const QString &cwd,
                                                     const QString &wineDriveRoot = {});
private:
    QString m_process;
    QVariantMap m_identity;
    QString m_token;
    qint64 m_sampleAt = 0;
    int m_catalogueRevision = -1;
};
}
