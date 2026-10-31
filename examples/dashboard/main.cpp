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

// Dashboard: charts (bars, lines, pie, horizontal bars), rich text (HTML),
// a table with grid lines that follow stretched cells, zebra rows, product
// labels in columns and a watermark.

#include "exampleutil.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QStringList months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"};
    BrListDataSource monthly({
        {{"month", "Jan"}, {"revenue", 182}, {"costs", 120}},
        {{"month", "Feb"}, {"revenue", 195}, {"costs", 131}},
        {{"month", "Mar"}, {"revenue", 240}, {"costs", 150}},
        {{"month", "Apr"}, {"revenue", 228}, {"costs", 149}},
        {{"month", "May"}, {"revenue", 265}, {"costs", 160}},
        {{"month", "Jun"}, {"revenue", 301}, {"costs", 171}},
    });
    BrListDataSource products({
        {{"sku", "BR-1001"}, {"name", "Wireless keyboard"}, {"notes", "Best seller in the first semester."}, {"stock", 120}, {"price", 189.9}},
        {{"sku", "BR-1002"}, {"name", "Optical mouse"}, {"notes", "Replaced the previous model in March; the new supplier delivers in 5 business days."}, {"stock", 340}, {"price", 59.9}},
        {{"sku", "BR-1003"}, {"name", "27\" monitor"}, {"notes", ""}, {"stock", 25}, {"price", 1499.0}},
        {{"sku", "BR-1004"}, {"name", "USB-C hub"}, {"notes", "Low stock: reorder."}, {"stock", 8}, {"price", 129.0}},
        {{"sku", "BR-1005"}, {"name", "Headset"}, {"notes", "Promotion in June."}, {"stock", 75}, {"price", 249.0}},
        {{"sku", "BR-1006"}, {"name", "Webcam Full HD"}, {"notes", ""}, {"stock", 52}, {"price", 199.0}},
    });

    BrReport report;
    report.setTitle(QStringLiteral("Sales Dashboard - 1st semester"));
    report.setDataSource(&products);
    report.setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    report.setFont(QFont(QStringLiteral("Arial"), 9));
    report.setWatermark(QStringLiteral("CONFIDENTIAL"));
    const double w = report.contentRect().width();

    // --- Title: heading, HTML summary and charts --------------------------
    BrBand *title = report.addBand(Br::BandType::Title, 178);
    auto *heading = new BrSystemInfo(Br::InfoType::Title, title);
    heading->setGeometry(0, 0, w, 10);
    heading->setFont(QFont(QStringLiteral("Arial"), 16, QFont::Bold));
    auto *intro = new BrLabel(title);
    intro->setGeometry(0, 11, w, 12);
    intro->setAllowHtml(true);
    intro->setText(QStringLiteral(
        "Revenue grew <b>65%</b> from January to June, while costs grew <b>43%</b>. "
        "The margin reached <font color='#2ca02c'><b>43%</b></font> in June. "
        "<i>Values in thousands of dollars.</i>"));

    auto *bars = new BrChart(Br::ChartType::Bar, title);
    bars->setGeometry(0, 26, w / 2 - 2, 70);
    bars->setTitle(QStringLiteral("Revenue x costs"));
    bars->setDataSource(&monthly, QStringLiteral("month"), {"revenue", "costs"});
    bars->setBorders(BrBorders::all(0.2, QColor(0xc0, 0xc8, 0xd0)));

    auto *lines = new BrChart(Br::ChartType::Line, title);
    lines->setGeometry(w / 2 + 2, 26, w / 2 - 2, 70);
    lines->setTitle(QStringLiteral("Margin (%)"));
    lines->setLabels(months);
    lines->addSeries(QStringLiteral("Margin"), {34.1, 32.8, 37.5, 34.6, 39.6, 43.2}, QColor(0x2c, 0xa0, 0x2c));
    lines->setShowValues(true);
    lines->setValueFormat(QStringLiteral("0.#"));
    lines->setBorders(BrBorders::all(0.2, QColor(0xc0, 0xc8, 0xd0)));

    auto *pie = new BrChart(Br::ChartType::Pie, title);
    pie->setGeometry(0, 100, w / 2 - 2, 70);
    pie->setTitle(QStringLiteral("Revenue by channel"));
    pie->setLabels({"Online store", "Resellers", "Physical stores", "Marketplaces"});
    pie->addSeries(QStringLiteral("Channel"), {46, 22, 18, 14});
    pie->setShowValues(true);
    pie->setBorders(BrBorders::all(0.2, QColor(0xc0, 0xc8, 0xd0)));

    auto *hbars = new BrChart(Br::ChartType::HorizontalBar, title);
    hbars->setGeometry(w / 2 + 2, 100, w / 2 - 2, 70);
    hbars->setTitle(QStringLiteral("Top regions"));
    hbars->setLabels({"Southeast", "South", "Northeast", "Midwest", "North"});
    hbars->addSeries(QStringLiteral("Revenue"), {612, 318, 254, 148, 79}, QColor(0x1f, 0x4e, 0x79));
    hbars->setShowValues(true);
    hbars->setBorders(BrBorders::all(0.2, QColor(0xc0, 0xc8, 0xd0)));

    // --- Products table: cells stretch together, notes push nothing ------
    struct Col { const char *caption; double x, w; Br::TextAlign a; };
    const Col cols[] = {{"SKU", 0, 22, Br::TextAlign::Left}, {"Product", 22, 45, Br::TextAlign::Left},
                        {"Notes", 67, 78, Br::TextAlign::Left}, {"Stock", 145, 20, Br::TextAlign::Right},
                        {"Price", 165, 25, Br::TextAlign::Right}};
    BrBand *head = report.addBand(Br::BandType::ColumnHeader, 7);
    head->setBackground(QColor(0x1f, 0x4e, 0x79));
    for (const Col &c : cols) {
        auto *l = new BrLabel(QString::fromLatin1(c.caption), head);
        l->setGeometry(c.x, 0, c.w, 7);
        l->setAlignment(c.a);
        l->setPadding(1);
        l->setColor(Qt::white);
        l->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));
    }
    BrBand *row = report.addBand(Br::BandType::Detail, 6);
    row->setAlternateBackground(QColor(0xee, 0xf2, 0xf8));
    const char *fields[] = {"sku", "name", "notes", "stock", "price"};
    for (int i = 0; i < 5; ++i) {
        const Col &c = cols[i];
        BrDBText *t = i == 2 ? new BrDBMemo(row) : new BrDBText(row);
        t->setDataField(QString::fromLatin1(fields[i]));
        t->setGeometry(c.x, 0, c.w, 6);
        t->setAlignment(c.a);
        t->setPadding(1);
        t->setLayout(Br::TextLayout::Top);
        t->setStretchToBand(true); // every cell as tall as the tallest one
        t->setBorders(BrBorders{false, false, true, true, 0.1, QColor(0xc0, 0xc8, 0xd0)});
        if (i == 4)
            t->setDisplayFormat(QStringLiteral("#,##0.00"));
        if (i == 3) // low stock in red
            t->setBeforePrint([&products](BrControl &c, bool &) {
                c.setColor(products.value("stock").toInt() < 20 ? Qt::red : Qt::black);
            });
    }

    // --- Summary: shelf labels in 3 columns, read from the same products ---
    auto *labels = new BrSubDetail(&report, &products);
    labels->setPositioning(Br::BandType::Summary);
    labels->setKeepHeaderWithDetail(true);
    auto *labelsTitle = labels->addBand(Br::BandType::Title, 14);
    auto *lt = new BrLabel(QStringLiteral("Shelf labels"), labelsTitle);
    lt->setGeometry(0, 6, w, 8);
    lt->setFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));
    BrBand *card = labels->addBand(Br::BandType::Detail, 30);
    card->setColumns(3, 4);
    auto *frame = new BrDraw(Br::DrawKind::RoundedRectangle, card);
    frame->setGeometry(0, 1, 60.6, 28);
    frame->setPenColor(QColor(0x9a, 0xa9, 0xbb));
    auto *name = new BrDBText(QStringLiteral("name"), card);
    name->setGeometry(3, 3, 55, 6);
    name->setShrinkToFit(true);
    name->setFont(QFont(QStringLiteral("Arial"), 11, QFont::Bold));
    auto *price = new BrDBText(QStringLiteral("price"), card);
    price->setGeometry(3, 10, 30, 8);
    price->setDisplayFormat(QStringLiteral("'$ '#,##0.00"));
    price->setFont(QFont(QStringLiteral("Arial"), 13, QFont::Bold));
    price->setColor(QColor(0x1f, 0x4e, 0x79));
    auto *code = new BrDBBarcode(Br::BarcodeType::Code128, QStringLiteral("sku"), card);
    code->setGeometry(3, 19, 32, 9);
    code->setFont(QFont(QStringLiteral("Arial"), 6));
    auto *qr = new BrDBBarcode(Br::BarcodeType::QrCode, QStringLiteral("sku"), card);
    qr->setGeometry(40, 9, 18, 18);

    BrBand *footer = report.addBand(Br::BandType::Footer, 6);
    auto *page = new BrSystemInfo(Br::InfoType::PagePreview, footer);
    page->setGeometry(0, 0, w, 6);
    page->setAlignment(Br::TextAlign::Right);
    page->setFormat(QStringLiteral("Page %1 of %2"));
    page->setFont(QFont(QStringLiteral("Arial"), 8));

    return runExample(report);
}
