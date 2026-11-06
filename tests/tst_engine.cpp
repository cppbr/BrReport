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
#include <QImage>
#include <QTest>

namespace {

QList<QVariantMap> numberedRows(int count)
{
    QList<QVariantMap> rows;
    for (int i = 1; i <= count; ++i)
        rows << QVariantMap{{QStringLiteral("id"), i},
                            {QStringLiteral("name"), QStringLiteral("Item %1").arg(i)},
                            {QStringLiteral("value"), 10.0}};
    return rows;
}

QList<QVariantMap> salesRows()
{
    // Sorted by region, then city.
    const struct { const char *region; const char *city; double amount; } data[] = {
        {"North", "Belem", 100}, {"North", "Belem", 50}, {"North", "Manaus", 25},
        {"South", "Curitiba", 10}, {"South", "Porto Alegre", 20}, {"South", "Porto Alegre", 30},
        {"West", "Goiania", 7},
    };
    QList<QVariantMap> rows;
    for (const auto &d : data)
        rows << QVariantMap{{QStringLiteral("region"), QString::fromLatin1(d.region)},
                            {QStringLiteral("city"), QString::fromLatin1(d.city)},
                            {QStringLiteral("amount"), d.amount}};
    return rows;
}

QString numberedLines(int count)
{
    QStringList lines;
    for (int i = 0; i < count; ++i)
        lines << QStringLiteral("line %1").arg(i);
    return lines.join(QLatin1Char('\n'));
}

bool isBlank(const QImage &image)
{
    const QRgb background = image.pixel(0, 0);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixel(x, y) != background)
                return false;
    return true;
}

QStringList allText(const BrDocument &doc)
{
    QStringList lines;
    for (int i = 0; i < doc.pageCount(); ++i)
        lines << doc.pageText(i).split(QLatin1Char('\n'));
    return lines;
}

} // namespace

class TstEngine : public QObject
{
    Q_OBJECT
private slots:
    void pagination()
    {
        BrListDataSource ds(numberedRows(100));
        BrReport report;
        report.setLocale(QLocale(QStringLiteral("en_US")));
        report.setDataSource(&ds);
        report.setMargins(10, 10, 10, 10); // A4: 277 mm of content height

        auto *header = report.addBand(Br::BandType::Header, 10);
        new BrLabel(QStringLiteral("HEADER"), header);
        auto *detail = report.addBand(Br::BandType::Detail, 10);
        new BrDBText(QStringLiteral("name"), detail);
        auto *footer = report.addBand(Br::BandType::Footer, 10);
        auto *info = new BrSystemInfo(Br::InfoType::PagePreview, footer);
        info->setFormat(QStringLiteral("Page %1 of %2"));
        auto *pageTotal = new BrDBResult(QStringLiteral("value"), Br::ResultType::Sum, footer);
        pageTotal->setGeometry(100, 0, 40, 10);
        pageTotal->setResetAfterPrint(true);
        pageTotal->setDisplayFormat(QStringLiteral("0.00"));

        QVERIFY2(report.prepare(), qPrintable(report.errorString()));
        const BrDocument &doc = report.document();
        // (277 - 10 header - 10 footer) / 10 = 25 rows per page
        QCOMPARE(doc.pageCount(), 4);
        for (int p = 0; p < 4; ++p) {
            const QString text = doc.pageText(p);
            QVERIFY(text.startsWith(QStringLiteral("HEADER")));
            QVERIFY2(text.contains(QStringLiteral("Page %1 of 4").arg(p + 1)), qPrintable(text));
            QVERIFY2(text.contains(QStringLiteral("250.00")), qPrintable(text)); // per-page total
            QVERIFY(text.contains(QStringLiteral("Item %1").arg(p * 25 + 1)));
            QVERIFY(text.contains(QStringLiteral("Item %1").arg(p * 25 + 25)));
        }
    }

    void titleAndSummary()
    {
        BrListDataSource ds(numberedRows(3));
        BrReport report;
        report.setDataSource(&ds);
        new BrLabel(QStringLiteral("TITLE"), report.addBand(Br::BandType::Title, 10));
        new BrLabel(QStringLiteral("COLHEAD"), report.addBand(Br::BandType::ColumnHeader, 10));
        new BrDBText(QStringLiteral("name"), report.addBand(Br::BandType::Detail, 10));
        new BrLabel(QStringLiteral("COLFOOT"), report.addBand(Br::BandType::ColumnFooter, 10));
        auto *summary = report.addBand(Br::BandType::Summary, 10);
        new BrDBResult(QStringLiteral("id"), Br::ResultType::Count, summary);
        QVERIFY(report.prepare());
        QCOMPARE(allText(report.document()),
                 QStringList({QStringLiteral("TITLE"), QStringLiteral("COLHEAD"),
                              QStringLiteral("Item 1"), QStringLiteral("Item 2"),
                              QStringLiteral("Item 3"), QStringLiteral("COLFOOT"),
                              QStringLiteral("3")}));
    }

    void nestedGroups()
    {
        BrListDataSource ds(salesRows());
        BrReport report;
        report.setLocale(QLocale(QStringLiteral("en_US")));
        report.setDataSource(&ds);

        auto *region = new BrGroup(&report);
        region->setDataFields(QStringLiteral("region"));
        auto *rh = region->addBand(Br::BandType::Header, 8);
        auto *rhText = new BrDBText(rh);
        rhText->setDataFormula(QStringLiteral("'Region: ' + region"));
        auto *rs = region->addBand(Br::BandType::Summary, 8);
        auto *rsum = new BrDBResult(QStringLiteral("amount"), Br::ResultType::Sum, rs);
        rsum->setPrefix(QStringLiteral("Region total: "));

        auto *city = new BrGroup(region);
        city->setDataFields(QStringLiteral("city"));
        auto *ct = city->addBand(Br::BandType::Title, 8);
        new BrDBText(QStringLiteral("city"), ct);
        new BrDBText(QStringLiteral("amount"), city->addBand(Br::BandType::Detail, 8));
        auto *cs = city->addBand(Br::BandType::Summary, 8);
        auto *csum = new BrDBResult(QStringLiteral("amount"), Br::ResultType::Sum, cs);
        csum->setPrefix(QStringLiteral("City total: "));

        auto *summary = report.addBand(Br::BandType::Summary, 8);
        auto *grand = new BrDBResult(QStringLiteral("amount"), Br::ResultType::Sum, summary);
        grand->setPrefix(QStringLiteral("Grand total: "));

        QVERIFY(report.prepare());
        const QStringList expected = {
            QStringLiteral("Region: North"), QStringLiteral("Belem"), QStringLiteral("100"),
            QStringLiteral("50"), QStringLiteral("City total: 150"), QStringLiteral("Manaus"),
            QStringLiteral("25"), QStringLiteral("City total: 25"),
            QStringLiteral("Region total: 175"), QStringLiteral("Region: South"),
            QStringLiteral("Curitiba"), QStringLiteral("10"), QStringLiteral("City total: 10"),
            QStringLiteral("Porto Alegre"), QStringLiteral("20"), QStringLiteral("30"),
            QStringLiteral("City total: 50"), QStringLiteral("Region total: 60"),
            QStringLiteral("Region: West"), QStringLiteral("Goiania"), QStringLiteral("7"),
            QStringLiteral("City total: 7"), QStringLiteral("Region total: 7"),
            QStringLiteral("Grand total: 242")};
        QCOMPARE(allText(report.document()), expected);
    }

    void subDetail()
    {
        BrListDataSource orders({
            {{QStringLiteral("order_id"), 1}, {QStringLiteral("customer"), QStringLiteral("Ana")}},
            {{QStringLiteral("order_id"), 2}, {QStringLiteral("customer"), QStringLiteral("Bruno")}},
            {{QStringLiteral("order_id"), 3}, {QStringLiteral("customer"), QStringLiteral("Caio")}},
        });
        BrListDataSource items({
            {{QStringLiteral("order_id"), 1}, {QStringLiteral("product"), QStringLiteral("Pen")}},
            {{QStringLiteral("order_id"), 1}, {QStringLiteral("product"), QStringLiteral("Book")}},
            {{QStringLiteral("order_id"), 2}, {QStringLiteral("product"), QStringLiteral("Mug")}},
        });
        BrReport report;
        report.setDataSource(&orders);
        new BrDBText(QStringLiteral("customer"), report.addBand(Br::BandType::Detail, 8));

        auto *sub = new BrSubDetail(&report, &items);
        sub->setBeforeOpen([&items](BrSubDetail &, BrRenderContext &ctx) {
            const QVariant id = ctx.fieldValue(QStringLiteral("order_id"));
            items.setFilter([id](const QVariantMap &row) {
                return row.value(QStringLiteral("order_id")) == id;
            });
        });
        new BrLabel(QStringLiteral("Items:"), sub->addBand(Br::BandType::Header, 6));
        new BrDBText(QStringLiteral("product"), sub->addBand(Br::BandType::Detail, 6));
        auto *count = new BrDBResult(QStringLiteral("product"), Br::ResultType::Count,
                                     sub->addBand(Br::BandType::Summary, 6));
        count->setSuffix(QStringLiteral(" item(s)"));

        QVERIFY(report.prepare());
        QCOMPARE(allText(report.document()),
                 QStringList({QStringLiteral("Ana"), QStringLiteral("Items:"), QStringLiteral("Pen"),
                              QStringLiteral("Book"), QStringLiteral("2 item(s)"),
                              QStringLiteral("Bruno"), QStringLiteral("Items:"),
                              QStringLiteral("Mug"), QStringLiteral("1 item(s)"),
                              QStringLiteral("Caio")}));
    }

    void noDataSourcePrintsOnce()
    {
        BrReport report;
        new BrLabel(QStringLiteral("Hello"), report.addBand(Br::BandType::Detail, 10));
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageCount(), 1);
        QCOMPARE(report.document().pageText(0), QStringLiteral("Hello"));
    }

    void beforePrintAndPageBreak()
    {
        BrListDataSource ds(numberedRows(4));
        BrReport report;
        report.setDataSource(&ds);
        auto *detail = report.addBand(Br::BandType::Detail, 10);
        auto *text = new BrDBText(QStringLiteral("name"), detail);
        text->setBeforePrint([](BrControl &, bool &printIt) { printIt = true; });
        detail->setBeforePrint([](BrControl &band, bool &printIt) {
            // Skip odd records
            auto *b = static_cast<BrBand *>(&band);
            BrReport *r = b->report();
            Q_UNUSED(r)
            printIt = true;
        });
        detail->setPageBreak(Br::PageBreak::After);
        QVERIFY(report.prepare());
        // A break after each record, but no empty page at the end.
        QCOMPARE(report.document().pageCount(), 4);
    }

    void splitBandContinuesOnNextPage()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        new BrLabel(QStringLiteral("before"), report.addBand(Br::BandType::Detail, 60));
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        band->setAllowSplit(true);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setText(numberedLines(60));
        new BrLabel(QStringLiteral("after"), report.addBand(Br::BandType::Detail, 5));
        QVERIFY(report.prepare());

        const BrDocument &doc = report.document();
        QVERIFY(doc.pageCount() >= 2);
        QVERIFY(doc.pageText(0).contains(QStringLiteral("line 0")));

        // Every line once and in order, none out of the printable area
        QStringList printed;
        for (const QString &text : allText(doc))
            if (text.startsWith(QStringLiteral("line ")))
                printed << text;
        QCOMPARE(printed, numberedLines(60).split(QLatin1Char('\n')));
        const QRectF content = report.contentRect();
        for (const BrPage &page : doc.pages)
            for (const BrPageItem &item : page.items) {
                QVERIFY(item.rect.bottom() <= content.bottom() + 0.01);
                QVERIFY(item.rect.top() >= content.top() - 0.01);
            }

        // The part on the next page starts at the top, and the next band follows the text
        double lastBottom = 0, afterTop = 0;
        for (const BrPageItem &item : doc.pages.last().items) {
            if (item.text == QStringLiteral("after")) {
                afterTop = item.rect.top();
            } else {
                lastBottom = std::max(lastBottom, item.rect.bottom());
            }
        }
        QVERIFY(doc.pages.last().items.size() > 1);
        QVERIFY(afterTop >= lastBottom - 0.01);
    }

    void bandWithoutSplitMovesWhole()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        new BrLabel(QStringLiteral("before"), report.addBand(Br::BandType::Detail, 60));
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setText(numberedLines(15));
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageCount(), 2);
        QVERIFY(!report.document().pageText(0).contains(QStringLiteral("line 0")));
    }

    void bandTallerThanThePageIsAlwaysSplit()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setText(numberedLines(100));
        QVERIFY(report.prepare());
        const BrDocument &doc = report.document();
        QVERIFY(doc.pageCount() >= 2);
        QCOMPARE(allText(doc).filter(QStringLiteral("line ")).size(), 100);
        for (const BrPage &page : doc.pages)
            for (const BrPageItem &item : page.items)
                QVERIFY(item.rect.bottom() <= report.contentRect().bottom() + 0.01);
    }

    void splitLeavesRoomForTheFooter()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        new BrLabel(QStringLiteral("page footer"), report.addBand(Br::BandType::Footer, 10));
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        band->setAllowSplit(true);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setText(numberedLines(60));
        QVERIFY(report.prepare());
        const double limit = report.contentRect().bottom() - 10;
        for (const BrPage &page : report.document().pages)
            for (const BrPageItem &item : page.items)
                if (item.text != QStringLiteral("page footer"))
                    QVERIFY(item.rect.bottom() <= limit + 0.01);
    }

    void splitDoesNotCutLinesSideBySide()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        band->setAllowSplit(true);
        auto *left = new BrMemo(band);
        left->setGeometry(0, 0, 40, 5);
        left->setText(numberedLines(70));
        auto *right = new BrMemo(band);
        right->setGeometry(45, 0, 40, 5);
        QFont big = right->font();
        big.setPointSizeF(big.pointSizeF() * 1.7);
        right->setFont(big);
        right->setText(numberedLines(40));
        QVERIFY(report.prepare());

        QCOMPARE(allText(report.document()).filter(QStringLiteral("line ")).size(), 70 + 40);
        for (const BrPage &page : report.document().pages)
            for (const BrPageItem &item : page.items)
                QVERIFY(item.rect.bottom() <= report.contentRect().bottom() + 0.01);
    }

    void splitCutsFramesAndBackground()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        band->setAllowSplit(true);
        band->setBackground(Qt::yellow);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setText(numberedLines(100));
        QVERIFY(report.prepare());

        const BrDocument &doc = report.document();
        QVERIFY(doc.pageCount() >= 2);
        double total = 0;
        for (const BrPage &page : doc.pages) {
            int backgrounds = 0;
            for (const BrPageItem &item : page.items) {
                if (item.kind == BrPageItem::Rect && item.brushColor == QColor(Qt::yellow)) {
                    ++backgrounds;
                    total += item.rect.height();
                }
            }
            QCOMPARE(backgrounds, 1);
        }
        double lines = BrTextMetrics::lineHeight(memo->font()) * 100;
        QVERIFY(total >= lines - 0.01); // the whole text has a background
    }

    void splitKeepsImagesWhole()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        band->setAllowSplit(true);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 40, 5);
        memo->setText(numberedLines(70));
        QImage picture(10, 10, QImage::Format_RGB32);
        picture.fill(Qt::red);
        auto *image = new BrImage(band);
        image->setImage(picture);
        const double height = 30;
        image->setGeometry(50, 70, 30, height); // on the page break
        QVERIFY(report.prepare());

        int images = 0;
        for (const BrPage &page : report.document().pages)
            for (const BrPageItem &item : page.items)
                if (item.kind == BrPageItem::Image) {
                    ++images;
                    QVERIFY(item.rect.bottom() <= report.contentRect().bottom() + 0.01);
                }
        QCOMPARE(images, 1);
    }

    void splitRichText()
    {
        QString html;
        for (int i = 0; i < 80; ++i)
            html += QStringLiteral("<p>paragraph %1</p>").arg(i);

        BrReport report;
        report.setPageSizeMm(100, 100);
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        band->setAllowSplit(true);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setAllowHtml(true);
        memo->setText(html);
        QVERIFY(report.prepare());

        const BrDocument &doc = report.document();
        QVERIFY(doc.pageCount() >= 2);
        double offset = -1, shown = 0;
        for (const BrPage &page : doc.pages) {
            const auto items = page.items;
            for (const BrPageItem &item : items) {
                if (item.kind != BrPageItem::RichText)
                    continue;
                QVERIFY(item.rect.bottom() <= report.contentRect().bottom() + 0.01);
                QVERIFY(item.textOffset > offset); // each page shows the next part
                offset = item.textOffset;
                shown += item.rect.height();
            }
        }
        QVERIFY(qAbs(shown - BrTextMetrics::richTextHeight(memo->font(), html, 80)) < 0.01);

        // The second part is drawn, from the top of the page
        const QImage image = BrRenderer::toImage(doc.pages.at(1), 96);
        QVERIFY(!isBlank(image.copy(0, 0, image.width(), image.height() / 3)));

        // The offset is kept in prepared reports
        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QVERIFY(doc.save(&buffer));
        buffer.seek(0);
        BrDocument loaded;
        QVERIFY(loaded.load(&buffer));
        QCOMPARE(loaded.pages.at(1).items.size(), doc.pages.at(1).items.size());
        for (int i = 0; i < doc.pages.at(1).items.size(); ++i)
            QCOMPARE(loaded.pages.at(1).items.at(i).textOffset, doc.pages.at(1).items.at(i).textOffset);
    }

    void splitIsKeptInReportFiles()
    {
        BrReport report;
        report.addBand(Br::BandType::Detail, 5)->setAllowSplit(true);
        BrReport loaded;
        QVERIFY(loaded.loadFromJson(report.toJson()));
        QVERIFY(loaded.bands(Br::BandType::Detail).first()->allowSplit());
    }

    void splitStopsAtTheMaximumOfPages()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        report.setMaxPages(2);
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setText(numberedLines(1000));
        report.prepare();
        QCOMPARE(report.document().pageCount(), 2);
    }

    void splitDoesNotApplyToColumnsOrContinuousPages()
    {
        BrReport report;
        report.setPageSizeMm(100, 100);
        report.setContinuous(true);
        BrBand *band = report.addBand(Br::BandType::Detail, 5);
        band->setAllowSplit(true);
        auto *memo = new BrMemo(band);
        memo->setGeometry(0, 0, 80, 5);
        memo->setText(numberedLines(100));
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageCount(), 1);
        QCOMPARE(allText(report.document()).filter(QStringLiteral("line ")).size(), 100);
    }

    void hideIfEmptyCollapsesItsSpace()
    {
        QList<QVariantMap> rows = {{{QStringLiteral("line"), QString()}}};
        BrListDataSource source(rows);
        BrReport report;
        report.addDataSource(QStringLiteral("data"), &source);
        report.setDataSourceName(QStringLiteral("data"));
        BrBand *band = report.addBand(Br::BandType::Detail, 20);
        auto *top = new BrLabel(QStringLiteral("top"), band);
        top->setGeometry(0, 0, 40, 5);
        auto *empty = new BrDBText(QStringLiteral("line"), band);
        empty->setGeometry(0, 5, 40, 5);
        empty->setHideIfEmpty(true);
        auto *below = new BrLabel(QStringLiteral("below"), band);
        below->setGeometry(0, 10, 40, 5);
        auto *beside = new BrLabel(QStringLiteral("beside"), band);
        beside->setGeometry(50, 5, 40, 5); // nothing above it: it stays
        new BrLabel(QStringLiteral("next"), report.addBand(Br::BandType::Summary, 5));
        QVERIFY(report.prepare());

        const QRectF content = report.contentRect();
        QHash<QString, double> tops;
        for (const BrPageItem &item : report.document().pages.first().items)
            tops[item.text] = item.rect.top() - content.top();
        QCOMPARE(tops.value(QStringLiteral("below")), 5.0);
        QCOMPARE(tops.value(QStringLiteral("beside")), 5.0);
        QCOMPARE(tops.value(QStringLiteral("next")), 15.0); // 20 less the 5 that collapsed
    }

    void hideIfEmptyLeavesObjectsBesideIt()
    {
        QList<QVariantMap> rows = {{{QStringLiteral("line"), QString()}}};
        BrListDataSource source(rows);
        BrReport report;
        report.addDataSource(QStringLiteral("data"), &source);
        report.setDataSourceName(QStringLiteral("data"));
        BrBand *band = report.addBand(Br::BandType::Detail, 15);
        auto *empty = new BrDBText(QStringLiteral("line"), band);
        empty->setGeometry(0, 0, 40, 5);
        empty->setHideIfEmpty(true);
        auto *column = new BrLabel(QStringLiteral("column"), band);
        column->setGeometry(50, 0, 40, 5);
        auto *wide = new BrLabel(QStringLiteral("wide"), band);
        wide->setGeometry(0, 5, 90, 5); // under both: the column stops it
        QVERIFY(report.prepare());
        for (const BrPageItem &item : report.document().pages.first().items)
            if (item.text == QStringLiteral("wide"))
                QCOMPARE(item.rect.top() - report.contentRect().top(), 5.0);
    }

    void hideIfEmptyWithShiftOffKeepsTheLayout()
    {
        QList<QVariantMap> rows = {{{QStringLiteral("line"), QString()}}};
        BrListDataSource source(rows);
        BrReport report;
        report.addDataSource(QStringLiteral("data"), &source);
        report.setDataSourceName(QStringLiteral("data"));
        BrBand *band = report.addBand(Br::BandType::Detail, 15);
        band->setShiftOnStretch(false);
        auto *empty = new BrDBText(QStringLiteral("line"), band);
        empty->setGeometry(0, 0, 40, 5);
        empty->setHideIfEmpty(true);
        auto *below = new BrLabel(QStringLiteral("below"), band);
        below->setGeometry(0, 5, 40, 5);
        QVERIFY(report.prepare());
        for (const BrPageItem &item : report.document().pages.first().items)
            if (item.text == QStringLiteral("below"))
                QCOMPARE(item.rect.top() - report.contentRect().top(), 5.0);
    }

    void stretchingMemo()
    {
        BrReport report;
        auto *detail = report.addBand(Br::BandType::Detail, 5);
        auto *memo = new BrMemo(detail);
        memo->setGeometry(0, 0, 30, 5);
        memo->setText(QStringLiteral("one two three four five six seven eight nine ten eleven "
                                     "twelve thirteen fourteen fifteen"));
        new BrLabel(QStringLiteral("after"), report.addBand(Br::BandType::Detail, 5));
        QVERIFY(report.prepare());
        const BrPage &page = report.document().pages.first();
        double memoBottom = 0, afterTop = 0;
        for (const BrPageItem &it : page.items) {
            if (it.text == QStringLiteral("after"))
                afterTop = it.rect.top();
            else
                memoBottom = std::max(memoBottom, it.rect.bottom());
        }
        QVERIFY(memoBottom > 10 + 5); // more than one line
        QVERIFY(afterTop >= memoBottom - 0.01);
    }

    void expressionsAndVariables()
    {
        BrListDataSource ds(numberedRows(1));
        BrReport report;
        report.setLocale(QLocale(QStringLiteral("en_US")));
        report.setDataSource(&ds);
        report.setVariable(QStringLiteral("company"), QStringLiteral("ACME"));
        report.registerFunction(QStringLiteral("double"), [](const QVariantList &args) {
            return QVariant(args.value(0).toDouble() * 2);
        });
        auto *band = report.addBand(Br::BandType::Detail, 10);
        auto *t = new BrDBText(band);
        t->setDataFormula(QStringLiteral("company + ' ' + double(value) + ' p' + page()"));
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageText(0), QStringLiteral("ACME 20 p1"));
    }

    void maxPages()
    {
        BrListDataSource ds(numberedRows(50));
        BrReport report;
        report.setDataSource(&ds);
        report.setMaxPages(2);
        report.addBand(Br::BandType::Detail, 10)->setPageBreak(Br::PageBreak::After);
        QVERIFY(!report.prepare());
        QVERIFY(!report.errorString().isEmpty());
        QCOMPARE(report.document().pageCount(), 2);
    }
};

QTEST_MAIN(TstEngine)
#include "tst_engine.moc"
