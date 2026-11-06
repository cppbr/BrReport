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

// Master/detail with SQL: every invoice starts on a new page and numbers its own
// pages ("page 1 of 2"), read from an
// in-memory SQLite database through BrSqlDataSource.

#include "exampleutil.h"

#include <QApplication>
#include <QDate>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

namespace {

bool createDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(QStringLiteral(":memory:"));
    if (!db.open()) {
        QTextStream(stderr) << "SQLite: " << db.lastError().text() << Qt::endl;
        return false;
    }
    QSqlQuery q;
    q.exec(QStringLiteral("CREATE TABLE invoice (id INTEGER PRIMARY KEY, customer TEXT, "
                          "document TEXT, issued DATE)"));
    q.exec(QStringLiteral("CREATE TABLE item (invoice_id INTEGER, description TEXT, "
                          "qty INTEGER, price REAL)"));
    const QStringList customers = {"Acme Ltda", "Brasil Foods S.A.", "Comercial Silva",
                                   "Delta Engenharia"};
    const QStringList products = {"Steel bolt M8", "Hydraulic hose 1/2\"", "Industrial glove",
                                  "Safety goggles", "Measuring tape 5 m", "Electric drill 650 W",
                                  "Hammer 500 g", "Screwdriver set (12 pieces)"};
    int seed = 3;
    for (int i = 0; i < customers.size(); ++i) {
        q.prepare(QStringLiteral("INSERT INTO invoice VALUES (?, ?, ?, ?)"));
        q.addBindValue(1001 + i);
        q.addBindValue(customers.at(i));
        q.addBindValue(QStringLiteral("%1.%2.%3/0001-%4")
                           .arg(11 + i * 7).arg(222 + i, 3, 10, QLatin1Char('0'))
                           .arg(333 + i, 3, 10, QLatin1Char('0')).arg(44 + i));
        q.addBindValue(QDate(2026, 10, 1 + i * 5));
        q.exec();
        const int lines = i == 3 ? 48 : 3 + i * 4; // the last invoice needs two pages
        for (int l = 0; l < lines; ++l) {
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            q.prepare(QStringLiteral("INSERT INTO item VALUES (?, ?, ?, ?)"));
            q.addBindValue(1001 + i);
            q.addBindValue(products.at(seed % products.size()));
            q.addBindValue(1 + seed % 20);
            q.addBindValue((seed % 50000) / 100.0 + 1.0);
            q.exec();
        }
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (!createDatabase())
        return 1;

    // The queries belong to the report, so --save writes a complete report file.
    BrReport report;
    report.setTitle(QStringLiteral("Invoices"));
    report.setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    report.addQuery(QStringLiteral("invoices"), QStringLiteral("SELECT * FROM invoice ORDER BY id"));
    report.addQuery(QStringLiteral("items"),
                    QStringLiteral("SELECT * FROM item ORDER BY invoice_id, description"));
    report.setDataSourceName(QStringLiteral("invoices"));
    report.setMargins(15, 15, 15, 15);
    const QString money = QStringLiteral("#,##0.00");

    // Master record: invoice header (Detail band of the report)
    BrBand *invoice = report.addBand(Br::BandType::Detail, 32);
    invoice->setPageBreak(Br::PageBreak::Before);
    auto *box = new BrDraw(Br::DrawKind::RoundedRectangle, invoice);
    box->setGeometry(0, 0, 180, 28);
    box->setFillColor(QColor(0xf3, 0xf6, 0xfa));
    box->setPenColor(QColor(0x9a, 0xa9, 0xbb));
    auto *number = new BrDBText(invoice);
    number->setDataFormula(QStringLiteral("'INVOICE #' + id"));
    number->setGeometry(4, 2, 100, 9);
    number->setFont(QFont(QStringLiteral("Arial"), 14, QFont::Bold));
    auto *customer = new BrDBText(QStringLiteral("customer"), invoice);
    customer->setGeometry(4, 12, 110, 6);
    customer->setFont(QFont(QStringLiteral("Arial"), 11));
    auto *doc = new BrDBText(QStringLiteral("document"), invoice);
    doc->setGeometry(4, 19, 110, 6);
    doc->setPrefix(QStringLiteral("Tax ID: "));
    auto *issued = new BrDBText(QStringLiteral("issued"), invoice);
    issued->setGeometry(120, 2, 56, 7);
    issued->setAlignment(Br::TextAlign::Right);
    issued->setPrefix(QStringLiteral("Issued: "));
    issued->setDisplayFormat(QStringLiteral("MMM d, yyyy"));
    auto *code = new BrDBBarcode(Br::BarcodeType::Code128, QStringLiteral("id"), invoice);
    code->setGeometry(126, 11, 50, 14);
    code->setFont(QFont(QStringLiteral("Arial"), 7));

    // Detail records: invoice items
    auto *lines = new BrSubDetail(&report);
    lines->setDataSourceName(QStringLiteral("items"));
    lines->setMasterFields(QStringLiteral("id"));
    lines->setDetailFields(QStringLiteral("invoice_id"));
    lines->setResetPageNumber(true); // "page X of Y" counts the pages of each invoice

    BrBand *cols = lines->addBand(Br::BandType::Header, 7);
    cols->setBorders(BrBorders::bottomOnly(0.3));
    const struct { const char *t; double x, w; Br::TextAlign a; } heads[] = {
        {"#", 0, 8, Br::TextAlign::Right}, {"Description", 12, 90, Br::TextAlign::Left},
        {"Qty", 105, 15, Br::TextAlign::Right}, {"Price", 122, 25, Br::TextAlign::Right},
        {"Amount", 150, 30, Br::TextAlign::Right}};
    for (const auto &h : heads) {
        auto *l = new BrLabel(QString::fromLatin1(h.t), cols);
        l->setGeometry(h.x, 0, h.w, 7);
        l->setAlignment(h.a);
        l->setFont(QFont(QStringLiteral("Arial"), 9, QFont::Bold));
    }

    BrBand *line = lines->addBand(Br::BandType::Detail, 6);
    auto *recno = new BrSystemInfo(Br::InfoType::RecNo, line);
    recno->setGeometry(0, 0, 8, 6);
    recno->setAlignment(Br::TextAlign::Right);
    (new BrDBText(QStringLiteral("description"), line))->setGeometry(12, 0, 90, 6);
    auto *qty = new BrDBText(QStringLiteral("qty"), line);
    qty->setGeometry(105, 0, 15, 6);
    qty->setAlignment(Br::TextAlign::Right);
    auto *price = new BrDBText(QStringLiteral("price"), line);
    price->setGeometry(122, 0, 25, 6);
    price->setAlignment(Br::TextAlign::Right);
    price->setDisplayFormat(money);
    auto *amount = new BrDBText(line);
    amount->setDataFormula(QStringLiteral("qty * price"));
    amount->setGeometry(150, 0, 30, 6);
    amount->setAlignment(Br::TextAlign::Right);
    amount->setDisplayFormat(money);

    BrBand *totals = lines->addBand(Br::BandType::Summary, 10);
    auto *total = new BrDBResult(totals);
    total->setDataFormula(QStringLiteral("qty * price"));
    total->setDisplayFormat(QStringLiteral("'Total: $ '#,##0.00"));
    total->setGeometry(110, 2, 70, 7);
    total->setAlignment(Br::TextAlign::Right);
    total->setFont(QFont(QStringLiteral("Arial"), 11, QFont::Bold));
    total->setBorders(BrBorders{false, true, false, false});

    // Page footer
    BrBand *footer = report.addBand(Br::BandType::Footer, 8);
    auto *pages = new BrSystemInfo(Br::InfoType::PagePreview, footer);
    pages->setGeometry(0, 0, 90, 8);
    pages->setFormat(QStringLiteral("Invoice page %1 of %2"));
    pages->setFont(QFont(QStringLiteral("Arial"), 8));
    auto *sheet = new BrDBText(footer);
    sheet->setDataFormula(QStringLiteral("'Sheet ' + documentpage() + ' of ' + documentpages()"));
    sheet->setGeometry(90, 0, 90, 8);
    sheet->setAlignment(Br::TextAlign::Right);
    sheet->setFont(QFont(QStringLiteral("Arial"), 8));

    return runExample(report);
}
