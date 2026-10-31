// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include "LanguageSettings.h"
#include "TrayMenu.h"
#include "UpdateChecker.h"

#include <memory>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

class TestLanguages : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void systemUsesFirstSupportedPreference()
    {
        QCOMPARE(bosun::LanguageSettings::systemLanguage({"fr-CA", "en-US"}), QStringLiteral("fr"));
        QCOMPARE(bosun::LanguageSettings::systemLanguage({"ja-JP", "fr_FR"}), QStringLiteral("fr"));
        QCOMPARE(bosun::LanguageSettings::systemLanguage({"en-GB", "fr-FR"}), QStringLiteral("en"));
        QCOMPARE(bosun::LanguageSettings::systemLanguage({"de-DE"}), QStringLiteral("de"));
        QCOMPARE(bosun::LanguageSettings::systemLanguage({"es-MX"}), QStringLiteral("es"));
        QCOMPARE(bosun::LanguageSettings::systemLanguage({"ja-JP"}), QStringLiteral("en"));
    }
    void preferenceSurvivesRestartAndRejectsUnknownLanguages()
    {
        const QTemporaryDir directory;
        const auto path = directory.filePath("bosun.conf");
        {
            bosun::LanguageSettings settings(path);
            settings.setPreference("fr");
            QCOMPARE(QCoreApplication::translate("TrayMenu", "Show"), QStringLiteral("Afficher"));
            settings.setPreference("../../other");
            QCOMPARE(settings.preference(), QStringLiteral("fr"));
        }
        bosun::LanguageSettings restarted(path);
        QCOMPARE(restarted.preference(), QStringLiteral("fr"));
        restarted.setPreference("en");
        QCOMPARE(bosun::trayEntries(false, true).first().label, QStringLiteral("Show"));
        restarted.setPreference("system");
        QCOMPARE(restarted.preference(), QStringLiteral("system"));
    }
    void invalidSavedPreferenceFallsBackToSystem()
    {
        const QTemporaryDir directory;
        const auto path = directory.filePath("bosun.conf");
        { QSettings settings(path, QSettings::IniFormat); settings.setValue("interface/language", "unknown"); }
        const bosun::LanguageSettings languages(path);
        QCOMPARE(languages.preference(), QStringLiteral("system"));
    }
    void retranslationChangesQmlAndTrayTogether()
    {
        bosun::LanguageSettings languages({});
        languages.setPreference("en");
        QQmlEngine engine;
        connect(&languages, &bosun::LanguageSettings::changed, &engine, &QQmlEngine::retranslate);
        QQmlComponent component(&engine);
        component.setData("import QtQml\nQtObject { property string label: qsTr(\"Keyboard\") }", QUrl());
        const std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QCOMPARE(object->property("label").toString(), QStringLiteral("Keyboard"));
        languages.setPreference("fr");
        QCOMPARE(object->property("label").toString(), QStringLiteral("Clavier"));
        QCOMPARE(bosun::trayEntries(false, false).first().label, QStringLiteral("Afficher"));
        QCOMPARE(QCoreApplication::translate("GameModePage", "Prêt — lancez un jeu"), QStringLiteral("Prêt — lancez un jeu"));
        languages.setPreference("de");
        QCOMPARE(object->property("label").toString(), QStringLiteral("Tastatur"));
        QCOMPARE(bosun::trayEntries(false, false).first().label, QStringLiteral("Anzeigen"));
        languages.setPreference("es");
        QCOMPARE(object->property("label").toString(), QStringLiteral("Teclado"));
        QCOMPARE(bosun::trayEntries(false, false).first().label, QStringLiteral("Mostrar"));
        languages.setPreference("en");
        QCOMPARE(object->property("label").toString(), QStringLiteral("Keyboard"));
        QCOMPARE(QCoreApplication::translate("GameModePage", "Prêt — lancez un jeu"), QStringLiteral("Ready — start a game"));
    }
    void cachedUpdateStateFollowsLanguageWithoutNetworkRequests()
    {
        const QTemporaryDir directory;
        bosun::LanguageSettings languages({});
        languages.setPreference("en");
        const bosun::UpdateChecker updates({"0.2.3", directory.filePath("updates.conf"), {}});
        QCOMPARE(updates.status(), QStringLiteral("Updates have not been checked in this session."));
        languages.setPreference("fr");
        QCOMPARE(updates.status(), QStringLiteral("Aucune vérification de mise à jour dans cette session."));
    }
    void catalogsHaveMatchingKeysAndPlaceholders()
    {
        // Constructing the settings registers resources even in this static-library test.
        const bosun::LanguageSettings languages({});
        QFile english(":/bosun/translations/en.json");
        QVERIFY(english.open(QIODevice::ReadOnly));
        const auto en = QJsonDocument::fromJson(english.readAll()).object();
        const QRegularExpression placeholder(QStringLiteral("%(?:[1-9][0-9]*|n)"));
        const auto placeholders = [&placeholder](const QString &text) {
            QStringList result;
            auto matches = placeholder.globalMatch(text);
            while (matches.hasNext()) result.append(matches.next().captured());
            result.sort();
            return result;
        };
        for (const QString &language : {QStringLiteral("fr"), QStringLiteral("de"), QStringLiteral("es")}) {
            QFile file(QStringLiteral(":/bosun/translations/%1.json").arg(language));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
            QCOMPARE(en.keys(), catalog.keys());
            for (auto it = en.begin(); it != en.end(); ++it) {
                QVERIFY2(!it.value().toString().isEmpty(), qPrintable(it.key()));
                QVERIFY2(!catalog.value(it.key()).toString().isEmpty(), qPrintable(it.key()));
                QCOMPARE(placeholders(it.key()), placeholders(it.value().toString()));
                QCOMPARE(placeholders(it.key()), placeholders(catalog.value(it.key()).toString()));
            }
        }
    }
};
QTEST_GUILESS_MAIN(TestLanguages)
#include "test_languages.moc"
