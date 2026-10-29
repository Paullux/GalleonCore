// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "FpsSetup.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
static void initFpsSetup() { Q_INIT_RESOURCE(fps_setup); } // NOLINT(misc-use-anonymous-namespace) -- Qt resource symbol is global.
namespace galleond {
FpsSetup::FpsSetup(QObject *parent) : QObject(parent) {
    initFpsSetup();
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(120000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_error = tr("Configuration timed out. Some selected items may already have been applied; inspect the backups before retrying.");
        m_process.kill();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
        if (m_output.size() > qsizetype{2} * 1024 * 1024) {
            m_error = tr("Setup response exceeded the size limit.");
            m_process.kill();
        }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_timeout.stop(); m_busy = false;
        m_error = tr("Could not start Python 3. Install python3 to configure launchers.");
        Q_EMIT changed();
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &FpsSetup::finished);
}
FpsSetup::~FpsSetup() {
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill(); m_process.waitForFinished(1000);
    }
}
void FpsSetup::start(const QVariantMap &request) {
    if (m_busy) return;
    m_error.clear();
#ifdef Q_OS_LINUX
    const QString python = QStandardPaths::findExecutable(QStringLiteral("python3"));
    QFile source(QStringLiteral(":/fpssetup/tools/fps_setup.py"));
    const QString path = m_temporary.filePath(QStringLiteral("fps_setup.py"));
    QFile target(path);
    if (python.isEmpty() || !m_temporary.isValid() || !source.open(QIODevice::ReadOnly)
        || !target.open(QIODevice::WriteOnly)) {
        m_error = tr("Python 3 is required and the setup helper must be readable.");
        Q_EMIT changed(); return;
    }
    const QByteArray script = source.readAll();
    if (target.write(script) != script.size()) {
        m_error = tr("Could not prepare the setup helper."); Q_EMIT changed(); return;
    }
    target.close();
    m_action = request.value(QStringLiteral("action")).toString();
    m_output.clear(); m_busy = true;
    m_process.start(python, {QStringLiteral("-I"), QStringLiteral("-B"), path});
    m_process.write(QJsonDocument::fromVariant(request).toJson(QJsonDocument::Compact));
    m_process.closeWriteChannel();
    m_timeout.start();
#else
    Q_UNUSED(request)
    m_error = tr("Launcher configuration is available only on Linux.");
#endif
    Q_EMIT changed();
}
void FpsSetup::sourceFailed(const QString &message) {
    m_error = tr("MangoHud files were prepared, but saving the Bosun FPS source failed: %1").arg(message.left(300));
    for (auto &entry : m_results) {
        auto result = entry.toMap();
        if (result.value(QStringLiteral("common")).toBool()) {
            result[QStringLiteral("status")] = QStringLiteral("error");
            result[QStringLiteral("detail")] = m_error;
            entry = result;
        }
    }
    Q_EMIT changed();
}
void FpsSetup::scan() { start({{QStringLiteral("action"), QStringLiteral("scan")}}); }
void FpsSetup::apply(const QStringList &ids) {
    if (m_busy || ids.isEmpty()) return;
    QVariantList selected;
    for (const auto &entry : m_rows) {
        const auto row = entry.toMap();
        if (row.value(QStringLiteral("enabled")).toBool() && ids.contains(row.value(QStringLiteral("id")).toString()))
            selected.append(row);
    }
    if (selected.isEmpty()) return;
    m_results.clear();
    start({{QStringLiteral("action"), QStringLiteral("apply")}, {QStringLiteral("selected"), selected}});
}
void FpsSetup::restore(const QString &token) {
    if (m_busy || token.isEmpty()) return;
    // Only offer the backup tokens returned by this session's helper.
    for (const auto &entry : m_results) {
        if (entry.toMap().value(QStringLiteral("backup")).toString() != token) continue;
        m_restoreToken = token;
        start({{QStringLiteral("action"), QStringLiteral("restore")}, {QStringLiteral("token"), token}});
        return;
    }
}
void FpsSetup::finished(int code, QProcess::ExitStatus status) {
    m_timeout.stop(); m_busy = false;
    m_output += m_process.readAllStandardOutput();
    const auto object = m_output.size() <= qsizetype{2} * 1024 * 1024 ? QJsonDocument::fromJson(m_output).object() : QJsonObject();
    if (code != 0 || status != QProcess::NormalExit || object.isEmpty() || object.contains(QStringLiteral("error"))) {
        if (m_error.isEmpty()) m_error = object.value(QStringLiteral("error")).toString(tr("Setup failed. No success can be confirmed; inspect the backup folder if files were changed."));
    } else if (m_action == QStringLiteral("scan")) {
        const auto rows = object.value(QStringLiteral("rows")).toVariant().toList();
        if (rows.size() <= 1024) m_rows = rows;
    } else if (m_action == QStringLiteral("apply")) {
        m_results = object.value(QStringLiteral("results")).toVariant().toList();
        bool configured = false;
        for (const auto &entry : m_results) {
            if (entry.toMap().value(QStringLiteral("common")).toBool()) configured = true;
        }
        if (configured) Q_EMIT commonConfigured();
    } else if (m_action == QStringLiteral("restore")) {
        for (auto &entry : m_results) {
            auto result = entry.toMap();
            if (result.value(QStringLiteral("backup")).toString() == m_restoreToken) {
                result[QStringLiteral("status")] = QStringLiteral("restored");
                result[QStringLiteral("detail")] = object.value(QStringLiteral("detail")).toString();
                result[QStringLiteral("backup")] = QString();
                entry = result;
            }
        }
    }
    const bool refreshRows = m_error.isEmpty() && (m_action == QStringLiteral("apply") || m_action == QStringLiteral("restore"));
    Q_EMIT changed();
    if (refreshRows) scan();
}
}
