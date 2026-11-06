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
#include <QTemporaryDir>
#include <QTest>

class TstDocument : public QObject
{
    Q_OBJECT
private:
    BrDocument makeDocument()
    {
        BrReport report;
        report.setTitle(QStringLiteral("Test"));
        auto *band = report.addBand(Br::BandType::Detail, 30);
        auto *label = new BrLabel(QStringLiteral("Hello <world> & \"friends\""), band);
        label->setGeometry(0, 0, 100, 8);
        label->setBorders(BrBorders::all());
        new BrDraw(Br::DrawKind::Ellipse, band);
        auto *bc = new BrBarcode(Br::BarcodeType::Code128, QStringLiteral("ABC123"), band);
        bc->setGeometry(0, 10, 50, 15);
        auto *img = new BrImage(band);
        QImage image(10, 10, QImage::Format_ARGB32);
        image.fill(Qt::red);
        img->setImage(image);
        img->setGeometry(60, 10, 10, 10);
        report.prepare();
        return report.document();
    }

private slots:
    void roundTrip()
    {
        const BrDocument doc = makeDocument();
        QVERIFY(doc.pageCount() == 1);
        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QVERIFY(doc.save(&buffer));
        buffer.seek(0);
        BrDocument loaded;
        QVERIFY(loaded.load(&buffer));
        QCOMPARE(loaded.title, doc.title);
        QCOMPARE(loaded.pageCount(), doc.pageCount());
        QCOMPARE(loaded.pages.first().items.size(), doc.pages.first().items.size());
        QCOMPARE(loaded.pageText(0), doc.pageText(0));
        QCOMPARE(loaded.pages.first().size, doc.pages.first().size);

        QBuffer garbage;
        garbage.setData("not a document");
        garbage.open(QIODevice::ReadOnly);
        QVERIFY(!loaded.load(&garbage));
        QCOMPARE(loaded.pageCount(), doc.pageCount()); // unchanged on failure
    }

    void exports()
    {
        const BrDocument doc = makeDocument();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pdf = dir.filePath(QStringLiteral("out.pdf"));
        QVERIFY(BrExport::toPdf(doc, pdf));
        QFile pdfFile(pdf);
        QVERIFY(pdfFile.open(QIODevice::ReadOnly));
        QVERIFY(pdfFile.read(5) == "%PDF-");

        const QStringList images = BrExport::toImages(doc, dir.filePath(QStringLiteral("p%1.png")), 50);
        QCOMPARE(images.size(), 1);
        const QImage img(images.first());
        QCOMPARE(img.width(), qCeil(210 * 50 / 25.4));

        const QString svg = BrExport::toSvg(doc.pages.first());
        QVERIFY(svg.contains(QStringLiteral("Hello &lt;world&gt; &amp; &quot;friends&quot;")));
        QVERIFY(svg.contains(QStringLiteral("<ellipse")));
        QVERIFY(svg.contains(QStringLiteral("data:image/png;base64,")));
        QVERIFY(BrExport::toHtml(doc, dir.filePath(QStringLiteral("out.html"))));
    }

    void plainText()
    {
        BrDocument doc;
        BrPage first;
        first.size = QSizeF(100, 50);
        QFont font;
        first.items << BrPageItem::makeText(QRectF(25.4, 4.3, 20, 4), QStringLiteral("name"), font, Qt::black,
                                            Qt::AlignLeft | Qt::AlignVCenter);
        first.items << BrPageItem::makeText(QRectF(50.8, 4.3, 25.4, 4), QStringLiteral("9.50"), font, Qt::black,
                                            Qt::AlignRight | Qt::AlignVCenter);
        first.items << BrPageItem::makeText(QRectF(0, 0, 100, 50), QStringLiteral("COPY"), font, Qt::gray,
                                            Qt::AlignCenter, 45.0); // a watermark is not text of the report
        BrPage second = first;
        second.items.clear();
        second.items << BrPageItem::makeText(QRectF(0, 0, 20, 4), QStringLiteral("end"), font, Qt::black,
                                             Qt::AlignLeft | Qt::AlignVCenter);
        doc.pages << first << second;

        const QString text = BrExport::toText(doc);
        QCOMPARE(text, QStringLiteral("\n          name            9.50\n\fend\n"));
    }

    void plainTextOfRichText()
    {
        BrDocument doc;
        BrPage page;
        page.size = QSizeF(200, 100);
        BrPageItem item = BrPageItem::makeText(QRectF(0, 0, 100, 40), QStringLiteral("<p>one <b>two</b></p><p>three</p>"),
                                               QFont(), Qt::black, Qt::AlignTop);
        item.kind = BrPageItem::RichText;
        page.items << item;
        doc.pages << page;
        const QString text = BrExport::toText(doc);
        QVERIFY(text.contains(QStringLiteral("one two")));
        QVERIFY(text.contains(QStringLiteral("three")));

        // Only the part on this page of a split text
        const double lineHeight = BrTextMetrics::richTextHeight(QFont(), QStringLiteral("x"), 100);
        doc.pages.first().items.first().textOffset = lineHeight * 0.9;
        QVERIFY(!BrExport::toText(doc).contains(QStringLiteral("one two")));
        QVERIFY(BrExport::toText(doc).contains(QStringLiteral("three")));
    }

    void xlsx()
    {
        BrDocument doc;
        BrPage page;
        page.size = QSizeF(210, 297);
        QFont bold;
        bold.setBold(true);
        const auto text = [&page](const QFont &font, double x, double y, const QString &value, int align) {
            page.items << BrPageItem::makeText(QRectF(x, y, 30, 5), value, font, Qt::black, align);
        };
        text(bold, 10, 10, QStringLiteral("Name"), Qt::AlignLeft);
        text(bold, 50, 10, QStringLiteral("Total"), Qt::AlignRight);
        text(QFont(), 10, 16, QStringLiteral("A & B <c>"), Qt::AlignLeft);
        text(QFont(), 50, 16, QStringLiteral("1.234,56"), Qt::AlignRight);
        text(QFont(), 10, 22, QStringLiteral("00123"), Qt::AlignLeft); // a code, not a number
        text(QFont(), 50, 22, QStringLiteral("1,234.5"), Qt::AlignRight);
        doc.pages << page;

        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("out.xlsx"));
        QVERIFY(BrExport::toXlsx(doc, file));
        QFile in(file);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const QByteArray zip = in.readAll();
        QVERIFY(zip.startsWith("PK"));
        QVERIFY(zip.contains("xl/worksheets/sheet1.xml"));
        QVERIFY(zip.contains("<c r=\"A1\" s=\"1\" t=\"inlineStr\"><is><t xml:space=\"preserve\">Name</t>"));
        QVERIFY(zip.contains("A &amp; B &lt;c&gt;"));
        QVERIFY(zip.contains("<c r=\"B2\" s=\"2\"><v>1234.56</v></c>"));
        QVERIFY(zip.contains("<t xml:space=\"preserve\">00123</t>"));
        QVERIFY(zip.contains("<c r=\"B3\" s=\"2\"><v>1234.5</v></c>"));
    }

    void rtf()
    {
        const BrDocument doc = makeDocument();
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("out.rtf"));
        QVERIFY(BrExport::toRtf(doc, file));
        QFile in(file);
        QVERIFY(in.open(QIODevice::ReadOnly));
        const QByteArray rtf = in.readAll();
        QVERIFY(rtf.startsWith("{\\rtf1"));
        QVERIFY(rtf.contains("\\pgwsxn11906"));
        QVERIFY(rtf.contains("Hello <world> & \"friends\""));
        QVERIFY(rtf.contains("\\shp"));
        QVERIFY(rtf.contains("\\pngblip"));
        // The braces of RTF groups are balanced
        int depth = 0;
        for (int i = 0; i < rtf.size(); ++i) {
            if (rtf.at(i) == '\\')
                ++i;
            else if (rtf.at(i) == '{')
                ++depth;
            else if (rtf.at(i) == '}')
                --depth;
            QVERIFY(depth >= 0);
        }
        QCOMPARE(depth, 0);
    }

    void landscape()
    {
        BrReport report;
        report.setOrientation(QPageLayout::Landscape);
        report.addBand(Br::BandType::Detail, 10);
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pages.first().size, QSizeF(297, 210));
    }
};

QTEST_MAIN(TstDocument)
#include "tst_document.moc"
