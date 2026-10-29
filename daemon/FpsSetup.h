// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
namespace galleond {
// Offline setup runs in a bounded helper, never on the HID/rendering event loop.
class FpsSetup final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit FpsSetup(QObject *parent = nullptr);
    ~FpsSetup() override;
    [[nodiscard]] QVariantList rows() const { return m_rows; }
    [[nodiscard]] QVariantList results() const { return m_results; }
    [[nodiscard]] bool busy() const { return m_busy; }
    [[nodiscard]] QString error() const { return m_error; }
    void sourceFailed(const QString &message);
    Q_INVOKABLE void scan();
    Q_INVOKABLE void apply(const QStringList &ids);
    Q_INVOKABLE void restore(const QString &token);
Q_SIGNALS:
    void changed();
    void commonConfigured();
private:
    void start(const QVariantMap &request);
    void finished(int code, QProcess::ExitStatus status);
    QProcess m_process;
    QTimer m_timeout;
    QTemporaryDir m_temporary;
    QVariantList m_rows, m_results;
    QString m_action, m_restoreToken, m_error;
    QByteArray m_output;
    bool m_busy = false;
};
}
