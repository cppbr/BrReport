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

#include "brreportwizard_p.h"
#include "brpropertyeditor_p.h"
#include "brreportcore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMetaEnum>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

#ifdef BRREPORT_HAS_SQL
#include <QSqlDatabase>
#endif

#include <cmath>

namespace {

using Column = BrReportWizard::Column;
using FieldKind = BrReportWizard::FieldKind;

const int SampleRows = 50;
const int MinColumnSize = 6;  // characters
const int MaxColumnSize = 40;
const double CharacterWidth = 1.9; // mm, an average character at 9 pt
const double TextPadding = 1.0;    // mm
const char *const DecimalFormat = "#,##0.00";

enum TableColumn { FieldColumn, CaptionColumn, TotalColumn };
enum ItemRole { KindRole = Qt::UserRole, SizeRole };

bool isNumber(FieldKind kind)
{
    return kind == FieldKind::Integer || kind == FieldKind::Decimal;
}

/*! Decimal numbers read as text are common in SQL drivers; "01310" is more likely a code. */
FieldKind kindOf(const QVariant &value)
{
    if (BrFormat::isDateTime(value))
        return FieldKind::Date;
    if (value.userType() == QMetaType::QString) {
        const bool decimal = value.toString().contains(QLatin1Char('.')) && BrFormat::isNumeric(value);
        return decimal ? FieldKind::Decimal : FieldKind::Text;
    }
    if (!BrFormat::isNumeric(value))
        return FieldKind::Text;
    const int type = value.userType();
    return type == QMetaType::Double || type == QMetaType::Float ? FieldKind::Decimal : FieldKind::Integer;
}

FieldKind mergedKind(FieldKind a, FieldKind b)
{
    if (a == FieldKind::Unknown || a == b)
        return b;
    return isNumber(a) && isNumber(b) ? FieldKind::Decimal : FieldKind::Text;
}

int columnSize(const Column &column)
{
    const int size = qMax(column.caption.size(), column.size + (column.total >= 0 ? 2 : 0));
    return qBound(MinColumnSize, size, MaxColumnSize);
}

double neededWidth(const QList<Column> &columns)
{
    double width = 0;
    for (const Column &c : columns)
        width += columnSize(c) * CharacterWidth + 2 * TextPadding;
    return width;
}

double printableWidth(bool landscape)
{
    BrReport report;
    report.setLandscape(landscape);
    return report.contentRect().width();
}

QJsonObject boldFont()
{
    return {{QStringLiteral("family"), QStringLiteral("Arial")},
            {QStringLiteral("size"), 9},
            {QStringLiteral("bold"), true}};
}

QJsonObject band(const char *type, double height, const QJsonArray &children = {})
{
    QJsonObject json{{QStringLiteral("type"), QStringLiteral("Band")},
                     {QStringLiteral("bandType"), QString::fromLatin1(type)},
                     {QStringLiteral("height"), height}};
    if (!children.isEmpty())
        json.insert(QStringLiteral("children"), children);
    return json;
}

QJsonObject text(const char *type, double x, double width, double height, const QString &alignment = QStringLiteral("Left"))
{
    return {{QStringLiteral("type"), QString::fromLatin1(type)},
            {QStringLiteral("geometry"), QJsonArray{x, 0, width, height}},
            {QStringLiteral("alignment"), alignment},
            {QStringLiteral("layout"), QStringLiteral("Center")},
            {QStringLiteral("padding"), TextPadding}};
}

QString alignmentOf(const Column &column)
{
    return isNumber(column.kind) ? QStringLiteral("Right") : QStringLiteral("Left");
}

/*! Left edge of each column, plus the end of the last one, filling \a width in whole mm. */
QList<double> columnEdges(const QList<Column> &columns, double width)
{
    double sizes = 0;
    for (const Column &c : columns)
        sizes += columnSize(c);
    QList<double> edges{0.0};
    for (int i = 0; i < columns.size() - 1; ++i)
        edges << edges.last() + std::floor(width * columnSize(columns.at(i)) / sizes);
    edges << width;
    return edges;
}

/*! The band with the totals of \a columns; empty when there are none. */
QJsonObject totalsBand(const QList<Column> &columns, const QList<double> &edges, const QString &caption)
{
    const double height = 7;
    const QMetaEnum resultTypes = QMetaEnum::fromType<Br::ResultType>();
    QJsonArray children;
    for (int i = 0; i < columns.size(); ++i) {
        const Column &column = columns.at(i);
        if (column.total < 0)
            continue;
        const auto type = Br::ResultType(column.total);
        QJsonObject json = text("DBResult", edges.at(i), edges.at(i + 1) - edges.at(i), height, alignmentOf(column));
        json.insert(QStringLiteral("dataField"), column.field);
        json.insert(QStringLiteral("resultType"), QString::fromLatin1(resultTypes.valueToKey(column.total)));
        json.insert(QStringLiteral("font"), boldFont());
        json.insert(QStringLiteral("shrinkToFit"), true); // totals are longer than the values
        const bool decimal = type == Br::ResultType::Average
                             || (column.kind == FieldKind::Decimal && type != Br::ResultType::Count);
        if (decimal)
            json.insert(QStringLiteral("displayFormat"), QString::fromLatin1(DecimalFormat));
        children << json;
    }
    if (children.isEmpty())
        return {};
    if (columns.first().total < 0) {
        QJsonObject label = text("Label", 0, edges.at(1), height);
        label.insert(QStringLiteral("text"), caption);
        label.insert(QStringLiteral("font"), boldFont());
        children << label;
    }
    QJsonObject json = band("Summary", height, children);
    json.insert(QStringLiteral("borders"), QJsonObject{{QStringLiteral("sides"), QStringLiteral("t")},
                                                       {QStringLiteral("width"), 0.3}});
    return json;
}

} // namespace

// ---------------------------------------------------------------------------

class BrReportWizard::DataPage : public QWizardPage
{
public:
    explicit DataPage(const QMap<QString, BrDataSource *> &dataSources) : m_dataSources(dataSources)
    {
        setTitle(BrReportWizard::tr("Report and data"));
        setSubTitle(BrReportWizard::tr("Name the report and choose where its records come from."));

        m_title = new QLineEdit(BrReportWizard::tr("New report"), this);
        m_title->setObjectName(QStringLiteral("title"));
        m_source = new QComboBox(this);
        m_source->setObjectName(QStringLiteral("source"));
        for (auto it = dataSources.begin(); it != dataSources.end(); ++it)
            m_source->addItem(it.key(), it.key());

        auto *form = new QFormLayout;
        form->addRow(BrReportWizard::tr("Title"), m_title);
        form->addRow(BrReportWizard::tr("Data"), m_source);
        auto *layout = new QVBoxLayout(this);
        layout->addLayout(form);

#ifdef BRREPORT_HAS_SQL
        m_source->addItem(BrReportWizard::tr("SQL query"));
        m_sqlBox = new QWidget(this);
        m_connection = new QComboBox(m_sqlBox);
        m_connection->setObjectName(QStringLiteral("connection"));
        m_connection->setEditable(true);
        m_connection->addItem(QString());
        for (const QString &c : QSqlDatabase::connectionNames())
            if (c != QLatin1String(QSqlDatabase::defaultConnection))
                m_connection->addItem(c);
        m_connection->setToolTip(BrReportWizard::tr("Empty = the application's default database connection"));
        m_sql = new QPlainTextEdit(m_sqlBox);
        m_sql->setObjectName(QStringLiteral("sql"));
        m_sql->setFont(QFont(QStringLiteral("monospace")));
        m_sql->setPlaceholderText(QStringLiteral("SELECT * FROM customers ORDER BY name"));
        auto *sqlForm = new QFormLayout(m_sqlBox);
        sqlForm->setContentsMargins(0, 0, 0, 0);
        sqlForm->addRow(BrReportWizard::tr("Connection"), m_connection);
        sqlForm->addRow(BrReportWizard::tr("SQL"), m_sql);
        layout->addWidget(m_sqlBox, 1);
        connect(m_source, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
            m_sqlBox->setVisible(m_source->currentData().isNull());
        });
        m_sqlBox->setVisible(m_source->currentData().isNull());
#endif

        m_error = new QLabel(this);
        m_error->setObjectName(QStringLiteral("error"));
        m_error->setWordWrap(true);
        m_error->setTextFormat(Qt::PlainText);
        m_error->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_error->hide();
        layout->addWidget(m_error);
        layout->addStretch();
    }

    bool validatePage() override
    {
        QString error;
        m_query.clear();
        m_fields.clear();
        const QVariant name = m_source->currentData();
        if (name.isNull())
            readQuery(&error);
        else
            m_fields = BrReportWizard::inspect(m_dataSources.value(name.toString()));
        if (error.isEmpty() && m_fields.isEmpty())
            error = BrReportWizard::tr("No fields were found in this data.");
        m_error->setText(error);
        m_error->setVisible(!error.isEmpty());
        return error.isEmpty();
    }

    QString reportTitle() const { return m_title->text().trimmed(); }
    QString dataSourceName() const
    {
        return m_query.isEmpty() ? m_source->currentData().toString()
                                 : m_query.value(QStringLiteral("name")).toString();
    }
    QVariantMap query() const { return m_query; }
    QList<Column> fields() const { return m_fields; }

private:
    void readQuery(QString *error)
    {
#ifdef BRREPORT_HAS_SQL
        const QString sql = m_sql->toPlainText().trimmed();
        if (sql.isEmpty()) {
            *error = BrReportWizard::tr("Type the SQL query.");
            return;
        }
        // The name is not shown here; the user can change it later, in Data > Edit query.
        QString name = QStringLiteral("query");
        for (int i = 2; m_dataSources.contains(name); ++i)
            name = QStringLiteral("query%1").arg(i);
        QVariantMap query{{QStringLiteral("name"), name}, {QStringLiteral("sql"), sql}};
        if (!m_connection->currentText().isEmpty())
            query.insert(QStringLiteral("connection"), m_connection->currentText());

        BrReport probe;
        probe.setQueries({query});
        if (!probe.openQueries()) {
            *error = probe.errorString();
            return;
        }
        m_fields = BrReportWizard::inspect(probe.findDataSource(name));
        m_query = query;
#else
        Q_UNUSED(error)
#endif
    }

    QMap<QString, BrDataSource *> m_dataSources;
    QLineEdit *m_title = nullptr;
    QComboBox *m_source = nullptr;
    QWidget *m_sqlBox = nullptr;
    QComboBox *m_connection = nullptr;
    QPlainTextEdit *m_sql = nullptr;
    QLabel *m_error = nullptr;
    QVariantMap m_query;
    QList<Column> m_fields;
};

// ---------------------------------------------------------------------------

class BrReportWizard::FieldsPage : public QWizardPage
{
public:
    explicit FieldsPage(DataPage *dataPage) : m_dataPage(dataPage)
    {
        setTitle(BrReportWizard::tr("Fields"));
        setSubTitle(BrReportWizard::tr("Check the fields to print, from left to right, and the totals you want."));

        m_table = new QTableWidget(0, 3, this);
        m_table->setHorizontalHeaderLabels({BrReportWizard::tr("Field"), BrReportWizard::tr("Caption"),
                                            BrReportWizard::tr("Total")});
        m_table->horizontalHeader()->setSectionResizeMode(CaptionColumn, QHeaderView::Stretch);
        m_table->verticalHeader()->hide();
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        connect(m_table, &QTableWidget::itemChanged, this, &QWizardPage::completeChanged);

        auto *buttons = new QVBoxLayout;
        auto addButton = [&](const QString &text, auto slot) {
            auto *button = new QPushButton(text, this);
            connect(button, &QPushButton::clicked, this, slot);
            buttons->addWidget(button);
        };
        addButton(BrReportWizard::tr("Move up"), [this]() { moveCurrentRow(-1); });
        addButton(BrReportWizard::tr("Move down"), [this]() { moveCurrentRow(1); });
        addButton(BrReportWizard::tr("Check all"), [this]() { checkAll(Qt::Checked); });
        addButton(BrReportWizard::tr("Uncheck all"), [this]() { checkAll(Qt::Unchecked); });
        buttons->addStretch();

        auto *layout = new QHBoxLayout(this);
        layout->addWidget(m_table, 1);
        layout->addLayout(buttons);
    }

    /*! Going back and forth keeps the choices, unless the data has other fields. */
    void initializePage() override
    {
        QStringList names;
        const QList<Column> fields = m_dataPage->fields();
        for (const Column &c : fields)
            names << c.field;
        if (names == m_names)
            return;
        m_names = names;
        const QSignalBlocker blocker(m_table); // rows are incomplete until the end of the loop
        m_table->setRowCount(fields.size());
        for (int row = 0; row < fields.size(); ++row) {
            const Column &c = fields.at(row);
            auto *field = new QTableWidgetItem(c.field);
            field->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            field->setCheckState(Qt::Checked);
            field->setData(KindRole, int(c.kind));
            field->setData(SizeRole, c.size);
            m_table->setItem(row, FieldColumn, field);
            m_table->setItem(row, CaptionColumn, new QTableWidgetItem(c.caption));
            setTotalCombo(row, -1);
        }
        m_table->resizeColumnToContents(FieldColumn);
        m_table->resizeColumnToContents(TotalColumn);
        emit completeChanged();
    }

    bool isComplete() const override { return !columns().isEmpty(); }

    QList<Column> columns() const
    {
        const QList<Column> fields = allFields();
        QList<Column> result;
        for (int row = 0; row < fields.size(); ++row)
            if (m_table->item(row, FieldColumn)->checkState() == Qt::Checked)
                result << fields.at(row);
        return result;
    }

    /*! Every field, checked or not, in the order of the table. */
    QList<Column> allFields() const
    {
        QList<Column> result;
        for (int row = 0; row < m_table->rowCount(); ++row) {
            const QTableWidgetItem *field = m_table->item(row, FieldColumn);
            Column c;
            c.field = field->text();
            c.caption = m_table->item(row, CaptionColumn)->text().trimmed();
            c.kind = FieldKind(field->data(KindRole).toInt());
            c.size = field->data(SizeRole).toInt();
            c.total = totalOf(row);
            result << c;
        }
        return result;
    }

private:
    int totalOf(int row) const
    {
        return static_cast<QComboBox *>(m_table->cellWidget(row, TotalColumn))->currentData().toInt();
    }

    /*! Only a count makes sense for text and dates; a field without values may be anything. */
    void setTotalCombo(int row, int total)
    {
        const auto kind = FieldKind(m_table->item(row, FieldColumn)->data(KindRole).toInt());
        auto *combo = new QComboBox(m_table);
        combo->addItem(BrReportWizard::tr("None"), -1);
        const QList<Br::ResultType> types = isNumber(kind) || kind == FieldKind::Unknown
                                                ? QList<Br::ResultType>{Br::ResultType::Sum, Br::ResultType::Count,
                                                                        Br::ResultType::Average, Br::ResultType::Min,
                                                                        Br::ResultType::Max}
                                                : QList<Br::ResultType>{Br::ResultType::Count};
        const QMetaEnum names = QMetaEnum::fromType<Br::ResultType>();
        for (Br::ResultType type : types)
            combo->addItem(BrPropertyEditor::labelFor(QString::fromLatin1(names.valueToKey(int(type)))), int(type));
        combo->setCurrentIndex(qMax(0, combo->findData(total)));
        m_table->setCellWidget(row, TotalColumn, combo);
    }

    void moveCurrentRow(int delta)
    {
        const int from = m_table->currentRow();
        const int to = from + delta;
        if (from < 0 || to < 0 || to >= m_table->rowCount())
            return;
        const int fromTotal = totalOf(from);
        const int toTotal = totalOf(to);
        const QSignalBlocker blocker(m_table); // the items are out of the table for a moment
        for (int column : {FieldColumn, CaptionColumn}) {
            QTableWidgetItem *a = m_table->takeItem(from, column);
            QTableWidgetItem *b = m_table->takeItem(to, column);
            m_table->setItem(from, column, b);
            m_table->setItem(to, column, a);
        }
        setTotalCombo(from, toTotal);
        setTotalCombo(to, fromTotal);
        m_table->setCurrentCell(to, m_table->currentColumn());
    }

    void checkAll(Qt::CheckState state)
    {
        for (int row = 0; row < m_table->rowCount(); ++row)
            m_table->item(row, FieldColumn)->setCheckState(state);
    }

    DataPage *m_dataPage;
    QTableWidget *m_table = nullptr;
    QStringList m_names;
};

// ---------------------------------------------------------------------------

class BrReportWizard::LayoutPage : public QWizardPage
{
public:
    explicit LayoutPage(FieldsPage *fieldsPage) : m_fieldsPage(fieldsPage)
    {
        setTitle(BrReportWizard::tr("Layout"));
        setSubTitle(BrReportWizard::tr("Everything can be adjusted in the designer afterwards."));

        m_group = new QComboBox(this);
        m_group->setObjectName(QStringLiteral("group"));
        auto *groupHint = new QLabel(BrReportWizard::tr("The data must be sorted by this field, and it leaves the "
                                                        "columns, because it is printed above each group."),
                                     this);
        groupHint->setWordWrap(true);
        m_landscape = new QCheckBox(BrReportWizard::tr("Landscape page"), this);
        m_landscape->setObjectName(QStringLiteral("landscape"));
        connect(m_landscape, &QCheckBox::clicked, this, [this]() { m_landscapeChosen = true; });
        m_zebra = new QCheckBox(BrReportWizard::tr("Alternate row colors"), this);
        m_zebra->setObjectName(QStringLiteral("zebra"));
        m_zebra->setChecked(true);

        auto *form = new QFormLayout(this);
        form->addRow(BrReportWizard::tr("Group by"), m_group);
        form->addRow(QString(), groupHint);
        form->addRow(QString(), m_landscape);
        form->addRow(QString(), m_zebra);
    }

    void initializePage() override
    {
        const QVariant current = m_group->currentData();
        m_group->clear();
        m_group->addItem(BrReportWizard::tr("(no group)"));
        for (const Column &c : m_fieldsPage->allFields())
            m_group->addItem(c.caption, c.field);
        m_group->setCurrentIndex(qMax(0, m_group->findData(current)));
        if (!m_landscapeChosen)
            m_landscape->setChecked(neededWidth(m_fieldsPage->columns()) > printableWidth(false));
    }

    void fill(Options *options) const
    {
        options->landscape = m_landscape->isChecked();
        options->zebra = m_zebra->isChecked();
        const QString group = m_group->currentData().toString();
        for (const Column &c : m_fieldsPage->allFields())
            if (c.field == group)
                options->group = c;
    }

private:
    FieldsPage *m_fieldsPage;
    QComboBox *m_group = nullptr;
    QCheckBox *m_landscape = nullptr;
    QCheckBox *m_zebra = nullptr;
    bool m_landscapeChosen = false;
};

// ---------------------------------------------------------------------------

BrReportWizard::BrReportWizard(const QMap<QString, BrDataSource *> &dataSources, QWidget *parent) : QWizard(parent)
{
    setWindowTitle(tr("New report wizard"));
    setOption(QWizard::NoBackButtonOnStartPage);
    resize(720, 540);
    m_dataPage = new DataPage(dataSources);
    m_fieldsPage = new FieldsPage(m_dataPage);
    m_layoutPage = new LayoutPage(m_fieldsPage);
    addPage(m_dataPage);
    addPage(m_fieldsPage);
    addPage(m_layoutPage);
}

BrReportWizard::Options BrReportWizard::options() const
{
    Options options;
    options.title = m_dataPage->reportTitle();
    options.dataSourceName = m_dataPage->dataSourceName();
    options.query = m_dataPage->query();
    options.columns = m_fieldsPage->columns();
    m_layoutPage->fill(&options);
    return options;
}

QList<BrReportWizard::Column> BrReportWizard::inspect(BrDataSource *dataSource)
{
    QList<Column> columns;
    if (!dataSource)
        return columns;
    bool hasRecord = dataSource->first();
    for (const QString &name : dataSource->fieldNames()) {
        Column c;
        c.field = name;
        c.caption = captionFor(name);
        columns << c;
    }
    for (int row = 0; hasRecord && row < SampleRows; ++row) {
        for (Column &c : columns) {
            const QVariant value = dataSource->value(c.field);
            if (!value.isValid() || value.isNull())
                continue;
            c.kind = mergedKind(c.kind, kindOf(value));
            const QString format = c.kind == FieldKind::Decimal ? QString::fromLatin1(DecimalFormat) : QString();
            c.size = qMax(c.size, int(BrFormat::format(value, format).size()));
        }
        hasRecord = dataSource->next();
    }
    dataSource->first();
    return columns;
}

QString BrReportWizard::captionFor(const QString &fieldName)
{
    QString words;
    const bool mixedCase = fieldName != fieldName.toUpper();
    for (int i = 0; i < fieldName.size(); ++i) {
        const QChar c = fieldName.at(i);
        if (mixedCase && i > 0 && c.isUpper() && fieldName.at(i - 1).isLower())
            words += QLatin1Char(' ');
        words += c == QLatin1Char('_') ? QLatin1Char(' ') : c.toLower();
    }
    words = words.simplified();
    if (!words.isEmpty())
        words[0] = words.at(0).toUpper();
    return words;
}

QByteArray BrReportWizard::buildLayout(const Options &o)
{
    QList<Column> columns;
    for (const Column &c : o.columns)
        if (c.field != o.group.field)
            columns << c;
    if (columns.isEmpty()) // grouped by the only column: nothing else to show in the detail
        columns = o.columns;

    const double width = printableWidth(o.landscape);
    const QList<double> edges = columnEdges(columns, width);

    // Page header
    const double headerHeight = 12;
    QJsonObject title{{QStringLiteral("type"), QStringLiteral("SystemInfo")},
                      {QStringLiteral("info"), QStringLiteral("Title")},
                      {QStringLiteral("geometry"), QJsonArray{0, 0, width - 30, headerHeight - 2}},
                      {QStringLiteral("font"), QJsonObject{{QStringLiteral("family"), QStringLiteral("Arial")},
                                                           {QStringLiteral("size"), 14},
                                                           {QStringLiteral("bold"), true}}}};
    QJsonObject date{{QStringLiteral("type"), QStringLiteral("SystemInfo")},
                     {QStringLiteral("info"), QStringLiteral("Date")},
                     {QStringLiteral("alignment"), QStringLiteral("Right")},
                     {QStringLiteral("geometry"), QJsonArray{width - 30, 0, 30, headerHeight - 2}}};
    QJsonObject header = band("Header", headerHeight, {title, date});

    // Column headers and detail
    const double captionHeight = 7;
    const double rowHeight = 6;
    QJsonArray captions;
    QJsonArray values;
    for (int i = 0; i < columns.size(); ++i) {
        const Column &column = columns.at(i);
        const double x = edges.at(i);
        const double w = edges.at(i + 1) - x;
        QJsonObject caption = text("Label", x, w, captionHeight, alignmentOf(column));
        caption.insert(QStringLiteral("text"), column.caption);
        caption.insert(QStringLiteral("color"), QStringLiteral("#ffffff"));
        caption.insert(QStringLiteral("font"), boldFont());
        captions << caption;

        QJsonObject value = text("DBText", x, w, rowHeight, alignmentOf(column));
        value.insert(QStringLiteral("dataField"), column.field);
        if (column.kind == FieldKind::Decimal)
            value.insert(QStringLiteral("displayFormat"), QString::fromLatin1(DecimalFormat));
        if (isNumber(column.kind))
            value.insert(QStringLiteral("shrinkToFit"), true); // a cut number would be a wrong number
        values << value;
    }
    QJsonObject columnHeader = band("ColumnHeader", captionHeight, captions);
    columnHeader.insert(QStringLiteral("background"), QStringLiteral("#1f4e79"));
    QJsonObject detail = band("Detail", rowHeight, values);
    if (o.zebra)
        detail.insert(QStringLiteral("alternateBackground"), QStringLiteral("#eef2f8"));

    // Records and totals
    const bool grouped = !o.group.field.isEmpty();
    QJsonArray bands{header, columnHeader};
    if (grouped) {
        QJsonObject groupValue = text("DBText", 0, width, captionHeight);
        groupValue.insert(QStringLiteral("dataField"), o.group.field);
        groupValue.insert(QStringLiteral("prefix"), o.group.caption + QStringLiteral(": "));
        groupValue.insert(QStringLiteral("font"), boldFont());
        if (o.group.kind == FieldKind::Decimal)
            groupValue.insert(QStringLiteral("displayFormat"), QString::fromLatin1(DecimalFormat));
        QJsonObject groupHeader = band("Header", captionHeight, {groupValue});
        groupHeader.insert(QStringLiteral("background"), QStringLiteral("#dce6f2"));

        QJsonArray groupBands{groupHeader, detail};
        const QJsonObject groupTotals = totalsBand(columns, edges, tr("Total"));
        if (!groupTotals.isEmpty())
            groupBands << groupTotals;
        bands << QJsonObject{{QStringLiteral("type"), QStringLiteral("Group")},
                             {QStringLiteral("dataFields"), o.group.field},
                             {QStringLiteral("keepHeaderWithDetail"), true},
                             {QStringLiteral("children"), groupBands}};
    } else {
        bands << detail;
    }
    const QJsonObject totals = totalsBand(columns, edges, grouped ? tr("Grand total") : tr("Total"));
    if (!totals.isEmpty())
        bands << totals;

    // Page footer
    const double footerHeight = 6;
    bands << band("Footer", footerHeight,
                  {QJsonObject{{QStringLiteral("type"), QStringLiteral("SystemInfo")},
                               {QStringLiteral("info"), QStringLiteral("PagePreview")},
                               {QStringLiteral("format"), tr("Page %1 of %2")},
                               {QStringLiteral("alignment"), QStringLiteral("Right")},
                               {QStringLiteral("geometry"), QJsonArray{width - 60, 0, 60, footerHeight}}}});

    QJsonObject report{{QStringLiteral("title"), o.title},
                       {QStringLiteral("landscape"), o.landscape},
                       {QStringLiteral("dataSourceName"), o.dataSourceName},
                       {QStringLiteral("font"), QJsonObject{{QStringLiteral("family"), QStringLiteral("Arial")},
                                                            {QStringLiteral("size"), 9}}},
                       {QStringLiteral("children"), bands}};
    if (!o.query.isEmpty())
        report.insert(QStringLiteral("queries"), QJsonArray{QJsonObject::fromVariantMap(o.query)});

    return QJsonDocument(QJsonObject{{QStringLiteral("format"), QStringLiteral("BrReport")},
                                     {QStringLiteral("version"), 1},
                                     {QStringLiteral("report"), report}})
        .toJson();
}
