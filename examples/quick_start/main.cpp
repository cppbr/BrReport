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

// The program shown in the README "Quick start" section.

#include "brreportwidgets.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BrListDataSource customers({
        {{"name", "Ana Silva"},   {"city", "Sao Paulo"}, {"balance", 1520.75}},
        {{"name", "Bruno Souza"}, {"city", "Curitiba"},  {"balance", -310.00}},
    });

    BrReport report;
    report.setTitle("Customers");
    report.setDataSource(&customers);

    // Page header: printed at the top of every page
    BrBand *header = report.addBand(Br::BandType::Header, 12);
    auto *title = new BrSystemInfo(Br::InfoType::Title, header);
    title->setGeometry(0, 0, 120, 10);             // x, y, width, height in mm
    title->setFont(QFont("Arial", 16, QFont::Bold));

    // Detail: printed once per record
    BrBand *detail = report.addBand(Br::BandType::Detail, 6);
    (new BrDBText("name", detail))->setGeometry(0, 0, 80, 6);
    (new BrDBText("city", detail))->setGeometry(85, 0, 50, 6);
    auto *balance = new BrDBText("balance", detail);
    balance->setGeometry(140, 0, 50, 6);
    balance->setAlignment(Br::TextAlign::Right);
    balance->setDisplayFormat("#,##0.00");

    // Summary: printed once at the end
    BrBand *summary = report.addBand(Br::BandType::Summary, 8);
    auto *total = new BrDBResult("balance", Br::ResultType::Sum, summary);
    total->setGeometry(140, 1, 50, 6);
    total->setAlignment(Br::TextAlign::Right);
    total->setPrefix("Total: ");

    // Page footer: bottom of every page
    BrBand *footer = report.addBand(Br::BandType::Footer, 6);
    auto *page = new BrSystemInfo(Br::InfoType::PagePreview, footer);
    page->setGeometry(0, 0, 190, 6);
    page->setFormat("Page %1 of %2");

    BrPrinting::preview(&report);          // or: report.exportToPdf("customers.pdf");
    return 0;
}
