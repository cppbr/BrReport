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

// Receipt for an 80 mm roll printer: a continuous page that grows with the
// content, word-wrapped item descriptions, totals and a payment QR Code.

#include "exampleutil.h"

#include <QApplication>
#include <QDateTime>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BrListDataSource items({
        {{"code", "0001"}, {"description", "Espresso coffee"}, {"qty", 2}, {"price", 6.50}},
        {{"code", "0002"}, {"description", "Cheese bread (portion with 6 units, freshly baked)"}, {"qty", 1}, {"price", 12.90}},
        {{"code", "0003"}, {"description", "Orange juice 500 ml"}, {"qty", 1}, {"price", 9.00}},
        {{"code", "0004"}, {"description", "Chocolate cake slice"}, {"qty", 3}, {"price", 8.75}},
        {{"code", "0005"}, {"description", "Mineral water"}, {"qty", 2}, {"price", 3.50}},
    });

    BrReport report;
    report.setTitle(QStringLiteral("Receipt"));
    report.setDataSource(&items);
    report.setPageSizeMm(80, 200);       // roll width; the height grows with the content
    report.setMargins(4, 4, 4, 4);
    report.setContinuous(true);
    report.setLocale(QLocale(QLocale::Portuguese, QLocale::Brazil));
    report.setFont(QFont(QStringLiteral("Arial"), 8));
    const double w = report.contentRect().width(); // 72 mm
    const QString money = QStringLiteral("#,##0.00");

    // Store header
    BrBand *title = report.addBand(Br::BandType::Title, 26);
    auto *store = new BrLabel(QStringLiteral("CAFE BRASILEIRO LTDA"), title);
    store->setGeometry(0, 0, w, 6);
    store->setAlignment(Br::TextAlign::Center);
    store->setFont(QFont(QStringLiteral("Arial"), 10, QFont::Bold));
    auto *address = new BrMemo(title);
    address->setGeometry(0, 6, w, 8);
    address->setAlignment(Br::TextAlign::Center);
    address->setText(QStringLiteral("Av. Paulista, 1000 - Sao Paulo - SP\nTax ID 12.345.678/0001-90"));
    auto *when = new BrSystemInfo(Br::InfoType::DateTime, title);
    when->setGeometry(0, 15, w, 5);
    when->setAlignment(Br::TextAlign::Center);
    when->setDisplayFormat(QStringLiteral("dd/MM/yyyy HH:mm"));
    auto *cols = new BrLabel(QStringLiteral("ITEM  DESCRIPTION              QTY x PRICE       TOTAL"), title);
    cols->setGeometry(0, 21, w, 5);
    cols->setBorders(BrBorders{false, true, false, true, 0.2, Qt::black, Qt::DashLine});
    cols->setFont(QFont(QStringLiteral("Arial"), 6.5, QFont::Bold));

    // One block per item; long descriptions wrap and push the line below
    BrBand *item = report.addBand(Br::BandType::Detail, 8);
    auto *desc = new BrDBMemo(item);
    desc->setDataFormula(QStringLiteral("code + '  ' + description"));
    desc->setGeometry(0, 0, w, 4);
    auto *qty = new BrDBText(item);
    qty->setDataFormula(QStringLiteral("qty + ' x ' + formatnumber(price, '#,##0.00')"));
    qty->setGeometry(8, 4, 40, 4);
    auto *total = new BrDBText(item);
    total->setDataFormula(QStringLiteral("qty * price"));
    total->setDisplayFormat(money);
    total->setGeometry(w - 25, 4, 25, 4);
    total->setAlignment(Br::TextAlign::Right);

    // Totals, payment QR Code and thanks
    BrBand *summary = report.addBand(Br::BandType::Summary, 64);
    auto *count = new BrDBResult(QStringLiteral("code"), Br::ResultType::Count, summary);
    count->setGeometry(0, 1, w, 5);
    count->setPrefix(QStringLiteral("Items: "));
    count->setBorders(BrBorders{false, true, false, false, 0.2, Qt::black, Qt::DashLine});
    auto *sumLabel = new BrLabel(QStringLiteral("TOTAL R$"), summary);
    sumLabel->setGeometry(0, 6, 30, 7);
    sumLabel->setFont(QFont(QStringLiteral("Arial"), 11, QFont::Bold));
    auto *sum = new BrDBResult(summary);
    sum->setDataFormula(QStringLiteral("qty * price"));
    sum->setDisplayFormat(money);
    sum->setGeometry(w - 40, 6, 40, 7);
    sum->setAlignment(Br::TextAlign::Right);
    sum->setFont(QFont(QStringLiteral("Arial"), 11, QFont::Bold));
    auto *qr = new BrBarcode(Br::BarcodeType::QrCode,
                             QStringLiteral("https://example.com/receipt?id=35261012345678000190"), summary);
    qr->setGeometry((w - 32) / 2, 15, 32, 32);
    auto *scan = new BrLabel(QStringLiteral("Scan to check your receipt"), summary);
    scan->setGeometry(0, 48, w, 5);
    scan->setAlignment(Br::TextAlign::Center);
    auto *thanks = new BrLabel(QStringLiteral("<b>Thank you</b> for your visit!"), summary);
    thanks->setAllowHtml(true);
    thanks->setGeometry(0, 55, w, 6);
    thanks->setAlignment(Br::TextAlign::Center);

    return runExample(report);
}
