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
#include "brreportcore.h"

#include <QAction>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

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
};

QTEST_MAIN(TstDesigner)
#include "tst_designer.moc"
