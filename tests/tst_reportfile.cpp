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

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

namespace {

QList<QVariantMap> orders()
{
    return {
        {{"id", 1}, {"customer", "Ana"}, {"region", "North"}, {"total", 150.0}},
        {{"id", 2}, {"customer", "Bruno"}, {"region", "North"}, {"total", -20.0}},
        {{"id", 3}, {"customer", "Carla"}, {"region", "South"}, {"total", 75.5}},
    };
}

QList<QVariantMap> items()
{
    return {
        {{"order_id", 1}, {"product", "Pen"}, {"qty", 2}},
        {{"order_id", 1}, {"product", "Book"}, {"qty", 1}},
        {{"order_id", 3}, {"product", "Mug"}, {"qty", 4}},
    };
}

// A report that uses most objects and properties, built in code.
void buildReport(BrReport &report)
{
    report.setTitle(QStringLiteral("Round trip"));
    report.setPaperSize(QSizeF(210, 297));
    report.setLandscape(true);
    report.setMargins(12, 8, 12, 8);
    report.setLocaleName(QStringLiteral("pt_BR"));
    report.setFont(QFont(QStringLiteral("Arial"), 9));
    report.setDataSourceName(QStringLiteral("orders"));
    report.setVariable(QStringLiteral("company"), QStringLiteral("ACME"));
    report.setWatermark(QStringLiteral("DRAFT"));
    report.setReportDateTime(QDateTime(QDate(2026, 1, 2), QTime(3, 4)));

    auto *header = report.addBand(Br::BandType::Header, 12);
    header->setObjectName(QStringLiteral("pageHeader"));
    header->setBorders(BrBorders::bottomOnly(0.4, QColor(10, 20, 30)));
    auto *title = new BrSystemInfo(Br::InfoType::Title, header);
    title->setGeometry(0, 0, 120, 10);
    title->setFont(QFont(QStringLiteral("Arial"), 14, QFont::Bold));
    auto *company = new BrDBText(header);
    company->setDataFormula(QStringLiteral("company + ' - ' + formatdate(reportdate(), 'yyyy')"));
    company->setGeometry(150, 0, 100, 10);
    company->setAlignment(Br::TextAlign::Right);

    auto *group = new BrGroup(&report);
    group->setObjectName(QStringLiteral("byRegion"));
    group->setDataFields(QStringLiteral("region"));
    group->setKeepHeaderWithDetail(true);
    auto *gh = group->addBand(Br::BandType::Header, 8);
    gh->setBackground(QColor(0x1f, 0x4e, 0x79));
    auto *ghText = new BrDBText(gh);
    ghText->setDataFormula(QStringLiteral("'Region: ' + region"));
    ghText->setColor(Qt::white);
    ghText->setGeometry(2, 0, 100, 8);

    auto *detail = group->addBand(Br::BandType::Detail, 6);
    detail->setAlternateBackground(QColor(240, 240, 240));
    auto *customer = new BrDBText(QStringLiteral("customer"), detail);
    customer->setGeometry(0, 0, 60, 6);
    auto *total = new BrDBText(QStringLiteral("total"), detail);
    total->setObjectName(QStringLiteral("total"));
    total->setGeometry(60, 0, 40, 6);
    total->setAlignment(Br::TextAlign::Right);
    total->setDisplayFormat(QStringLiteral("#,##0.00;(#,##0.00)"));
    total->setHighlightIf(QStringLiteral("total < 0"));
    total->setHighlightColor(Qt::red);
    total->setHighlightBold(true);
    auto *flag = new BrLabel(QStringLiteral("big"), detail);
    flag->setGeometry(105, 0, 20, 6);
    flag->setPrintIf(QStringLiteral("total > 100"));

    auto *sub = new BrSubDetail(group);
    sub->setDataSourceName(QStringLiteral("items"));
    sub->setMasterFields(QStringLiteral("id"));
    sub->setDetailFields(QStringLiteral("order_id"));
    auto *line = sub->addBand(Br::BandType::Detail, 5);
    auto *product = new BrDBText(line);
    product->setDataFormula(QStringLiteral("qty + ' x ' + product"));
    product->setGeometry(10, 0, 80, 5);
    product->setFont(QFont(QStringLiteral("Arial"), 8, QFont::Normal, true));

    auto *gs = group->addBand(Br::BandType::Summary, 8);
    auto *sum = new BrDBResult(QStringLiteral("total"), Br::ResultType::Sum, gs);
    sum->setGeometry(60, 0, 40, 8);
    sum->setDisplayFormat(QStringLiteral("#,##0.00"));
    sum->setBorders(BrBorders{false, true, false, false, 0.3, Qt::black, Qt::DashLine});

    auto *summary = report.addBand(Br::BandType::Summary, 70);
    auto *memo = new BrMemo(summary);
    memo->setGeometry(0, 0, 100, 10);
    memo->setAllowHtml(true);
    memo->setText(QStringLiteral("<b>Total</b> of <i>all</i> regions"));
    auto *chart = new BrChart(Br::ChartType::Bar, summary);
    chart->setGeometry(0, 12, 120, 50);
    chart->setTitle(QStringLiteral("Totals"));
    chart->setLabels({QStringLiteral("North"), QStringLiteral("South")});
    chart->addSeries(QStringLiteral("2026"), {130, 75.5}, QColor(1, 2, 3));
    chart->setShowValues(true);
    auto *qr = new BrBarcode(Br::BarcodeType::QrCode, QStringLiteral("https://example.com"), summary);
    qr->setGeometry(130, 12, 30, 30);
    qr->setQrErrorCorrection(Br::QrErrorCorrection::High);
    auto *draw = new BrDraw(Br::DrawKind::RoundedRectangle, summary);
    draw->setGeometry(170, 12, 20, 20);
    draw->setFillColor(QColor(255, 0, 0, 128));
    draw->setPenStyle(Qt::DotLine);
    auto *panel = new BrPanel(summary);
    panel->setGeometry(200, 12, 40, 20);
    panel->setBorders(BrBorders::all());
    auto *inPanel = new BrLabel(QStringLiteral("inside"), panel);
    inPanel->setAlign(Br::Align::Client);
    auto *image = new BrImage(summary);
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    image->setImage(img);
    image->setGeometry(245, 12, 10, 10);

    auto *footer = report.addBand(Br::BandType::Footer, 6);
    auto *page = new BrSystemInfo(Br::InfoType::PagePreview, footer);
    page->setFormat(QStringLiteral("Page %1 of %2"));
}

void compareDocuments(const BrDocument &a, const BrDocument &b)
{
    QCOMPARE(a.pageCount(), b.pageCount());
    for (int p = 0; p < a.pageCount(); ++p) {
        const BrPage &pa = a.pages.at(p), &pb = b.pages.at(p);
        QCOMPARE(pa.size, pb.size);
        QCOMPARE(pa.items.size(), pb.items.size());
        for (int i = 0; i < pa.items.size(); ++i) {
            const BrPageItem &x = pa.items.at(i), &y = pb.items.at(i);
            QCOMPARE(int(x.kind), int(y.kind));
            QCOMPARE(x.rect, y.rect);
            QCOMPARE(x.text, y.text);
            QCOMPARE(x.font, y.font);
            QCOMPARE(x.textColor, y.textColor);
            QCOMPARE(x.brushColor, y.brushColor);
            QCOMPARE(int(x.penStyle), int(y.penStyle));
        }
    }
}

} // namespace

class TstReportFile : public QObject
{
    Q_OBJECT
private slots:
    void roundTrip()
    {
        BrListDataSource ordersDs(orders()), itemsDs(items());
        BrReport original;
        original.addDataSource(QStringLiteral("orders"), &ordersDs);
        original.addDataSource(QStringLiteral("items"), &itemsDs);
        buildReport(original);
        QVERIFY2(original.prepare(), qPrintable(original.errorString()));

        const QByteArray json = original.toJson();
        BrReport loaded;
        loaded.addDataSource(QStringLiteral("orders"), &ordersDs);
        loaded.addDataSource(QStringLiteral("items"), &itemsDs);
        QVERIFY2(loaded.loadFromJson(json), qPrintable(loaded.errorString()));
        loaded.setReportDateTime(original.reportDateTime()); // not part of the layout
        QVERIFY2(loaded.prepare(), qPrintable(loaded.errorString()));

        compareDocuments(original.document(), loaded.document());
        QCOMPARE(loaded.toJson(), json); // stable
        QVERIFY(loaded.object<BrDBText>(QStringLiteral("total")));
        QVERIFY(loaded.object<BrGroup>(QStringLiteral("byRegion")));

        // Behavior expressed in the file
        const QString text = original.document().pageText(0);
        QVERIFY(text.contains(QStringLiteral("2 x Pen")));
        QVERIFY(!text.contains(QStringLiteral("Mug\nBook")));
        QCOMPARE(text.count(QStringLiteral("big")), 1);
    }

    void saveAndLoadFile()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("report.brt"));
        BrReport report;
        buildReport(report);
        QVERIFY(report.save(file));
        BrReport other;
        QVERIFY(other.load(file));
        QCOMPARE(other.toJson(), report.toJson());
        QVERIFY(!other.load(dir.filePath(QStringLiteral("missing.brt"))));
    }

    void handwrittenFile()
    {
        const QByteArray json = R"({
          "format": "BrReport", "version": 1,
          "report": {
            "title": "Handwritten", "paperSize": [80, 200], "continuous": true,
            "margins": [4, 4, 4, 4], "dataSourceName": "orders", "localeName": "en_US",
            "children": [
              { "type": "Band", "bandType": "Detail", "height": 6, "children": [
                  { "type": "DBText", "dataField": "customer", "geometry": [0, 0, 40, 6] },
                  { "type": "DBText", "name": "value", "dataField": "total", "geometry": [40, 0, 32, 6],
                    "alignment": "Right", "displayFormat": "0.00",
                    "highlightIf": "total < 0", "highlightColor": "#ff0000" } ] },
              { "type": "Band", "bandType": "Summary", "height": 6, "children": [
                  { "type": "DBResult", "dataField": "total", "resultType": "Count",
                    "prefix": "Orders: ", "geometry": [0, 0, 72, 6] } ] }
            ]
          }
        })";
        BrListDataSource ds(orders());
        BrReport report;
        report.addDataSource(QStringLiteral("orders"), &ds);
        QVERIFY2(report.loadFromJson(json), qPrintable(report.errorString()));
        QVERIFY(report.prepare());
        const BrDocument &doc = report.document();
        QCOMPARE(doc.pageCount(), 1);
        QCOMPARE(doc.pages.first().size, QSizeF(80, 4 + 3 * 6 + 6 + 4));
        QVERIFY(doc.pageText(0).contains(QStringLiteral("Orders: 3")));
        int highlighted = 0;
        for (const BrPageItem &it : doc.pages.first().items)
            if (it.text == QStringLiteral("-20.00")) {
                QCOMPARE(it.textColor, QColor(Qt::red));
                ++highlighted;
            }
        QCOMPARE(highlighted, 1);

        // Handlers can still be attached after loading
        auto *value = report.object<BrDBText>(QStringLiteral("value"));
        QVERIFY(value);
        value->setBeforePrint([](BrControl &c, bool &) { static_cast<BrDBText &>(c).setSuffix(QStringLiteral(" $")); });
        QVERIFY(report.prepare());
        QVERIFY(report.document().pageText(0).contains(QStringLiteral("150.00 $")));
    }

    void errors()
    {
        BrReport report;
        new BrLabel(QStringLiteral("keep?"), report.addBand(Br::BandType::Detail, 5));
        QVERIFY(!report.loadFromJson("{ not json"));
        QVERIFY(report.errorString().contains(QStringLiteral("Invalid JSON")));
        QVERIFY(!report.loadFromJson(R"({"format":"Other","version":1})"));
        QVERIFY(!report.loadFromJson(R"({"format":"BrReport","version":99,"report":{}})"));
        QVERIFY(!report.loadFromJson(R"({"format":"BrReport","version":1,"report":{"children":[{"type":"Nope"}]}})"));
        QVERIFY(report.errorString().contains(QStringLiteral("Nope")));
        QVERIFY(report.bands().isEmpty()); // no half-loaded layout
        // A label cannot be a direct child of the report
        QVERIFY(!report.loadFromJson(R"({"format":"BrReport","version":1,"report":{"children":[{"type":"Label"}]}})"));

        // Unknown properties are reported but do not stop loading
        QStringList warnings;
        QString error;
        QVERIFY(BrReportFile::fromJson(&report, R"({"format":"BrReport","version":1,"report":{
            "children":[{"type":"Band","bandType":"Detail","color2":"x","height":"bad"}]}})", &error, &warnings));
        QCOMPARE(warnings.size(), 2);
        QCOMPARE(report.bands().size(), 1);
    }

    void customType()
    {
        BrReportFile::registerType(QStringLiteral("Stamp"), [] {
            auto *l = new BrLabel(nullptr);
            l->setObjectName(QStringLiteral("stamp"));
            return l;
        });
        QVERIFY(BrReportFile::typeNames().contains(QStringLiteral("Stamp")));
        QVERIFY(BrReportFile::create(QStringLiteral("Stamp")));
        QCOMPARE(BrReportFile::typeName(new BrDBResult(nullptr)), QStringLiteral("DBResult"));
    }

    void protection()
    {
        BrReport report;
        report.setProtection(BrProtection{false, false, true, false, {QStringLiteral("queries")}});
        BrBand *band = report.addBand(Br::BandType::Detail, 10);
        band->setProtection(BrProtection{true, true, false, true, {}});
        auto *group = new BrGroup(&report);
        group->setProtection(BrProtection{false, true, false, false, {}});
        auto *label = new BrLabel(QStringLiteral("Fixed"), band);
        label->setProtection(BrProtection::all({QStringLiteral("text")}));
        auto *free = new BrLabel(QStringLiteral("Free"), band);

        const QByteArray json = report.toJson();
        const QJsonObject root = QJsonDocument::fromJson(json).object().value(QStringLiteral("report")).toObject();
        const QJsonObject file = root.value(QStringLiteral("children")).toArray().at(0).toObject();
        QCOMPARE(file.value(QStringLiteral("protection")).toObject().value(QStringLiteral("delete")), QJsonValue(true));
        QVERIFY(!file.value(QStringLiteral("protection")).toObject().contains(QStringLiteral("properties")));
        QVERIFY(!file.value(QStringLiteral("children")).toArray().at(1).toObject().contains(QStringLiteral("protection")));

        BrReport loaded;
        QVERIFY(loaded.loadFromJson(json));
        QCOMPARE(loaded.protection(), report.protection());
        QCOMPARE(loaded.bands().first()->protection(), band->protection());
        QCOMPARE(loaded.findChild<BrGroup *>()->protection(), group->protection());
        QCOMPARE(loaded.bands().first()->controls().first()->protection(), label->protection());
        QVERIFY(!loaded.bands().first()->controls().last()->protection().any());
        QCOMPARE(BrProtection::of(free), BrProtection());
        QCOMPARE(BrProtection::of(this), BrProtection());
        QCOMPARE(BrProtection::of(nullptr), BrProtection());

        // Loading another file does not keep the protection of the report
        BrReport empty;
        QVERIFY(loaded.loadFromJson(empty.toJson()));
        QVERIFY(!loaded.protection().any());
    }

    void masterDetailLinkInCode()
    {
        BrListDataSource ordersDs(orders()), itemsDs(items());
        BrReport report;
        report.setDataSource(QStringLiteral("orders"), &ordersDs);
        report.addDataSource(QStringLiteral("items"), &itemsDs);
        new BrDBText(QStringLiteral("customer"), report.addBand(Br::BandType::Detail, 5));
        auto *sub = new BrSubDetail(&report);
        sub->setDataSourceName(QStringLiteral("items"));
        sub->setMasterFields(QStringLiteral("id"));
        sub->setDetailFields(QStringLiteral("order_id"));
        new BrDBText(QStringLiteral("product"), sub->addBand(Br::BandType::Detail, 5));
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageText(0),
                 QStringLiteral("Ana\nPen\nBook\nBruno\nCarla\nMug"));
    }

    void sqlQueriesInFile()
    {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("erp"));
            db.setDatabaseName(QStringLiteral(":memory:"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE customer (name TEXT, city TEXT)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO customer VALUES ('Ana','Recife'),('Bruno','Belem'),('Carla','Recife')")));
        }
        const QByteArray json = R"({"format":"BrReport","version":1,"report":{
            "dataSourceName":"customers",
            "variables":{"city":"Recife"},
            "queries":[{"name":"customers","connection":"erp",
                        "sql":"SELECT name FROM customer WHERE city = :city ORDER BY name"}],
            "children":[{"type":"Band","bandType":"Detail","height":5,
                         "children":[{"type":"DBText","dataField":"name"}]}]}})";
        BrReport report;
        QVERIFY2(report.loadFromJson(json), qPrintable(report.errorString()));
        QVERIFY(report.dataSourceNames().contains(QStringLiteral("customers")));
        QVERIFY2(report.prepare(), qPrintable(report.errorString()));
        QCOMPARE(report.document().pageText(0), QStringLiteral("Ana\nCarla"));

        // Parameters come from the report variables
        report.setVariable(QStringLiteral("city"), QStringLiteral("Belem"));
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageText(0), QStringLiteral("Bruno"));

        // Saved with the layout
        QVERIFY(report.toJson().contains("SELECT name FROM customer"));

        // An application data source with the same name wins
        BrListDataSource own({{{QStringLiteral("name"), QStringLiteral("From the app")}}});
        report.addDataSource(QStringLiteral("customers"), &own);
        QVERIFY(report.prepare());
        QCOMPARE(report.document().pageText(0), QStringLiteral("From the app"));

        // Errors are reported
        BrReport bad;
        bad.addQuery(QStringLiteral("x"), QStringLiteral("SELECT * FROM missing"), QStringLiteral("erp"));
        QVERIFY(!bad.prepare());
        QVERIFY(bad.errorString().contains(QStringLiteral("Query 'x'")));
        BrReport noConnection;
        noConnection.addQuery(QStringLiteral("x"), QStringLiteral("SELECT 1"), QStringLiteral("nope"));
        QVERIFY(!noConnection.prepare());
        QVERIFY(noConnection.errorString().contains(QStringLiteral("not open")));
    }

    void parameters()
    {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("params"));
            db.setDatabaseName(QStringLiteral(":memory:"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE sale (id INTEGER, day TEXT)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO sale VALUES (1,'2026-01-10'),(2,'2026-02-10'),(3,'2026-03-10')")));
        }
        const QByteArray json = R"({"format":"BrReport","version":1,"report":{
            "dataSourceName":"sales",
            "parameters":[{"name":"start","caption":"From","type":"date","default":"2026-02-01"},
                          {"name":"minId","type":"integer","default":"1"}],
            "queries":[{"name":"sales","connection":"params",
                        "sql":"SELECT id FROM sale WHERE day >= :start AND id >= :minId ORDER BY id"}],
            "children":[{"type":"Band","bandType":"Detail","height":5,
                         "children":[{"type":"DBText","dataField":"id"}]},
                        {"type":"Band","bandType":"Footer","height":5,
                         "children":[{"type":"DBText","dataFormula":"minId + 1"}]}]}})";
        BrReport report;
        QVERIFY2(report.loadFromJson(json), qPrintable(report.errorString()));

        // Defaults apply without asking, typed, and do not become variables
        QCOMPARE(report.variable(QStringLiteral("start")), QVariant(QDate(2026, 2, 1)));
        QCOMPARE(report.variable(QStringLiteral("MINID")), QVariant(qlonglong(1)));
        QVERIFY(report.variables().isEmpty());
        QVERIFY2(report.prepare(), qPrintable(report.errorString()));
        QVERIFY(report.document().pageText(0).startsWith(QStringLiteral("2\n3")));

        // A variable overrides the default and is converted to the parameter type
        report.setVariable(QStringLiteral("start"), QStringLiteral("2026-03-01"));
        QCOMPARE(report.variable(QStringLiteral("start")), QVariant(QDate(2026, 3, 1)));
        QVERIFY(report.prepare());
        QVERIFY(report.document().pageText(0).startsWith(QStringLiteral("3")));

        // Saved with the layout
        BrReport copy;
        QVERIFY(copy.loadFromJson(report.toJson()));
        QCOMPARE(copy.parameters(), report.parameters());

        // Choices from a query, bound to the report variables
        QVariantMap customer{{QStringLiteral("name"), QStringLiteral("sale")},
                             {QStringLiteral("choicesConnection"), QStringLiteral("params")},
                             {QStringLiteral("choicesSql"),
                              QStringLiteral("SELECT id, 'Sale ' || id FROM sale WHERE day >= :start ORDER BY id")}};
        QString error;
        const QVariantList choices = report.parameterChoices(customer, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(choices.size(), 1);
        QCOMPARE(choices.first().toMap().value(QStringLiteral("value")).toInt(), 3);
        QCOMPARE(choices.first().toMap().value(QStringLiteral("caption")).toString(), QStringLiteral("Sale 3"));

        customer[QStringLiteral("choicesSql")] = QStringLiteral("SELECT id FROM sale ORDER BY id");
        const QVariantList plain = report.parameterChoices(customer, &error);
        QCOMPARE(plain.size(), 3);
        QVERIFY(!plain.first().toMap().contains(QStringLiteral("caption")));

        customer[QStringLiteral("choices")] = QVariantList{QStringLiteral("ignored")};
        QCOMPARE(report.parameterChoices(customer).size(), 3); // the query wins over a fixed list
        customer[QStringLiteral("choicesSql")] = QStringLiteral("SELECT * FROM missing");
        QVERIFY(report.parameterChoices(customer, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        customer[QStringLiteral("choicesConnection")] = QStringLiteral("nope");
        QVERIFY(report.parameterChoices(customer, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("not open")));
        customer.remove(QStringLiteral("choicesSql"));
        QCOMPARE(report.parameterChoices(customer, &error).size(), 1);
        QVERIFY(error.isEmpty());

        // Conversions
        const QVariantMap date{{QStringLiteral("type"), QStringLiteral("date")}};
        QCOMPARE(BrReport::parameterValue(date, QStringLiteral("today")), QVariant(QDate::currentDate()));
        QCOMPARE(BrReport::parameterValue(date, QVariant()), QVariant(QDate::currentDate()));
        const QVariantMap flag{{QStringLiteral("type"), QStringLiteral("boolean")}};
        QCOMPARE(BrReport::parameterValue(flag, QStringLiteral("false")), QVariant(false));
        QCOMPARE(BrReport::parameterValue({}, 12), QVariant(QStringLiteral("12")));
    }
};

QTEST_MAIN(TstReportFile)
#include "tst_reportfile.moc"
