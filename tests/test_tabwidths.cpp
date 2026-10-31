// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QTest>

static void initTabResources() { Q_INIT_RESOURCE(bosun_ui); } // NOLINT(misc-use-anonymous-namespace) -- Qt resource symbol is global.

class TestTabWidths : public QObject
{
    Q_OBJECT
private:
    QJSEngine m_engine;
    QJSValue m_distribute;
    QVariantList widths(const QVariantList &preferred, int selected, int available)
    {
        const auto result = m_distribute.call({m_engine.toScriptValue(preferred), QJSValue(selected),
                                              QJSValue(available), QJSValue(4), QJSValue(70)});
        if (result.isError()) qFatal("Could not execute the tab geometry helper");
        return result.toVariant().toList();
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
};
QTEST_GUILESS_MAIN(TestTabWidths)
#include "test_tabwidths.moc"
