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

// A classic listing: page header, column headers, zebra rows, page footer with
// "Page X of Y", per-page totals and a summary with grand totals.
// Everything is declarative, so the layout can be saved with --save customers.brt.

#include "exampleutil.h"

#include <QApplication>
#include <QDate>

namespace {

QList<QVariantMap> makeCustomers()
{
    const QStringList first = {"Ana", "Bruno", "Carla", "Diego", "Elisa", "Felipe", "Gabriela",
                               "Hugo", "Isabel", "Joao", "Karina", "Lucas", "Marina", "Nelson"};
    const QStringList last = {"Silva", "Souza", "Oliveira", "Santos", "Pereira", "Lima",
                              "Costa", "Ribeiro", "Almeida", "Carvalho"};
    const QStringList cities = {"Sao Paulo", "Rio de Janeiro", "Belo Horizonte", "Curitiba",
                                "Porto Alegre", "Recife", "Salvador", "Fortaleza"};
    QList<QVariantMap> rows;
    for (int i = 0; i < 90; ++i) {
        rows << QVariantMap{
            {"id", 1000 + i},
            {"name", first.at(i % first.size()) + ' ' + last.at((i * 7) % last.size())},
            {"city", cities.at((i * 3) % cities.size())},
            {"since", QDate(2015, 1, 1).addDays(i * 37)},
            {"balance", ((i * 7919) % 100000) / 10.0 - 1500.0},
        };
    }
    return rows;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BrListDataSource customers(makeCustomers());

    BrReport report;
    report.setTitle(QStringLiteral("Customer List"));
    report.setDataSource(QStringLiteral("customers"), &customers);
    report.setMargins(15, 12, 15, 12);
    report.setFont(QFont(QStringLiteral("Arial"), 9));
    const double width = report.contentRect().width(); // 180 mm on A4

    // --- Page header -----------------------------------------------------
    BrBand *header = report.addBand(Br::BandType::Header, 14);
    header->setBorders(BrBorders::bottomOnly(0.5));
    auto *title = new BrSystemInfo(Br::InfoType::Title, header);
    title->setGeometry(0, 0, 120, 10);
    title->setFont(QFont(QStringLiteral("Arial"), 16, QFont::Bold));
    auto *date = new BrSystemInfo(Br::InfoType::DateTime, header);
    date->setGeometry(width - 60, 0, 60, 10);
    date->setAlignment(Br::TextAlign::Right);
    date->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));

    // --- Column header (repeated on every page) ---------------------------
    struct Column { const char *caption; const char *field; double x, w; Br::TextAlign align; const char *format; };
    const Column columns[] = {
        {"Code", "id", 0, 15, Br::TextAlign::Right, ""},
        {"Name", "name", 18, 60, Br::TextAlign::Left, ""},
        {"City", "city", 80, 45, Br::TextAlign::Left, ""},
        {"Customer since", "since", 125, 25, Br::TextAlign::Center, "dd/MM/yyyy"},
        {"Balance", "balance", 150, 30, Br::TextAlign::Right, "#,##0.00;(#,##0.00)"},
    };
    BrBand *colHeader = report.addBand(Br::BandType::ColumnHeader, 7);
    colHeader->setBackground(QColor(0x2f, 0x55, 0x97));
    for (const Column &c : columns) {
        auto *l = new BrLabel(QString::fromLatin1(c.caption), colHeader);
        l->setGeometry(c.x, 0, c.w, 7);
        l->setAlignment(c.align);
        l->setColor(Qt::white);
        l->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));
    }

    // --- Detail ----------------------------------------------------------
    BrBand *detail = report.addBand(Br::BandType::Detail, 6);
    for (const Column &c : columns) {
        auto *t = new BrDBText(QString::fromLatin1(c.field), detail);
        t->setGeometry(c.x, 0, c.w, 6);
        t->setAlignment(c.align);
        t->setDisplayFormat(QString::fromLatin1(c.format));
        if (QByteArray(c.field) == "balance") {
            // Negative balances in red
            t->setHighlightIf(QStringLiteral("balance < 0"));
            t->setHighlightColor(Qt::red);
        }
    }
    detail->setAlternateBackground(QColor(0xee, 0xf2, 0xf8)); // zebra rows

    // --- Page footer: page totals and page numbers ------------------------
    BrBand *footer = report.addBand(Br::BandType::Footer, 7);
    footer->setBorders(BrBorders{false, true, false, false});
    auto *pageLabel = new BrLabel(QStringLiteral("Page total:"), footer);
    pageLabel->setGeometry(100, 0, 48, 7);
    pageLabel->setAlignment(Br::TextAlign::Right);
    auto *pageTotal = new BrDBResult(QStringLiteral("balance"), Br::ResultType::Sum, footer);
    pageTotal->setGeometry(150, 0, 30, 7);
    pageTotal->setAlignment(Br::TextAlign::Right);
    pageTotal->setDisplayFormat(QStringLiteral("#,##0.00;(#,##0.00)"));
    pageTotal->setResetAfterPrint(true); // per page
    auto *pages = new BrSystemInfo(Br::InfoType::PagePreview, footer);
    pages->setGeometry(0, 0, 50, 7);
    pages->setFormat(QStringLiteral("Page %1 of %2"));

    // --- Summary (end of report) -------------------------------------------
    BrBand *summary = report.addBand(Br::BandType::Summary, 12);
    auto *count = new BrDBResult(QStringLiteral("id"), Br::ResultType::Count, summary);
    count->setGeometry(0, 3, 80, 6);
    count->setPrefix(QStringLiteral("Customers: "));
    count->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));
    auto *avg = new BrDBResult(QStringLiteral("balance"), Br::ResultType::Average, summary);
    avg->setGeometry(60, 3, 60, 6);
    avg->setPrefix(QStringLiteral("Average: "));
    avg->setDisplayFormat(QStringLiteral("#,##0.00"));
    auto *total = new BrDBResult(QStringLiteral("balance"), Br::ResultType::Sum, summary);
    total->setGeometry(120, 3, 60, 6);
    total->setAlignment(Br::TextAlign::Right);
    total->setPrefix(QStringLiteral("Total: "));
    total->setDisplayFormat(QStringLiteral("#,##0.00;(#,##0.00)"));
    total->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));
    total->setBorders(BrBorders{false, true, false, false});

    return runExample(report);
}
