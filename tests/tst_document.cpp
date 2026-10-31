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
