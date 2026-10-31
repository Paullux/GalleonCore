// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QLocale>
#include <QTranslator>

namespace bosun {

// Embedded source-text catalogs avoid a runtime dependency on Qt Linguist tools.
// Only shipped catalogs are read; paths and language codes never come from the UI.
class CatalogTranslator final : public QTranslator
{
public:
    bool select(const QString &language);
    [[nodiscard]] QString translate(const char *context, const char *source,
                                    const char *disambiguation = nullptr, int n = -1) const override;
    [[nodiscard]] bool isEmpty() const override { return m_messages.isEmpty(); }
private:
    QJsonObject m_messages;
};

class LanguageSettings final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString preference READ preference WRITE setPreference NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
public:
    // An empty settings path makes previews/tests read-only and isolated.
    explicit LanguageSettings(QString settingsPath, QObject *parent = nullptr);
    ~LanguageSettings() override;
    [[nodiscard]] QString preference() const { return m_preference; }
    [[nodiscard]] QString language() const { return m_language; }
    [[nodiscard]] static QString systemLanguage(const QStringList &languages);
    void setPreference(const QString &preference);
Q_SIGNALS:
    void changed();
private:
    QString m_settingsPath;
    QString m_preference = QStringLiteral("system");
    QString m_language;
    CatalogTranslator m_translator;
    QLocale m_previousLocale;
};

} // namespace bosun
