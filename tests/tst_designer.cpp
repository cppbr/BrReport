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
#include "brparameterseditor_p.h"
#include "brpropertyeditor_p.h"
#include "brreportwidgets.h"
#include "brreportcore.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

namespace {

QAction *action(QWidget *w, const QString &text)
{
    for (QAction *a : w->findChildren<QAction *>())
        if (a->text().remove(QLatin1Char('&')) == text)
            return a;
    return nullptr;
}

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
        BrParametersEditor editor(parameters);
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
