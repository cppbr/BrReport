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

// Groups: sales grouped by region and then by month, with group
// headers, subtotals, a grand total and a page break per region.

#include "exampleutil.h"

#include <QApplication>
#include <QDate>

namespace {

QList<QVariantMap> makeSales()
{
    const QStringList regions = {"Midwest", "North", "Northeast", "South", "Southeast"};
    const QStringList products = {"Keyboard", "Mouse", "Monitor", "Notebook", "Headset"};
    QList<QVariantMap> rows;
    int seed = 7;
    for (const QString &region : regions) {
        for (int month = 1; month <= 3; ++month) {
            for (int i = 0; i < 6 + (seed % 5); ++i) {
                seed = (seed * 1103515245 + 12345) & 0x7fffffff;
                const int qty = 1 + seed % 9;
                const double price = 50.0 + (seed % 2000) / 2.0;
                rows << QVariantMap{{"region", region},
                                    {"date", QDate(2026, month, 1 + (seed % 28))},
                                    {"product", products.at(seed % products.size())},
                                    {"qty", qty},
                                    {"price", price}};
            }
        }
    }
    return rows; // already sorted by region and month, as groups require
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BrListDataSource sales(makeSales());

    BrReport report;
    report.setTitle(QStringLiteral("Sales by Region - Q1 2026"));
    report.setLocale(QLocale(QLocale::Portuguese, QLocale::Brazil)); // 1.234,56
    report.setDataSource(&sales);
    report.setFont(QFont(QStringLiteral("Arial"), 9));
    const QString money = QStringLiteral("'R$ '#,##0.00");

    // Page header
    BrBand *header = report.addBand(Br::BandType::Header, 12);
    auto *title = new BrSystemInfo(Br::InfoType::Title, header);
    title->setGeometry(0, 0, 150, 9);
    title->setFont(QFont(QStringLiteral("Arial"), 14, QFont::Bold));
    auto *page = new BrSystemInfo(Br::InfoType::PagePreview, header);
    page->setGeometry(150, 0, 40, 9);
    page->setAlignment(Br::TextAlign::Right);
    page->setFormat(QStringLiteral("%1 / %2"));

    // Group 1: region (new page for every region)
    auto *byRegion = new BrGroup(&report);
    byRegion->setDataFields(QStringLiteral("region"));
    byRegion->setPageBreak(Br::PageBreak::Before);

    BrBand *regionHeader = byRegion->addBand(Br::BandType::Header, 9);
    regionHeader->setBackground(QColor(0x1f, 0x4e, 0x79));
    auto *regionName = new BrDBText(regionHeader);
    regionName->setDataFormula(QStringLiteral("'Region: ' + upper(region)"));
    regionName->setGeometry(2, 0, 120, 9);
    regionName->setColor(Qt::white);
    regionName->setFont(QFont(QStringLiteral("Arial"), 11, QFont::Bold));

    BrBand *regionFooter = byRegion->addBand(Br::BandType::Summary, 10);
    auto *regionTotal = new BrDBResult(regionFooter);
    regionTotal->setDataFormula(QStringLiteral("qty * price"));
    regionTotal->setResultType(Br::ResultType::Sum);
    regionTotal->setDisplayFormat(money);
    regionTotal->setPrefix(QStringLiteral("Region total: "));
    regionTotal->setGeometry(90, 2, 100, 7);
    regionTotal->setAlignment(Br::TextAlign::Right);
    regionTotal->setFont(QFont(QStringLiteral("Arial"), 10, QFont::Bold));
    regionTotal->setBorders(BrBorders::all(0.3));
    regionTotal->setPadding(1);

    // Group 2: month, nested in the region group
    auto *byMonth = new BrGroup(byRegion);
    byMonth->setDataFormula(QStringLiteral("month(date)"));

    BrBand *monthTitle = byMonth->addBand(Br::BandType::Title, 8);
    auto *monthName = new BrDBText(monthTitle);
    monthName->setDataFormula(QStringLiteral("formatdate(date, 'MMMM yyyy')"));
    monthName->setGeometry(4, 1, 80, 7);
    monthName->setFont(QFont(QStringLiteral("Arial"), 10, QFont::Bold));

    BrBand *columns = byMonth->addBand(Br::BandType::ColumnHeader, 6);
    columns->setBorders(BrBorders::bottomOnly());
    const struct { const char *text; double x, w; Br::TextAlign a; } heads[] = {
        {"Date", 8, 25, Br::TextAlign::Left},     {"Product", 35, 50, Br::TextAlign::Left},
        {"Qty", 90, 15, Br::TextAlign::Right},    {"Unit price", 110, 35, Br::TextAlign::Right},
        {"Amount", 150, 40, Br::TextAlign::Right}};
    for (const auto &h : heads) {
        auto *l = new BrLabel(QString::fromLatin1(h.text), columns);
        l->setGeometry(h.x, 0, h.w, 6);
        l->setAlignment(h.a);
        l->setFont(QFont(QStringLiteral("Arial"), 8, QFont::Bold));
    }

    BrBand *detail = byMonth->addBand(Br::BandType::Detail, 5);
    auto addField = [detail](const QString &field, double x, double w, Br::TextAlign a,
                             const QString &format = QString()) {
        auto *t = new BrDBText(field, detail);
        t->setGeometry(x, 0, w, 5);
        t->setAlignment(a);
        t->setDisplayFormat(format);
        return t;
    };
    addField(QStringLiteral("date"), 8, 25, Br::TextAlign::Left, QStringLiteral("dd/MM/yyyy"));
    addField(QStringLiteral("product"), 35, 50, Br::TextAlign::Left);
    addField(QStringLiteral("qty"), 90, 15, Br::TextAlign::Right);
    addField(QStringLiteral("price"), 110, 35, Br::TextAlign::Right, money);
    auto *amount = addField(QString(), 150, 40, Br::TextAlign::Right, money);
    amount->setDataFormula(QStringLiteral("qty * price"));

    BrBand *monthFooter = byMonth->addBand(Br::BandType::Summary, 7);
    auto *monthCount = new BrDBResult(QStringLiteral("product"), Br::ResultType::Count, monthFooter);
    monthCount->setGeometry(35, 0, 60, 6);
    monthCount->setSuffix(QStringLiteral(" sale(s)"));
    auto *monthTotal = new BrDBResult(monthFooter);
    monthTotal->setDataFormula(QStringLiteral("qty * price"));
    monthTotal->setDisplayFormat(money);
    monthTotal->setGeometry(130, 0, 60, 6);
    monthTotal->setAlignment(Br::TextAlign::Right);
    monthTotal->setBorders(BrBorders{false, true, false, false});
    monthTotal->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));

    // Grand total (end of report)
    BrBand *summary = report.addBand(Br::BandType::Summary, 14);
    auto *grand = new BrDBResult(summary);
    grand->setDataFormula(QStringLiteral("qty * price"));
    grand->setDisplayFormat(money);
    grand->setPrefix(QStringLiteral("GRAND TOTAL: "));
    grand->setGeometry(70, 4, 120, 9);
    grand->setAlignment(Br::TextAlign::Right);
    grand->setFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));

    return runExample(report);
}
