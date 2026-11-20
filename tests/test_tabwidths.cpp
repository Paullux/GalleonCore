// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSEngine>
#include <QJSValue>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <numeric>

static void initTabResources() { Q_INIT_RESOURCE(bosun_ui); Q_INIT_RESOURCE(bosun_languages); } // NOLINT(misc-use-anonymous-namespace) -- Qt resource symbol is global.

namespace {
// The width of the window at its narrowest (Bosun.qml: minimumWidth 1000) minus what the bar leaves to the rest of the window (224).
constexpr int NarrowestBar = 776;
constexpr int Spacing = 4;
constexpr int Usual = 70;  // the usual minimum of an unselected tab
constexpr int Floor = 40;  // the narrowest it may be made

// The six fixed tabs of the window, in the order of Bosun.qml, as the language in use writes them (the keys of the catalogs).
QStringList fixedTabs()
{
    return {QStringLiteral("Keyboard"), QStringLiteral("Wallpapers"), QStringLiteral("Widgets"), QStringLiteral("Gaming"),
            QStringLiteral("Screensavers"), QStringLiteral("Dials")};
}
// The names of the themes come from the user's file: they do not change with the language.
QStringList themeNames()
{
    return {QStringLiteral("Contrôle"), QStringLiteral("Multimédia"), QStringLiteral("Jeu Vidéo"), QStringLiteral("Système"),
            QStringLiteral("Pavé Numérique")};
}
} // namespace

class TestTabWidths : public QObject
{
    Q_OBJECT
private:
    QJSEngine m_engine;
    QJSValue m_distribute;
    QJSValue m_fitMinimum;
    QVariantList widths(const QVariantList &preferred, int selected, int available)
    {
        const auto result = m_distribute.call({m_engine.toScriptValue(preferred), QJSValue(selected),
                                              QJSValue(available), QJSValue(4), QJSValue(70)});
        if (result.isError()) qFatal("Could not execute the tab geometry helper");
        return result.toVariant().toList();
    }
    double minimumFor(const QVariantList &preferred, int selected, int available)
    {
        const auto result = m_fitMinimum.call({m_engine.toScriptValue(preferred), QJSValue(selected), QJSValue(available),
                                              QJSValue(Spacing), QJSValue(Usual), QJSValue(Floor)});
        if (result.isError()) qFatal("Could not execute the tab minimum helper");
        return result.toNumber();
    }
    // The title of a fixed tab in `language`, from the catalog of the window (the keys are the English titles).
    static QStringList titlesIn(const QString &language, int extraThemes)
    {
        QFile file(QStringLiteral(":/bosun/translations/%1.json").arg(language));
        if (!file.open(QIODevice::ReadOnly)) qFatal("No catalog for a language");
        const QJsonObject catalog = QJsonDocument::fromJson(file.readAll()).object();
        QStringList titles = themeNames();
        for (int i = 0; i < extraThemes; ++i) titles << QStringLiteral("Thème %1").arg(i + 1);
        titles << QStringLiteral("+");
        for (const QString &key : fixedTabs()) titles << catalog.value(key).toString(key);
        return titles;
    }
    // What a title would like: about 8 pixels a letter and 24 around it (the window measures the real font; this is its order of size).
    static QVariantList preferredOf(const QStringList &titles)
    {
        QVariantList preferred;
        for (const QString &title : titles) preferred << static_cast<int>(title.size()) * 8 + 24;
        return preferred;
    }
    static double total(const QVariantList &widths)
    {
        const double sum = std::accumulate(widths.cbegin(), widths.cend(), 0.0, [](double a, const QVariant &w) { return a + w.toDouble(); });
        return sum + Spacing * static_cast<double>(std::max<qsizetype>(0, widths.size() - 1));
    }
private Q_SLOTS:
    void initTestCase()
    {
        initTabResources();
        QFile source(QStringLiteral(":/bosun/qml/TabWidths.js"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        // The geometry is plain JavaScript; only QML's module pragma is stripped.
        const auto script = QString::fromUtf8(source.readAll()).replace(QStringLiteral(".pragma library"), QString());
        const auto result = m_engine.evaluate(script);
        QVERIFY2(!result.isError(), qPrintable(result.toString()));
        m_distribute = m_engine.globalObject().property(QStringLiteral("distribute"));
        QVERIFY(m_distribute.isCallable());
        m_fitMinimum = m_engine.globalObject().property(QStringLiteral("fitMinimum"));
        QVERIFY(m_fitMinimum.isCallable());
    }
    void selectedTitleKeepsItsFullWidth()
    {
        const auto actual = widths({120, 200, 120}, 1, 348);
        QCOMPARE(actual, (QVariantList{70.0, 200.0, 70.0}));
    }
    void shortageIsSharedEqually()
    {
        const auto actual = widths({130, 200, 150}, 1, 448);
        QCOMPARE(actual, (QVariantList{110.0, 200.0, 130.0}));
    }
    void tabsAtMinimumStopShrinking()
    {
        const auto actual = widths({80, 200, 180}, 1, 368);
        QCOMPARE(actual, (QVariantList{70.0, 200.0, 90.0}));
    }
    void insufficientSpaceKeepsScrollableMinimums()
    {
        const auto actual = widths({120, 200, 120}, 1, 250);
        QCOMPARE(actual, (QVariantList{70.0, 200.0, 70.0}));
    }
    void spareSpaceIsShared()
    {
        const auto actual = widths({100, 120}, 1, 264);
        QCOMPARE(actual, (QVariantList{120.0, 140.0}));
    }
    void noTabsHaveNoWidth()
    {
        QVERIFY(widths({}, 0, 300).isEmpty());
    }

    // ---- every tab stays in view, in every language
    void theUsualMinimumIsKeptWhenEverythingFits()
    {
        QCOMPARE(minimumFor({100, 100, 100}, 0, 1000), double(Usual));
    }
    void theMinimumFallsToWhatFitsAndNotBelowTheFloor()
    {
        // Three other tabs, the selected one whole (200) and 3 gaps: (400 - 12 - 200) / 3 = 62.
        QCOMPARE(minimumFor({100, 200, 100, 100}, 1, 400), 62.0);
        // Nothing can fit: the floor, and the bar will scroll.
        QCOMPARE(minimumFor({100, 200, 100, 100}, 1, 100), double(Floor));
    }
    void aSingleTabNeedsNoShortening()
    {
        QCOMPARE(minimumFor({150}, 0, 100), double(Usual));
    }
    void everyTabIsInViewInEveryLanguageAtTheNarrowestWindow_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<int>("extraThemes");
        for (const QString &language : {QStringLiteral("en"), QStringLiteral("fr"), QStringLiteral("de"), QStringLiteral("es")}) {
            QTest::newRow(qPrintable(language + " with 5 themes")) << language << 0;
            QTest::newRow(qPrintable(language + " with 7 themes")) << language << 2;
        }
    }
    void everyTabIsInViewInEveryLanguageAtTheNarrowestWindow()
    {
        QFETCH(const QString, language);
        QFETCH(const int, extraThemes);
        const QStringList titles = titlesIn(language, extraThemes);
        const QVariantList preferred = preferredOf(titles);
        // The last tab is "Dials" and the first is a theme: both ends must be reachable and whole when selected.
        for (const int selected : {0, static_cast<int>(titles.size()) - 1}) {
            const double minimum = minimumFor(preferred, selected, NarrowestBar);
            QVERIFY2(minimum >= Floor && minimum <= Usual, qPrintable(QString::number(minimum)));
            const QVariantList actual = m_distribute
                .call({m_engine.toScriptValue(preferred), QJSValue(selected), QJSValue(NarrowestBar), QJSValue(Spacing), QJSValue(minimum)})
                .toVariant().toList();
            QCOMPARE(actual.size(), titles.size());
            QVERIFY2(total(actual) <= NarrowestBar + 0.5,
                     qPrintable(QStringLiteral("%1, tab %2: %3 px for %4").arg(language).arg(selected).arg(total(actual)).arg(NarrowestBar)));
            QVERIFY(actual.at(selected).toDouble() >= preferred.at(selected).toDouble()); // the selected title stays whole
            for (const QVariant &width : actual) QVERIFY(width.toDouble() >= Floor - 0.01);
        }
    }
    void aLongerLanguageIsShortenedMoreThanAShorterOne()
    {
        // The same window: the German titles ask for more than the English ones, so the narrowest tab that fits is narrower.
        const auto minimumIn = [&](const QString &language) {
            const QVariantList preferred = preferredOf(titlesIn(language, 0));
            return minimumFor(preferred, static_cast<int>(preferred.size()) - 1, NarrowestBar);
        };
        QVERIFY(minimumIn(QStringLiteral("de")) < minimumIn(QStringLiteral("en")));
    }
};
QTEST_GUILESS_MAIN(TestTabWidths)
#include "test_tabwidths.moc"
