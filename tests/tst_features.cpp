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

#include "brreportcore.h"

#include <QBuffer>
#include <QTest>

namespace {

QList<QVariantMap> rows(int count, const QString &group = QString(), int groupSize = 0)
{
    QList<QVariantMap> list;
    for (int i = 1; i <= count; ++i) {
        QVariantMap row{{QStringLiteral("id"), i}, {QStringLiteral("name"), QStringLiteral("Item %1").arg(i)}};
        if (groupSize > 0)
            row.insert(group, QStringLiteral("G%1").arg((i - 1) / groupSize + 1));
        list << row;
    }
    return list;
}

const BrPageItem *findText(const BrPage &page, const QString &text)
{
    for (const BrPageItem &it : page.items)
        if ((it.kind == BrPageItem::Text || it.kind == BrPageItem::RichText) && it.text == text)
            return &it;
    return nullptr;
}

int pageOf(const BrDocument &doc, const QString &text)
{
    for (int p = 0; p < doc.pageCount(); ++p)
        if (findText(doc.pages.at(p), text))
            return p;
    return -1;
}

} // namespace

class TstFeatures : public QObject
{
    Q_OBJECT
private slots:
    void continuousPage()
    {
        BrListDataSource ds(rows(50));
        BrReport report;
        report.setDataSource(&ds);
        report.setPageSizeMm(80, 200); // receipt printer roll
        report.setMargins(5, 5, 5, 5);
        report.setContinuous(true);
        new BrLabel(QStringLiteral("HEADER"), report.addBand(Br::BandType::Header, 10));
        new BrDBText(QStringLiteral("name"), report.addBand(Br::BandType::Detail, 10));
        new BrLabel(QStringLiteral("FOOTER"), report.addBand(Br::BandType::Footer, 8));
        report.bands(Br::BandType::Detail).first()->setPageBreak(Br::PageBreak::After);
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageCount(), 1);
        const BrPage &page = report.document().pages.first();
        QCOMPARE(page.size.width(), 80.0);
        QCOMPARE(page.size.height(), 5 + 10 + 500 + 8 + 5.0);
        QCOMPARE(findText(page, QStringLiteral("FOOTER"))->rect.top(), 515.0);
    }

    void watermark()
    {
        BrListDataSource ds(rows(60));
        BrReport report;
        report.setDataSource(&ds);
        report.setWatermark(QStringLiteral("DRAFT"));
        new BrDBText(QStringLiteral("name"), report.addBand(Br::BandType::Detail, 10));
        QVERIFY(report.prepare());
        QVERIFY(report.document().pageCount() >= 2);
        for (const BrPage &page : report.document().pages) {
            const BrPageItem &first = page.items.first(); // behind everything else
            QCOMPARE(first.text, QStringLiteral("DRAFT"));
            QCOMPARE(first.angle, 45.0);
        }
    }

    void zebraRestartsPerGroup()
    {
        BrListDataSource ds(rows(6, QStringLiteral("g"), 3));
        BrReport report;
        report.setDataSource(&ds);
        auto *group = new BrGroup(&report);
        group->setDataFields(QStringLiteral("g"));
        auto *detail = group->addBand(Br::BandType::Detail, 6);
        detail->setAlternateBackground(QColor(10, 20, 30));
        new BrDBText(QStringLiteral("name"), detail);
        QVERIFY(report.prepare());
        QList<double> stripes;
        for (const BrPageItem &it : report.document().pages.first().items)
            if (it.kind == BrPageItem::Rect && it.brushColor == QColor(10, 20, 30))
                stripes << it.rect.top();
        // rows 2 of each group (3 rows per group): y = 10 + 6 and 10 + 18 + 6
        QCOMPARE(stripes, QList<double>({16.0, 34.0}));
    }

    void firstAndLastPageBands()
    {
        BrListDataSource ds(rows(60));
        BrReport report;
        report.setDataSource(&ds);
        auto *header = report.addBand(Br::BandType::Header, 10);
        header->setPrintOnFirstPage(false);
        new BrLabel(QStringLiteral("HEADER"), header);
        new BrDBText(QStringLiteral("name"), report.addBand(Br::BandType::Detail, 10));
        auto *footer = report.addBand(Br::BandType::Footer, 10);
        footer->setPrintOnLastPage(false);
        new BrLabel(QStringLiteral("FOOTER"), footer);
        QVERIFY(report.prepare());
        const BrDocument &doc = report.document();
        const int last = doc.pageCount() - 1;
        QVERIFY(last >= 1);
        QVERIFY(!findText(doc.pages.at(0), QStringLiteral("HEADER")));
        QVERIFY(findText(doc.pages.at(1), QStringLiteral("HEADER")));
        QVERIFY(findText(doc.pages.at(0), QStringLiteral("FOOTER")));
        QVERIFY(!findText(doc.pages.at(last), QStringLiteral("FOOTER")));
    }

    void keepHeaderWithDetail()
    {
        // 26 rows of 10 mm fill 260 of the 277 mm; a group header (10) + its first
        // detail (10) would not fit, so the second group must start on page 2.
        QList<QVariantMap> data = rows(26);
        for (QVariantMap &r : data)
            r.insert(QStringLiteral("g"), QStringLiteral("A"));
        data << QVariantMap{{QStringLiteral("g"), QStringLiteral("B")}, {QStringLiteral("name"), QStringLiteral("B1")}};
        for (bool keep : {false, true}) {
            BrListDataSource ds(data);
            BrReport report;
            report.setDataSource(&ds);
            auto *group = new BrGroup(&report);
            group->setDataFields(QStringLiteral("g"));
            group->setKeepHeaderWithDetail(keep);
            auto *gh = group->addBand(Br::BandType::Title, 7);
            (new BrDBText(gh))->setDataFormula(QStringLiteral("'Group ' + g"));
            new BrDBText(QStringLiteral("name"), group->addBand(Br::BandType::Detail, 10));
            QVERIFY(report.prepare());
            const BrDocument &doc = report.document();
            QCOMPARE(pageOf(doc, QStringLiteral("B1")), 1);
            QCOMPARE(pageOf(doc, QStringLiteral("Group B")), keep ? 1 : 0); // orphan without the option
        }
    }

    void columns()
    {
        BrListDataSource ds(rows(7));
        BrReport report;
        report.setDataSource(&ds);
        auto *detail = report.addBand(Br::BandType::Detail, 10);
        detail->setColumns(3, 5);
        auto *t = new BrDBText(QStringLiteral("name"), detail);
        t->setAlign(Br::Align::Client);
        new BrLabel(QStringLiteral("END"), report.addBand(Br::BandType::Summary, 5));
        QVERIFY(report.prepare());
        const BrPage &page = report.document().pages.first();
        // 190 mm wide, 3 columns, 5 mm spacing: 60 mm per column
        QCOMPARE(findText(page, QStringLiteral("Item 1"))->rect, QRectF(10, 10, 60, 10));
        QCOMPARE(findText(page, QStringLiteral("Item 2"))->rect, QRectF(75, 10, 60, 10));
        QCOMPARE(findText(page, QStringLiteral("Item 3"))->rect, QRectF(140, 10, 60, 10));
        QCOMPARE(findText(page, QStringLiteral("Item 4"))->rect, QRectF(10, 20, 60, 10));
        QCOMPARE(findText(page, QStringLiteral("Item 7"))->rect, QRectF(10, 30, 60, 10));
        QCOMPARE(findText(page, QStringLiteral("END"))->rect.top(), 40.0);
    }

    void shiftAndStretchToBand()
    {
        BrReport report;
        auto *band = report.addBand(Br::BandType::Detail, 12);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 30, 5);
        memo->setText(QStringLiteral("one two three four five six seven eight nine ten eleven twelve"));
        auto *below = new BrLabel(QStringLiteral("below"), band);
        below->setGeometry(10, 6, 30, 5);
        auto *aside = new BrLabel(QStringLiteral("aside"), band);
        aside->setGeometry(100, 6, 30, 5);
        auto *line = new BrDraw(Br::DrawKind::VerticalLine, band);
        line->setGeometry(180, 0, 2, 12);
        line->setStretchToBand(true);
        QVERIFY(report.prepare());
        const BrPage &page = report.document().pages.first();
        double memoBottom = 0;
        for (const BrPageItem &it : page.items)
            if (it.kind == BrPageItem::Text && it.text != QStringLiteral("below")
                && it.text != QStringLiteral("aside"))
                memoBottom = std::max(memoBottom, it.rect.bottom());
        QVERIFY(memoBottom > 10 + 5);
        QVERIFY(findText(page, QStringLiteral("below"))->rect.top() >= memoBottom - 0.01);
        QCOMPARE(findText(page, QStringLiteral("aside"))->rect.top(), 16.0); // not pushed
        double lineBottom = 0;
        for (const BrPageItem &it : page.items)
            if (it.kind == BrPageItem::Line)
                lineBottom = it.points.at(1).y();
        QVERIFY(lineBottom > 22.0); // grew with the band (design: 10 + 12)
    }

    void twoDimensionalCodesInAReport()
    {
        BrReport report;
        BrBand *band = report.addBand(Br::BandType::Detail, 40);
        auto *matrix = new BrBarcode(Br::BarcodeType::DataMatrix, QStringLiteral("123456"), band);
        matrix->setGeometry(0, 0, 30, 30);
        auto *stacked = new BrBarcode(Br::BarcodeType::Pdf417, QStringLiteral("document"), band);
        stacked->setGeometry(40, 0, 60, 30);
        auto *bars = new BrBarcode(Br::BarcodeType::Code39, QStringLiteral("CODE39"), band);
        bars->setGeometry(110, 0, 60, 30);
        QVERIFY(report.prepare());

        const QRectF content = report.contentRect();
        int inMatrix = 0, inStacked = 0;
        for (const BrPageItem &item : report.document().pages.first().items) {
            if (item.kind != BrPageItem::Rect)
                continue;
            const QRectF r = item.rect.translated(-content.topLeft());
            if (r.right() <= 30.01) {
                ++inMatrix;
                QVERIFY(r.bottom() <= 30.01);
            } else if (r.left() >= 39.99 && r.right() <= 100.01) {
                ++inStacked;
            }
        }
        QVERIFY(inMatrix > 10);
        QVERIFY(inStacked > 100);
        QVERIFY(report.document().pageText(0).contains(QStringLiteral("CODE39")));
    }

    void hideIfEmptyAndShrink()
    {
        BrReport report;
        report.setVariable(QStringLiteral("empty"), QString());
        auto *band = report.addBand(Br::BandType::Detail, 20);
        auto *hidden = new BrDBText(band);
        hidden->setDataFormula(QStringLiteral("empty"));
        hidden->setBorders(BrBorders::all());
        hidden->setHideIfEmpty(true);
        auto *shrink = new BrLabel(QStringLiteral("A rather long text that does not fit"), band);
        shrink->setGeometry(0, 10, 25, 5);
        shrink->setFont(QFont(QStringLiteral("Arial"), 12));
        shrink->setShrinkToFit(true);
        QVERIFY(report.prepare());
        const BrPage &page = report.document().pages.first();
        for (const BrPageItem &it : page.items)
            QVERIFY(it.kind != BrPageItem::Rect);
        const BrPageItem *t = findText(page, shrink->text());
        QVERIFY(t);
        QVERIFY(t->font.pointSizeF() < 12.0);
        QVERIFY(BrTextMetrics::width(t->font, t->text) <= 25.01);
    }

    void richText()
    {
        BrReport report;
        auto *band = report.addBand(Br::BandType::Detail, 5);
        auto *html = new BrLabel(band);
        html->setGeometry(0, 0, 40, 5);
        html->setAllowHtml(true);
        html->setAutoStretch(true);
        html->setText(QStringLiteral("<b>Bold</b> and <i>italic</i> and <font color=red>red</font> "
                                     "text that wraps over several lines in a narrow box."));
        QVERIFY(report.prepare());
        const BrDocument &doc = report.document();
        const BrPageItem *it = findText(doc.pages.first(), html->text());
        QVERIFY(it);
        QCOMPARE(it->kind, BrPageItem::RichText);
        QVERIFY(it->rect.height() > 10.0);

        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QVERIFY(doc.save(&buffer));
        buffer.seek(0);
        BrDocument loaded;
        QVERIFY(loaded.load(&buffer));
        QCOMPARE(findText(loaded.pages.first(), html->text())->kind, BrPageItem::RichText);
        QVERIFY(BrExport::toSvg(doc.pages.first()).contains(QStringLiteral("<foreignObject")));
        QVERIFY(!BrRenderer::toImage(doc.pages.first(), 72).isNull()); // paints without crashing
    }

    void charts()
    {
        BrListDataSource sales({
            {{QStringLiteral("month"), QStringLiteral("Jan")}, {QStringLiteral("a"), 10}, {QStringLiteral("b"), 5}},
            {{QStringLiteral("month"), QStringLiteral("Feb")}, {QStringLiteral("a"), 20}, {QStringLiteral("b"), 0}},
            {{QStringLiteral("month"), QStringLiteral("Mar")}, {QStringLiteral("a"), 30}, {QStringLiteral("b"), 15}},
        });
        auto count = [](const BrPage &page, BrPageItem::Kind kind, const QColor &brush) {
            int n = 0;
            for (const BrPageItem &it : page.items)
                if (it.kind == kind && (!brush.isValid() || it.brushColor == brush))
                    ++n;
            return n;
        };

        BrReport report;
        auto *band = report.addBand(Br::BandType::Detail, 80);
        auto *pie = new BrChart(Br::ChartType::Pie, band);
        pie->setGeometry(0, 0, 90, 80);
        pie->setLabels({QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z"), QStringLiteral("none")});
        pie->addSeries(QStringLiteral("s"), {1, 2, 3, 0});
        pie->setShowValues(true);
        auto *bars = new BrChart(Br::ChartType::Bar, band);
        bars->setGeometry(95, 0, 95, 80);
        bars->setDataSource(&sales, QStringLiteral("month"), {QStringLiteral("a"), QStringLiteral("b")});
        bars->setPalette({QColor(1, 1, 1), QColor(2, 2, 2)});
        QVERIFY(report.prepare());
        const BrPage &page = report.document().pages.first();
        QCOMPARE(count(page, BrPageItem::Polygon, QColor()), 3); // zero slice skipped
        QVERIFY(findText(page, QStringLiteral("50%")));          // 3 of 6
        QCOMPARE(count(page, BrPageItem::Rect, QColor(1, 1, 1)), 3 + 1); // 3 bars + legend swatch
        QCOMPARE(count(page, BrPageItem::Rect, QColor(2, 2, 2)), 3 + 1);
        QVERIFY(findText(page, QStringLiteral("Feb")));
    }

    void pageNumbersPerGroup()
    {
        // Group A: 40 rows (2 pages), group B: 10 rows (1 page), group C: 60 rows (3 pages)
        QList<QVariantMap> data;
        const QList<QPair<QString, int>> sizes = {{QStringLiteral("A"), 40}, {QStringLiteral("B"), 10},
                                                  {QStringLiteral("C"), 60}};
        for (const auto &g : sizes)
            for (int i = 1; i <= g.second; ++i)
                data << QVariantMap{{QStringLiteral("g"), g.first},
                                    {QStringLiteral("name"), QStringLiteral("%1-%2").arg(g.first).arg(i)}};
        BrListDataSource ds(data);
        BrReport report;
        report.setDataSource(&ds);
        // Page header printed when the page starts (before the group resets the numbering)
        auto *header = report.addBand(Br::BandType::Header, 10);
        auto *pageInHeader = new BrSystemInfo(Br::InfoType::PagePreview, header);
        pageInHeader->setFormat(QStringLiteral("H %1/%2"));
        auto *group = new BrGroup(&report);
        group->setDataFields(QStringLiteral("g"));
        group->setPageBreak(Br::PageBreak::Before);
        group->setResetPageNumber(true);
        new BrDBText(QStringLiteral("name"), group->addBand(Br::BandType::Detail, 10));
        auto *footer = report.addBand(Br::BandType::Footer, 10);
        auto *page = new BrSystemInfo(Br::InfoType::PagePreview, footer);
        page->setFormat(QStringLiteral("Page %1 of %2"));
        auto *sheet = new BrDBText(footer);
        sheet->setGeometry(100, 0, 60, 10);
        sheet->setDataFormula(QStringLiteral("'Sheet ' + documentpage() + ' of ' + documentpages()"));

        QVERIFY(report.prepare());
        const BrDocument &doc = report.document();
        QCOMPARE(doc.pageCount(), 6); // 25 rows per page: 2 + 1 + 3
        const QStringList expected = {QStringLiteral("1 of 2"), QStringLiteral("2 of 2"),
                                      QStringLiteral("1 of 1"), QStringLiteral("1 of 3"),
                                      QStringLiteral("2 of 3"), QStringLiteral("3 of 3")};
        for (int p = 0; p < 6; ++p) {
            const QString text = doc.pageText(p);
            QVERIFY2(text.contains(QStringLiteral("Page ") + expected.at(p)), qPrintable(text));
            QVERIFY2(text.contains(QStringLiteral("H ") + QString(expected.at(p)).replace(QStringLiteral(" of "), QStringLiteral("/"))),
                     qPrintable(text));
            QVERIFY2(text.contains(QStringLiteral("Sheet %1 of 6").arg(p + 1)), qPrintable(text));
            QVERIFY(!text.contains(QChar(0x0001)));
        }
        QVERIFY(doc.pageText(2).contains(QStringLiteral("B-1")));
    }
};

QTEST_MAIN(TstFeatures)
#include "tst_features.moc"
