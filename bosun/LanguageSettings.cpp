// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LanguageSettings.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QLocale>
#include <QSettings>

static void initLanguageResources() { Q_INIT_RESOURCE(bosun_languages); } // NOLINT(misc-use-anonymous-namespace) -- Qt resource symbol is global.

namespace bosun {
namespace {
bool supported(const QString &language)
{
    return language == QStringLiteral("en") || language == QStringLiteral("fr")
           || language == QStringLiteral("de") || language == QStringLiteral("es");
}
}

bool CatalogTranslator::select(const QString &language)
{
    if (!supported(language)) return false;
    initLanguageResources();
    QFile file(QStringLiteral(":/bosun/translations/%1.json").arg(language));
    if (!file.open(QIODevice::ReadOnly)) return false;
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return false;
    m_messages = document.object();
    return !m_messages.isEmpty();
}

QString CatalogTranslator::translate(const char *context, const char *source, const char *disambiguation, int n) const
{
    Q_UNUSED(context)
    Q_UNUSED(disambiguation)
    Q_UNUSED(n)
    return m_messages.value(QString::fromUtf8(source)).toString();
}

QString LanguageSettings::systemLanguage(const QStringList &languages)
{
    for (const auto &name : languages) {
        const QString language = name.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
        if (supported(language)) return language;
    }
    return QStringLiteral("en");
}

LanguageSettings::LanguageSettings(QString settingsPath, QObject *parent)
    : QObject(parent), m_settingsPath(std::move(settingsPath))
{
    QString preference = QStringLiteral("system");
    if (!m_settingsPath.isEmpty()) {
        const QSettings settings(m_settingsPath, QSettings::IniFormat);
        preference = settings.value(QStringLiteral("interface/language"), preference).toString();
    }
    if (preference != QStringLiteral("system") && !supported(preference)) {
        qWarning("Bosun: invalid saved interface language; using the system language");
        preference = QStringLiteral("system");
    }
    m_preference = preference;
    m_language = preference == QStringLiteral("system") ? systemLanguage(QLocale::system().uiLanguages()) : preference;
    if (m_translator.select(m_language)) QCoreApplication::installTranslator(&m_translator);
    QLocale::setDefault(preference == QStringLiteral("system") ? QLocale::system() : QLocale(m_language));
}

LanguageSettings::~LanguageSettings()
{
    QCoreApplication::removeTranslator(&m_translator);
    QLocale::setDefault(m_previousLocale);
}

void LanguageSettings::setPreference(const QString &preference)
{
    if (preference == m_preference) return;
    if (preference != QStringLiteral("system") && !supported(preference)) {
        qWarning("Bosun: unsupported interface language ignored");
        return;
    }
    const QString language = preference == QStringLiteral("system") ? systemLanguage(QLocale::system().uiLanguages()) : preference;
    if (!m_translator.select(language)) return;
    m_preference = preference;
    m_language = language;
    QLocale::setDefault(preference == QStringLiteral("system") ? QLocale::system() : QLocale(language));
    if (!m_settingsPath.isEmpty()) {
        QSettings settings(m_settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("interface/language"), preference);
        settings.sync();
        if (settings.status() != QSettings::NoError) qWarning("Bosun: could not save the interface language preference");
    }
    Q_EMIT changed();
}

} // namespace bosun
