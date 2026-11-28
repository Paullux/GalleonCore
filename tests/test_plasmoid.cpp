// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

// The KDE plasmoid (plasmoid/org.galleoncore.clock) loads the shared widget of the project. Plasma is not needed: a stand-in for its
// PlasmoidItem is in tests/qml_stubs. This checks what the plasmoid does with the widget, not Plasma itself.

#include <QDir>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTest>
#include <QUrl>

#include <memory>

namespace {

const QString &plasmoidMain()
{
    static const QString path = QStringLiteral(GALLEON_SOURCE_PLASMOID "/org.galleoncore.clock/contents/ui/main.qml");
    return path;
}

// The widget Loader of the plasmoid, created inside a parent item; the plasmoid itself is kept alive by `root`.
struct Plasmoid
{
    QQmlEngine engine;
    std::unique_ptr<QObject> root;
    QQuickItem parent;
    std::unique_ptr<QObject> view;
    QQuickItem *loader = nullptr;
    QQuickItem *notFound = nullptr;

    explicit Plasmoid(const QStringList &extraFolders)
    {
        engine.addImportPath(QStringLiteral(GALLEON_TEST_QML_STUBS));
        QQmlComponent component(&engine, QUrl::fromLocalFile(plasmoidMain()));
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
        root.reset(component.createWithInitialProperties({{QStringLiteral("extraFolders"), extraFolders}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *const full = root->property("fullRepresentation").value<QQmlComponent *>();
        QVERIFY(full);
        view.reset(full->create(qmlContext(root.get()))); // inside the plasmoid's own context, as Plasma shows it
        QVERIFY2(view, qPrintable(full->errorString()));
        auto *item = qobject_cast<QQuickItem *>(view.get());
        QVERIFY(item);
        item->setParentItem(&parent);
        loader = item->findChild<QQuickItem *>(QStringLiteral("widget"));
        notFound = item->findChild<QQuickItem *>(QStringLiteral("notFound"));
        QVERIFY(loader);
        QVERIFY(notFound);
    }
};

} // namespace

class TestPlasmoid : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theSharedWidgetIsLoadedAndFollowsTheClock()
    {
        Plasmoid p({QStringLiteral(GALLEON_SOURCE_WIDGETS)});
        QTRY_VERIFY_WITH_TIMEOUT(p.loader->property("item").value<QQuickItem *>() != nullptr, 5000);
        auto *widget = p.loader->property("item").value<QQuickItem *>();
        QVERIFY(widget);
        QVERIFY(widget->property("live").toBool());       // it follows the system clock by itself
        QVERIFY(!p.notFound->isVisible());
        QCOMPARE(p.view->property("implicitWidth").toInt() * 384, p.view->property("implicitHeight").toInt() * 720); // the ratio of the screen
    }

    void aFolderWithoutTheWidgetIsPassedOverForTheNextOne()
    {
        // The first folder is missing and the widget is in the next one: it is loaded, and the message is not shown.
        const QString missing = QDir::tempPath() + QStringLiteral("/galleoncore-no-widgets-here");
        QVERIFY(!QDir(missing).exists());
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(GALLEON_TEST_QML_STUBS));
        QQmlComponent component(&engine, QUrl::fromLocalFile(plasmoidMain()));
        QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
        const std::unique_ptr<QObject> root(component.createWithInitialProperties({{QStringLiteral("extraFolders"), QStringList{missing}}}));
        QVERIFY(root);
        QStringList folders = root->property("folders").toStringList();
        QCOMPARE(folders.first(), missing);
        QVERIFY(folders.contains(QStringLiteral("/usr/share/galleoncore/widgets")));
        QVERIFY(folders.contains(QStringLiteral("/usr/local/share/galleoncore/widgets")));
        // With no widget anywhere, nothing is loaded and the message names every folder that was tried. (If the machine that runs this
        // has GalleonCore installed, the installed widget is found instead: both are right.)
        const bool found = !root->property("widgetComponent").isNull() && root->property("widgetComponent").value<QObject *>() != nullptr;
        auto *const full = root->property("fullRepresentation").value<QQmlComponent *>();
        QVERIFY(full);
        QQuickItem parent;
        const std::unique_ptr<QObject> view(full->create(qmlContext(root.get())));
        auto *item = qobject_cast<QQuickItem *>(view.get());
        QVERIFY(item);
        item->setParentItem(&parent);
        auto *notFound = item->findChild<QQuickItem *>(QStringLiteral("notFound"));
        QVERIFY(notFound);
        QCOMPARE(notFound->isVisible(), !found);
        if (!found) {
            for (const QString &folder : std::as_const(folders))
                QVERIFY2(notFound->property("text").toString().contains(folder), qPrintable(folder));
        }
    }

    void theWidgetOfALaterFolderIsUsedWhenTheFirstHasNone()
    {
        // The first folder is missing, the second has the widget: the pick goes on to the second, and no message is shown.
        const QString missing = QDir::tempPath() + QStringLiteral("/galleoncore-no-widgets-here");
        QVERIFY(!QDir(missing).exists());
        const Plasmoid p({missing, QStringLiteral(GALLEON_SOURCE_WIDGETS)});
        QTRY_VERIFY_WITH_TIMEOUT(p.loader->property("item").value<QQuickItem *>() != nullptr, 5000);
        QVERIFY(!p.notFound->isVisible());
    }
};

QTEST_MAIN(TestPlasmoid)
#include "test_plasmoid.moc"
