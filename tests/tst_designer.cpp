/*
 * BrReport - Band-oriented report generator for C++/Qt
 * Copyright 2026 João Elson
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "brdesignerwidget.h"
#include "brdesignscene_p.h"
#include "brdesignview_p.h"
#include "brexpressioneditor_p.h"
#include "brparameterseditor_p.h"
#include "brpropertyeditor_p.h"
#include "brreportwidgets.h"
#include "brreportcore.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTest>
#include <QTextBlock>
#include <QToolButton>
#include <QTimer>
#include <QTreeWidget>

#include <cmath>

namespace {

QAction *action(QWidget *w, const QString &text)
{
    for (QAction *a : w->findChildren<QAction *>())
        if (a->text().remove(QLatin1Char('&')) == text)
            return a;
    return nullptr;
}

QTreeWidgetItem *treeItem(QTreeWidget *tree, const QString &text)
{
    const QList<QTreeWidgetItem *> items = tree->findItems(text, Qt::MatchExactly | Qt::MatchRecursive);
    return items.isEmpty() ? nullptr : items.first();
}

// Warnings printed while a report runs, to detect formulas the engine rejects.
QStringList g_warnings;
void recordWarning(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type == QtWarningMsg)
        g_warnings << message;
}

void sendMouse(QWidget *widget, QEvent::Type type, const QPoint &pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(pos), QPointF(widget->mapToGlobal(pos)), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

/*! Designer with a detail band of 30 mm and another of 20 mm, shown so that the mouse works. */
struct LayoutFixture
{
    BrDesignerWidget widget;
    BrDesignScene *scene = nullptr;
    BrDesignView *view = nullptr;
    BrBand *first = nullptr;
    BrBand *second = nullptr;

    LayoutFixture()
    {
        BrReport *report = widget.report();
        scene = widget.findChild<BrDesignScene *>();
        view = widget.findChild<BrDesignView *>();
        report->clearLayout();
        first = report->addBand(Br::BandType::Detail, 30);
        second = report->addBand(Br::BandType::Summary, 20);
        commit();
        widget.show();
        view->setTransform(QTransform::fromScale(4, 4)); // the snap distance is 5 pixels
        view->centerOn(80, 40);
    }

    void commit()
    {
        emit scene->layoutEdited();
        QCoreApplication::processEvents();
    }

    BrDBText *addText(BrBand *band, const QRectF &geometry)
    {
        auto *text = new BrDBText(QStringLiteral("name"), band);
        text->setGeometry(geometry);
        commit();
        return text;
    }

    QPointF toScene(BrControl *control, const QPointF &local) const
    {
        const QList<QGraphicsItem *> items = scene->items();
        for (QGraphicsItem *item : items)
            if (item->type() == BrControlItem::Type
                && static_cast<BrControlItem *>(item)->control() == control)
                return item->mapToScene(local);
        return {};
    }

    /*! Drags from \a from by \a mm, in steps as a real mouse does. */
    void drag(const QPointF &from, const QPointF &mm)
    {
        QWidget *viewport = view->viewport();
        const QPoint start = view->mapFromScene(from);
        const QPoint end = view->mapFromScene(from + mm);
        sendMouse(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int i = 1; i <= 4; ++i)
            sendMouse(viewport, QEvent::MouseMove, start + (end - start) * i / 4, Qt::NoButton, Qt::LeftButton);
        sendMouse(viewport, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();
    }
};

BrBand *detailBand(BrReport *report)
{
    for (BrBand *b : report->findChildren<BrBand *>())
        if (b->bandType() == Br::BandType::Detail)
            return b;
    return nullptr;
}

} // namespace

class TstDesigner : public QObject
{
    Q_OBJECT
private slots:
    void parametersDialog()
    {
        BrReport report;
        report.addParameter(QStringLiteral("start"), QStringLiteral("From"), QStringLiteral("date"),
                            QStringLiteral("2026-02-01"));
        report.addParameter(QStringLiteral("city"), QString(), QStringLiteral("string"), QStringLiteral("Recife"));
        report.addParameter(QStringLiteral("limit"), QString(), QStringLiteral("integer"), 5);
        report.addParameter(QStringLiteral("all"), QString(), QStringLiteral("boolean"), true);
        report.addParameter(QStringLiteral("status"), QString(), QStringLiteral("integer"), 2,
                            {QVariantMap{{QStringLiteral("value"), 1}, {QStringLiteral("caption"), QStringLiteral("Active")}},
                             QVariantMap{{QStringLiteral("value"), 2}, {QStringLiteral("caption"), QStringLiteral("Inactive")}}});
        report.setVariable(QStringLiteral("city"), QStringLiteral("Belem"));

        BrParametersDialog dialog(&report);
        QCOMPARE(dialog.findChild<QDateEdit *>(QStringLiteral("start"))->date(), QDate(2026, 2, 1));
        auto *city = dialog.findChild<QLineEdit *>(QStringLiteral("city"));
        QCOMPARE(city->text(), QStringLiteral("Belem")); // the variable wins over the default
        QCOMPARE(dialog.findChild<QSpinBox *>(QStringLiteral("limit"))->value(), 5);
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("all"))->isChecked());
        auto *status = dialog.findChild<QComboBox *>(QStringLiteral("status"));
        QCOMPARE(status->currentText(), QStringLiteral("Inactive"));

        city->setText(QStringLiteral("Natal"));
        status->setCurrentIndex(0);
        const QVariantMap values = dialog.values();
        QCOMPARE(values.value(QStringLiteral("city")).toString(), QStringLiteral("Natal"));
        QCOMPARE(values.value(QStringLiteral("status")).toInt(), 1);
        QCOMPARE(values.value(QStringLiteral("start")).toDate(), QDate(2026, 2, 1));
        QCOMPARE(report.variable(QStringLiteral("city")).toString(), QStringLiteral("Belem"));

        // Canceling stops printing; a report without parameters does not ask
        QTimer::singleShot(0, this, []() { QApplication::activeModalWidget()->close(); });
        QVERIFY(!BrPrinting::askParameters(&report));
        BrReport plain;
        QVERIFY(BrPrinting::askParameters(&plain));

        // Accepting stores the answers and discards the prepared document
        QVERIFY(report.prepare());
        QTimer::singleShot(0, this, []() {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            modal->findChild<QLineEdit *>(QStringLiteral("city"))->setText(QStringLiteral("Natal"));
            modal->accept();
        });
        QVERIFY(BrPrinting::askParameters(&report));
        QCOMPARE(report.variable(QStringLiteral("city")).toString(), QStringLiteral("Natal"));
        QVERIFY(!report.isPrepared());
    }

    void parametersEditor()
    {
        const QVariantList parameters = {
            QVariantMap{{QStringLiteral("name"), QStringLiteral("start")},
                        {QStringLiteral("caption"), QStringLiteral("From")},
                        {QStringLiteral("type"), QStringLiteral("date")},
                        {QStringLiteral("default"), QStringLiteral("today")}},
            QVariantMap{{QStringLiteral("name"), QStringLiteral("status")},
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("choices"),
                         QVariantList{QStringLiteral("open"),
                                      QVariantMap{{QStringLiteral("value"), QString()},
                                                  {QStringLiteral("caption"), QStringLiteral("All")}}}}}};
        BrReport report;
        BrParametersEditor editor(parameters, &report);
        QCOMPARE(editor.parameters(), parameters);

        auto *table = editor.findChild<QTableWidget *>();
        QCOMPARE(table->item(1, 4)->text(), QStringLiteral("open; =All"));

        // Move down keeps every column of both rows
        table->setCurrentCell(0, 0);
        for (QPushButton *button : editor.findChildren<QPushButton *>())
            if (button->text() == QStringLiteral("Move down"))
                button->click();
        QCOMPARE(editor.parameters(), QVariantList({parameters.at(1), parameters.at(0)}));

        // Invalid input keeps the dialog open
        QTimer::singleShot(0, this, []() { QApplication::activeModalWidget()->close(); });
        table->item(0, 0)->setText(QStringLiteral("1 bad"));
        editor.accept();
        QVERIFY(editor.result() != QDialog::Accepted);
    }

    void parametersChoicesFromSql()
    {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("choices"));
            db.setDatabaseName(QStringLiteral(":memory:"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE customer (id INTEGER, name TEXT)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO customer VALUES (7,'Ana'),(9,'Bruno')")));
        }
        QVariantMap parameter{{QStringLiteral("name"), QStringLiteral("customer")},
                              {QStringLiteral("type"), QStringLiteral("integer")},
                              {QStringLiteral("default"), 9},
                              {QStringLiteral("choicesSql"), QStringLiteral("SELECT id, name FROM customer ORDER BY name")},
                              {QStringLiteral("choicesConnection"), QStringLiteral("choices")}};
        BrReport report;
        report.setParameters({parameter});
        {
            BrParametersDialog dialog(&report);
            auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("customer"));
            QVERIFY(combo);
            QCOMPARE(combo->count(), 2);
            QCOMPARE(combo->currentText(), QStringLiteral("Bruno"));
            QCOMPARE(dialog.values().value(QStringLiteral("customer")).toInt(), 9);
            QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("errors")));
        }

        // When the list cannot be loaded, the user is told and can type the value
        parameter[QStringLiteral("choicesSql")] = QStringLiteral("SELECT * FROM missing");
        report.setParameters({parameter});
        {
            BrParametersDialog dialog(&report);
            QVERIFY(dialog.findChild<QSpinBox *>(QStringLiteral("customer")));
            QVERIFY(dialog.findChild<QLabel *>(QStringLiteral("errors")));
        }

        // The editor keeps the query, shows it read-only and can remove it
        parameter[QStringLiteral("default")] = QStringLiteral("9");
        BrParametersEditor editor({parameter}, &report);
        QCOMPARE(editor.parameters(), QVariantList{parameter});
        auto *table = editor.findChild<QTableWidget *>();
        QVERIFY(!(table->item(0, 4)->flags() & Qt::ItemIsEditable));
        table->setCurrentCell(0, 0);
        QTimer::singleShot(0, this, []() {
            auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            auto *sql = modal->findChild<QPlainTextEdit *>();
            QCOMPARE(sql->toPlainText(), QStringLiteral("SELECT * FROM missing"));
            sql->clear();
            modal->accept();
        });
        for (QPushButton *button : editor.findChildren<QPushButton *>())
            if (button->text() == QStringLiteral("Choices from SQL..."))
                button->click();
        QVERIFY(!editor.parameters().first().toMap().contains(QStringLiteral("choicesSql")));
        QVERIFY(table->item(0, 4)->text().isEmpty());
        QVERIFY(table->item(0, 4)->flags() & Qt::ItemIsEditable);
    }

    void parametersInDesigner()
    {
        BrDesignerWidget w;
        w.report()->addParameter(QStringLiteral("city"), QString(), QStringLiteral("string"),
                                 QStringLiteral("Recife"));
        const QByteArray saved = w.toJson();
        QVERIFY(w.loadFromJson(saved));
        bool listed = false;
        for (QTreeWidget *tree : w.findChildren<QTreeWidget *>())
            listed = listed || !tree->findItems(QStringLiteral("city"), Qt::MatchRecursive).isEmpty();
        QVERIFY(listed);

        // The value typed for a preview is remembered but never saved in the report
        const auto answer = [this](const QString &value) {
            QTimer::singleShot(0, this, [value, this]() {
                auto *modal = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                auto *city = modal->findChild<QLineEdit *>(QStringLiteral("city"));
                QCOMPARE(city->text(), value);
                city->setText(QStringLiteral("Natal"));
                modal->accept();
                QTimer::singleShot(0, this, []() { QApplication::activeModalWidget()->close(); });
            });
        };
        answer(QStringLiteral("Recife"));
        w.preview();
        QVERIFY(w.report()->variables().isEmpty());
        QCOMPARE(w.toJson(), saved);
        QVERIFY(!w.isModified());
        answer(QStringLiteral("Natal"));
        w.preview();
    }

    void expressionEditor()
    {
        BrListDataSource ds({{{QStringLiteral("name"), QStringLiteral("Ana")}, {QStringLiteral("Unit Price"), 10}}});
        BrReport report;
        report.setDataSource(&ds);
        report.addParameter(QStringLiteral("city"), QStringLiteral("City"));
        report.setVariable(QStringLiteral("company"), QStringLiteral("ACME"));
        report.registerFunction(QStringLiteral("tax"), [](const QVariantList &) { return 1; });
        auto *band = report.addBand(Br::BandType::Detail);
        auto *text = new BrDBText(band);

        BrExpressionEditor dialog(text, "printIf", QStringLiteral("name"));
        auto *edit = dialog.findChild<QPlainTextEdit *>();
        auto *tree = dialog.findChild<QTreeWidget *>();
        auto *status = dialog.findChild<QLabel *>(QStringLiteral("status"));
        QPushButton *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        QVERIFY(edit && tree && status && ok);
        QVERIFY(ok->isEnabled());

        // Fields, parameters, variables, functions of the application
        for (const char *name : {"name", "Unit Price", "city", "company", "tax()", "upper(text)"})
            QVERIFY2(treeItem(tree, QString::fromLatin1(name)), name);

        // Syntax errors block OK; unknown names only warn
        edit->setPlainText(QStringLiteral("1 +"));
        QVERIFY(!ok->isEnabled());
        edit->setPlainText(QStringLiteral("'open"));
        QVERIFY(!ok->isEnabled());
        edit->setPlainText(QStringLiteral("nome = 1 and nosuch(tax(city, company, NAME))"));
        QVERIFY(ok->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("nome, nosuch()")));
        QVERIFY(!status->text().contains(QStringLiteral("tax")));
        QVERIFY(!status->text().contains(QStringLiteral("city")));
        edit->setPlainText(QString());
        QVERIFY(ok->isEnabled());
        QVERIFY(status->text().isEmpty());

        // Insert a field that needs brackets, a function around the selection, an operator
        edit->setPlainText(QStringLiteral("name"));
        edit->selectAll();
        emit tree->itemDoubleClicked(treeItem(tree, QStringLiteral("upper(text)")), 0);
        QCOMPARE(edit->toPlainText(), QStringLiteral("upper(name)"));
        edit->moveCursor(QTextCursor::End);
        const QList<QToolButton *> buttons = dialog.findChildren<QToolButton *>();
        for (QToolButton *button : buttons)
            if (button->text() == QStringLiteral("&&"))
                button->click();
        emit tree->itemDoubleClicked(treeItem(tree, QStringLiteral("Unit Price")), 0);
        QCOMPARE(edit->toPlainText(), QStringLiteral("upper(name) & [Unit Price]"));

        // The cursor goes between the parentheses of a function with arguments
        edit->clear();
        emit tree->itemDoubleClicked(treeItem(tree, QStringLiteral("year(date)")), 0);
        QCOMPARE(edit->textCursor().position(), 5);
        edit->clear();
        emit tree->itemDoubleClicked(treeItem(tree, QStringLiteral("today()")), 0);
        QCOMPARE(edit->textCursor().position(), 7);

        // The property editor shows one line, so the result is one line too
        edit->setPlainText(QStringLiteral("  1 +\n2  "));
        QCOMPARE(dialog.formula(), QStringLiteral("1 + 2"));

        QCOMPARE(BrExpressionEditor::quotedName(QStringLiteral("sale.total")), QStringLiteral("sale.total"));
        QCOMPARE(BrExpressionEditor::quotedName(QStringLiteral("Or")), QStringLiteral("[Or]"));
        QCOMPARE(BrExpressionEditor::quotedName(QStringLiteral("2nd")), QStringLiteral("[2nd]"));
    }

    void expressionEditorQualifiedNames()
    {
        BrListDataSource ds({{{QStringLiteral("name"), QStringLiteral("Ana")}}});
        BrListDataSource other({{{QStringLiteral("total"), 1}}});
        BrReport report;
        report.setDataSource(&ds);
        report.addDataSource(QStringLiteral("sale"), &other);
        auto *text = new BrDBText(report.addBand(Br::BandType::Detail));
        BrExpressionEditor dialog(text, "dataFormula", QStringLiteral("sale.total + sale.nope"));
        const QString status = dialog.findChild<QLabel *>(QStringLiteral("status"))->text();
        QVERIFY2(status.contains(QStringLiteral("sale.nope")), qPrintable(status));
        QVERIFY2(!status.contains(QStringLiteral("sale.total")), qPrintable(status));
    }

    void expressionEditorWithoutData()
    {
        // No data source: fields cannot be checked, so no name is reported as missing.
        BrReport report;
        auto *text = new BrDBText(report.addBand(Br::BandType::Detail));
        BrExpressionEditor dialog(text, "highlightIf", QStringLiteral("anything > 1"));
        QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("status"))->text().contains(QStringLiteral("anything")));
    }

    void expressionFunctionsExistInEngine()
    {
        // The catalog is written by hand: every name must run in a real report.
        BrListDataSource ds({{{QStringLiteral("name"), QStringLiteral("Ana")}}});
        BrReport report;
        report.setDataSource(&ds);
        auto *band = report.addBand(Br::BandType::Detail);
        const QStringList names = BrExpressionEditor::knownFunctions();
        for (const QString &name : names) {
            auto *text = new BrDBText(band);
            text->setDataFormula(name + QStringLiteral("(1, 1, 1)"));
        }
        g_warnings.clear();
        const QtMessageHandler previous = qInstallMessageHandler(recordWarning);
        const bool prepared = report.prepare();
        qInstallMessageHandler(previous);
        QVERIFY(prepared);
        QVERIFY2(g_warnings.isEmpty(), qPrintable(g_warnings.join(QLatin1Char('\n'))));
    }

    void formulaButtonInPropertyEditor()
    {
        BrReport report;
        auto *text = new BrDBText(report.addBand(Br::BandType::Detail));
        BrPropertyEditor editor;
        editor.setObjects({text});
        int builders = 0;
        const QList<QToolButton *> buttons = editor.findChildren<QToolButton *>();
        for (QToolButton *button : buttons)
            builders += button->toolTip() == QStringLiteral("Open the expression builder");
        QCOMPARE(builders, 3); // dataFormula, printIf, highlightIf

        QTimer::singleShot(0, this, []() {
            auto *dialog = qobject_cast<BrExpressionEditor *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QPlainTextEdit *>()->setPlainText(QStringLiteral("1 = 1"));
            dialog->accept();
        });
        for (QToolButton *button : buttons)
            if (button->toolTip() == QStringLiteral("Open the expression builder")) {
                button->click();
                break;
            }
        QCOMPARE(text->dataFormula() + text->printIf() + text->highlightIf(), QStringLiteral("1 = 1"));
    }

    void moveSnapsToOtherObjects()
    {
        LayoutFixture f;
        BrDBText *fixed = f.addText(f.first, QRectF(10.3, 2, 30, 6));
        BrDBText *moved = f.addText(f.first, QRectF(60, 12.2, 20, 6));
        // The left edge lands 0.5 mm from the right edge of the other object.
        f.drag(f.toScene(moved, QPointF(5, 3)), QPointF(-19.2, 0));
        QVERIFY(f.scene->snapLines().isEmpty()); // only while dragging
        QCOMPARE(moved->x(), fixed->x() + 30);
        f.widget.undo();
        QCOMPARE(detailBand(f.widget.report())->controls().last()->x(), 60.0);
    }

    void moveSnapShowsLineAndCanBeTurnedOff()
    {
        LayoutFixture f;
        f.addText(f.first, QRectF(10.3, 2, 30, 6));
        BrDBText *moved = f.addText(f.first, QRectF(60, 12, 20, 6));
        const QPoint start = f.view->mapFromScene(f.toScene(moved, QPointF(5, 3)));
        const QPoint end = f.view->mapFromScene(f.toScene(moved, QPointF(5, 3)) + QPointF(-19.5, 0));
        QWidget *viewport = f.view->viewport();
        sendMouse(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        sendMouse(viewport, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
        QCOMPARE(f.scene->snapLines().size(), 1);
        QVERIFY(f.scene->snapLines().first().dx() == 0.0);
        sendMouse(viewport, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();

        // Without object snapping only the grid applies.
        action(&f.widget, QStringLiteral("Snap to objects and guides"))->setChecked(false);
        BrDBText *other = f.addText(f.first, QRectF(60, 20, 20, 6));
        f.drag(f.toScene(other, QPointF(5, 3)), QPointF(-19.5, 0));
        QCOMPARE(other->x(), 41.0);
        action(&f.widget, QStringLiteral("Snap to objects and guides"))->setChecked(true);
    }

    void snapAcrossBandsIsHorizontalOnly()
    {
        LayoutFixture f;
        BrDBText *above = f.addText(f.first, QRectF(33.3, 22.3, 30, 6));
        BrDBText *moved = f.addText(f.second, QRectF(60, 5, 20, 6));
        // Columns line up across bands: x is 0.4 mm away and snaps.
        f.drag(f.toScene(moved, QPointF(5, 3)), QPointF(-26.3, 0));
        QCOMPARE(moved->x(), 33.3);

        // The top lands 0.4 mm from the bottom of the object above, seen on the screen, but
        // positions inside different bands are unrelated, so it stays on the grid.
        moved->setGeometry(QRectF(100, 1, 20, 6));
        f.commit();
        const double bottom = f.toScene(above, QPointF(0, 6)).y();
        f.first->setHeight(29.0);
        f.commit();
        const double distance = bottom - f.toScene(moved, QPointF()).y();
        f.drag(f.toScene(moved, QPointF(5, 3)), QPointF(0, distance + 0.4));
        QCOMPARE(moved->y(), std::round(moved->y()));
    }

    void multipleObjectsSnapTogether()
    {
        LayoutFixture f;
        f.addText(f.first, QRectF(10.3, 1, 30, 4));
        BrDBText *a = f.addText(f.first, QRectF(60, 8, 20, 6));
        BrDBText *b = f.addText(f.first, QRectF(90.7, 15, 20, 6));
        const double gap = b->x() - a->x();
        f.scene->selectObjects({a, b});
        f.drag(f.toScene(a, QPointF(5, 3)), QPointF(-19.5, 0));
        QCOMPARE(a->x(), 40.3);
        QCOMPARE(b->x() - a->x(), gap);
    }

    void clickDoesNotPullTheObject()
    {
        LayoutFixture f;
        f.addText(f.first, QRectF(10.3, 2, 30, 6));
        BrDBText *moved = f.addText(f.first, QRectF(40.8, 12, 20, 6));
        const QPoint start = f.view->mapFromScene(f.toScene(moved, QPointF(5, 3)));
        QWidget *viewport = f.view->viewport();
        sendMouse(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        sendMouse(viewport, QEvent::MouseMove, start + QPoint(1, 0), Qt::NoButton, Qt::LeftButton);
        sendMouse(viewport, QEvent::MouseButtonRelease, start + QPoint(1, 0), Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();
        QCOMPARE(moved->x(), 40.8);
        QCOMPARE(f.scene->selectedObjects(), QList<QObject *>{moved});
    }

    void resizeSnapsToAGuide()
    {
        LayoutFixture f;
        f.widget.report()->setVerticalGuides({95.4});
        BrDBText *text = f.addText(f.first, QRectF(20, 5, 30, 6));
        f.scene->selectObjects({text});
        const double offset = f.toScene(text, QPointF()).x() - text->x(); // left margin of the band
        // Middle handle of the right edge, dropped 0.5 mm before the guide
        f.drag(f.toScene(text, QPointF(30, 3)), QPointF(95.4 - 0.5 - offset - 50, 0));
        QCOMPARE(text->x() + text->width() + offset, 95.4);
        QCOMPARE(text->x(), 20.0);
    }

    void guidesFromRulers()
    {
        LayoutFixture f;
        BrReport *report = f.widget.report();
        QWidget *viewport = f.view->viewport();
        const QList<BrRuler *> rulers = f.view->findChildren<BrRuler *>();
        QCOMPARE(rulers.size(), 2);
        BrRuler *left = nullptr;
        for (BrRuler *r : rulers)
            if (r->height() > r->width())
                left = r;
        QVERIFY(left);

        // Dragged from the left ruler into the view: a vertical guide on the grid
        const QPoint target = f.view->mapFromScene(QPointF(70.2, 40));
        const auto toRuler = [&](const QPoint &p) { return left->mapFrom(f.view, viewport->mapTo(f.view, p)); };
        sendMouse(left, QEvent::MouseButtonPress, QPoint(5, target.y()), Qt::LeftButton, Qt::LeftButton);
        sendMouse(left, QEvent::MouseMove, toRuler(target), Qt::NoButton, Qt::LeftButton);
        sendMouse(left, QEvent::MouseButtonRelease, toRuler(target), Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();
        QCOMPARE(report->verticalGuides(), QVariantList{70.0});

        // Saved in the report file, undoable, and not a printed object
        const QByteArray json = report->toJson();
        QVERIFY(json.contains("verticalGuides"));
        f.widget.undo();
        QVERIFY(report->verticalGuides().isEmpty());
        f.widget.redo();
        QCOMPARE(report->verticalGuides(), QVariantList{70.0});
        BrReport loaded;
        QVERIFY(BrReportFile::fromJson(&loaded, json));
        QCOMPARE(loaded.verticalGuides(), QVariantList{70.0});

        // Moved with the mouse
        f.drag(QPointF(70, 40), QPointF(10, 0));
        QCOMPARE(report->verticalGuides(), QVariantList{80.0});

        // Dragged out of the view: removed
        const QPoint start = f.view->mapFromScene(QPointF(80, 40));
        sendMouse(viewport, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        sendMouse(viewport, QEvent::MouseMove, QPoint(-5, start.y()), Qt::NoButton, Qt::LeftButton);
        sendMouse(viewport, QEvent::MouseButtonRelease, QPoint(-5, start.y()), Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();
        QVERIFY(report->verticalGuides().isEmpty());
        int guideItems = 0;
        for (QGraphicsItem *item : f.scene->items())
            guideItems += item->type() == BrGuideItem::Type;
        QCOMPARE(guideItems, 0);

        // A guide released over the ruler it came from is not created
        sendMouse(left, QEvent::MouseButtonPress, QPoint(5, 50), Qt::LeftButton, Qt::LeftButton);
        sendMouse(left, QEvent::MouseButtonRelease, QPoint(5, 60), Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();
        QVERIFY(report->verticalGuides().isEmpty());
        QVERIFY(report->horizontalGuides().isEmpty());

        report->setHorizontalGuides({12.0, 34.0});
        f.scene->rebuild();
        action(&f.widget, QStringLiteral("Clear guides"))->trigger();
        QCoreApplication::processEvents();
        QVERIFY(report->horizontalGuides().isEmpty());
    }

    void rulers()
    {
        QCOMPARE(BrRuler::labelStep(10.0), 5);
        QCOMPARE(BrRuler::labelStep(3.8), 20);
        QCOMPARE(BrRuler::labelStep(0.9), 50);
        QCOMPARE(BrRuler::labelStep(0.01), 1000);

        LayoutFixture f;
        const int margin = f.view->viewport()->geometry().left() - f.view->frameWidth();
        QCOMPARE(margin, int(BrDesignView::RulerSize));
        action(&f.widget, QStringLiteral("Rulers"))->setChecked(false);
        QCOMPARE(f.view->viewport()->geometry().left() - f.view->frameWidth(), 0);
        const QList<BrRuler *> rulers = f.view->findChildren<BrRuler *>();
        QVERIFY(!rulers.first()->isVisible());
        action(&f.widget, QStringLiteral("Rulers"))->setChecked(true);
        QVERIFY(rulers.first()->isVisible());
        QVERIFY(!f.view->grab().isNull());
    }

    void newReportLayout()
    {
        BrDesignerWidget w;
        QCOMPARE(w.report()->bands().size(), 3);
        QVERIFY(!w.isModified());
        auto *scene = w.findChild<BrDesignScene *>();
        QVERIFY(scene);
        QVERIFY(scene->items().size() > 3); // page, margins, bands, objects
    }

    void insertUndoRedo()
    {
        BrDesignerWidget w;
        auto *scene = w.findChild<BrDesignScene *>();
        BrBand *detail = detailBand(w.report());
        QVERIFY(detail);
        const QJsonObject json{{QStringLiteral("type"), QStringLiteral("DBText")},
                               {QStringLiteral("dataField"), QStringLiteral("name")},
                               {QStringLiteral("geometry"), QJsonArray{0, 0, 30, 6}}};
        emit scene->insertRequested(detail, QPointF(12, 1), json);
        QCoreApplication::processEvents();
        detail = detailBand(w.report());
        QCOMPARE(detail->controls().size(), 1);
        auto *text = qobject_cast<BrDBText *>(detail->controls().first());
        QVERIFY(text);
        QCOMPARE(text->geometry(), QRectF(12, 1, 30, 6));
        QCOMPARE(text->dataField(), QStringLiteral("name"));
        QVERIFY(w.isModified());
        QCOMPARE(scene->selectedObjects(), QList<QObject *>{text});

        w.undo();
        QCOMPARE(detailBand(w.report())->controls().size(), 0);
        QVERIFY(!w.isModified());
        w.redo();
        QCOMPARE(detailBand(w.report())->controls().size(), 1);
    }

    void moveIsUndoable()
    {
        BrDesignerWidget w;
        auto *scene = w.findChild<BrDesignScene *>();
        BrControl *title = w.report()->bands().first()->controls().first();
        const QRectF before = title->geometry();
        title->setGeometry(before.translated(5, 0));
        emit scene->layoutEdited();
        QCoreApplication::processEvents();
        QVERIFY(w.isModified());
        w.undo();
        QCOMPARE(w.report()->bands().first()->controls().first()->geometry(), before);
    }

    void copyPasteDelete()
    {
        BrDesignerWidget w;
        auto *scene = w.findChild<BrDesignScene *>();
        BrControl *title = w.report()->bands().first()->controls().first();
        scene->selectObjects({title});
        action(&w, QStringLiteral("Copy"))->trigger();
        action(&w, QStringLiteral("Paste"))->trigger();
        QCoreApplication::processEvents();
        BrBand *header = w.report()->bands().first();
        QCOMPARE(header->controls().size(), 2);
        QCOMPARE(header->controls().at(1)->geometry(), title->geometry().translated(2, 2));
        scene->selectObjects({header->controls().at(1)});
        action(&w, QStringLiteral("Delete"))->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(w.report()->bands().first()->controls().size(), 1);
        w.undo();
        QCOMPARE(w.report()->bands().first()->controls().size(), 2);
    }

    void bandsAndOrder()
    {
        BrDesignerWidget w;
        auto *scene = w.findChild<BrDesignScene *>();
        scene->selectObjects({detailBand(w.report())});
        const int before = int(w.report()->bands().size());
        action(&w, QStringLiteral("Summary"))->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(w.report()->bands().size(), before + 1);
        QCOMPARE(w.report()->bands().last()->bandType(), Br::BandType::Summary);
        // Move the new band up one position
        action(&w, QStringLiteral("Move up / backward"))->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(w.report()->bands().at(before - 1)->bandType(), Br::BandType::Summary);
    }

    void saveHandlerAndJson()
    {
        BrDesignerWidget w;
        QByteArray stored;
        w.setSaveHandler([&stored](const QByteArray &json) {
            stored = json;
            return true;
        });
        w.report()->setTitle(QStringLiteral("Stored in a database"));
        emit w.findChild<BrDesignScene *>()->layoutEdited();
        QCoreApplication::processEvents();
        QVERIFY(w.isModified());
        QVERIFY(w.save());
        QVERIFY(!w.isModified());
        QVERIFY(stored.contains("Stored in a database"));

        BrDesignerWidget other;
        QVERIFY(other.loadFromJson(stored));
        QCOMPARE(other.report()->title(), QStringLiteral("Stored in a database"));
        QString error;
        QVERIFY(!other.loadFromJson("{ broken", &error));
        QVERIFY(!error.isEmpty());
    }

    void designRendering()
    {
        // Data-aware objects show placeholders instead of data
        BrReport report;
        auto *band = report.addBand(Br::BandType::Detail, 10);
        auto *field = new BrDBText(QStringLiteral("customer"), band);
        auto *sum = new BrDBResult(QStringLiteral("total"), Br::ResultType::Sum, band);
        auto *page = new BrSystemInfo(Br::InfoType::PagePreview, band);
        page->setFormat(QStringLiteral("Page %1 of %2"));
        auto texts = [](BrControl *c) {
            QStringList list;
            for (const BrPageItem &it : BrDesignContext::renderControl(c, QSizeF(40, 6)))
                list << it.text;
            return list.join(QLatin1Char('|'));
        };
        QCOMPARE(texts(field), QStringLiteral("[customer]"));
        QCOMPARE(texts(sum), QStringLiteral("Sum(total)"));
        QCOMPARE(texts(page), QStringLiteral("Page # of ##"));
    }

    void portugueseTranslation()
    {
        QVERIFY(BrTranslations::availableLanguages().contains(QStringLiteral("pt_BR")));
        QVERIFY(BrTranslations::install(QLocale(QLocale::Portuguese, QLocale::Brazil)));
        {
            BrDesignerWidget w;
            QStringList menus;
            for (QAction *a : w.menuBar()->actions())
                menus << a->text().remove(QLatin1Char('&'));
            QVERIFY2(menus.contains(QStringLiteral("Arquivo")), qPrintable(menus.join(QLatin1Char(','))));
            QVERIFY(menus.contains(QStringLiteral("Relatório")));
            QCOMPARE(BrPropertyEditor::labelFor(QStringLiteral("dataField")), QStringLiteral("Campo"));
            QCOMPARE(BrPropertyEditor::labelFor(QStringLiteral("watermarkText")), QStringLiteral("Texto da marca d'água"));
            QCOMPARE(BrPropertyEditor::labelFor(QStringLiteral("PagePreview")), QStringLiteral("Página X de Y"));
            BrPreviewDialog preview;
            QCOMPARE(BrExpressionEditor::functionCatalog().first().signature, QStringLiteral("upper(texto)"));
            QCOMPARE(preview.windowTitle(), QStringLiteral("Visualização de impressão"));
        }
        BrTranslations::uninstall();
        QCOMPARE(BrPropertyEditor::labelFor(QStringLiteral("dataField")), QStringLiteral("Field"));
        QVERIFY(!BrTranslations::install(QLocale(QLocale::Japanese)));
        BrTranslations::uninstall();
    }
};

QTEST_MAIN(TstDesigner)
#include "tst_designer.moc"
