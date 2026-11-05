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

#include "brparameterseditor_p.h"

#include "brreport.h"

#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

#ifdef BRREPORT_HAS_SQL
#include <QFormLayout>
#include <QSqlDatabase>
#endif

namespace {

enum Column { NameColumn, CaptionColumn, TypeColumn, DefaultColumn, ChoicesColumn, ColumnCount };

// The choices cell keeps the query that fills the list, if any.
constexpr int SqlRole = Qt::UserRole;
constexpr int ConnectionRole = Qt::UserRole + 1;

// "1=Active; 2=Inactive; Other": value=caption pairs or plain values
QString choicesToText(const QVariantList &choices)
{
    QStringList parts;
    for (const QVariant &choice : choices) {
        const QVariantMap map = choice.toMap();
        if (map.isEmpty()) {
            parts << choice.toString();
            continue;
        }
        const QString value = map.value(QStringLiteral("value")).toString();
        const QString caption = map.value(QStringLiteral("caption")).toString();
        parts << (caption.isEmpty() ? value : value + QLatin1Char('=') + caption);
    }
    return parts.join(QStringLiteral("; "));
}

QVariantList choicesFromText(const QString &text)
{
    QVariantList choices;
    const QStringList parts = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const int equals = part.indexOf(QLatin1Char('='));
        if (equals < 0) {
            if (!part.trimmed().isEmpty())
                choices << part.trimmed();
            continue;
        }
        choices << QVariantMap{{QStringLiteral("value"), part.left(equals).trimmed()},
                               {QStringLiteral("caption"), part.mid(equals + 1).trimmed()}};
    }
    return choices;
}

} // namespace

BrParametersEditor::BrParametersEditor(const QVariantList &parameters, const BrReport *report, QWidget *parent)
    : QDialog(parent), m_report(report)
{
    setWindowTitle(tr("Report parameters"));
    resize(760, 360);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Caption"), tr("Type"), tr("Default value"), tr("Choices")});
    m_table->horizontalHeader()->setSectionResizeMode(ChoicesColumn, QHeaderView::Stretch);
    m_table->horizontalHeaderItem(DefaultColumn)->setToolTip(tr("Dates: yyyy-mm-dd or \"today\". Yes/No: true or false."));
    m_table->horizontalHeaderItem(ChoicesColumn)->setToolTip(tr("Optional, separated by ';': value or value=caption."));
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    for (const QVariant &p : parameters)
        addRow(p.toMap());

    auto *add = new QPushButton(tr("Add"), this);
    auto *remove = new QPushButton(tr("Remove"), this);
    auto *up = new QPushButton(tr("Move up"), this);
    auto *down = new QPushButton(tr("Move down"), this);
    connect(add, &QPushButton::clicked, this, [this]() {
        addRow({});
        m_table->setCurrentCell(m_table->rowCount() - 1, NameColumn);
        m_table->editItem(m_table->currentItem());
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        if (m_table->currentRow() >= 0)
            m_table->removeRow(m_table->currentRow());
    });
    connect(up, &QPushButton::clicked, this, [this]() { moveCurrentRow(-1); });
    connect(down, &QPushButton::clicked, this, [this]() { moveCurrentRow(1); });
    auto *rowButtons = new QHBoxLayout;
    rowButtons->addWidget(add);
    rowButtons->addWidget(remove);
    rowButtons->addWidget(up);
    rowButtons->addWidget(down);
#ifdef BRREPORT_HAS_SQL
    auto *query = new QPushButton(tr("Choices from SQL..."), this);
    connect(query, &QPushButton::clicked, this, &BrParametersEditor::editChoicesQuery);
    rowButtons->addWidget(query);
#endif
    rowButtons->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &BrParametersEditor::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *hint = new QLabel(tr("The user is asked for these values before the report runs. "
                               "Use them as :name in SQL queries and as name in formulas."), this);
    hint->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(m_table, 1);
    layout->addLayout(rowButtons);
    layout->addWidget(buttons);
}

QVariantMap BrParametersEditor::rowValues(int row) const
{
    auto text = [this, row](int column) { return m_table->item(row, column)->text().trimmed(); };
    QVariantMap p{{QStringLiteral("name"), text(NameColumn)},
                  {QStringLiteral("type"), qobject_cast<QComboBox *>(m_table->cellWidget(row, TypeColumn))->currentData()}};
    if (!text(CaptionColumn).isEmpty())
        p.insert(QStringLiteral("caption"), text(CaptionColumn));
    if (!text(DefaultColumn).isEmpty())
        p.insert(QStringLiteral("default"), text(DefaultColumn));
    const QTableWidgetItem *choicesItem = m_table->item(row, ChoicesColumn);
    const QString sql = choicesItem->data(SqlRole).toString();
    if (!sql.isEmpty()) {
        p.insert(QStringLiteral("choicesSql"), sql);
        const QString connection = choicesItem->data(ConnectionRole).toString();
        if (!connection.isEmpty())
            p.insert(QStringLiteral("choicesConnection"), connection);
        return p;
    }
    const QVariantList choices = choicesFromText(text(ChoicesColumn));
    if (!choices.isEmpty())
        p.insert(QStringLiteral("choices"), choices);
    return p;
}

void BrParametersEditor::setRowValues(int row, const QVariantMap &parameter)
{
    m_table->item(row, NameColumn)->setText(parameter.value(QStringLiteral("name")).toString());
    m_table->item(row, CaptionColumn)->setText(parameter.value(QStringLiteral("caption")).toString());
    m_table->item(row, DefaultColumn)->setText(parameter.value(QStringLiteral("default")).toString());
    m_table->item(row, ChoicesColumn)->setText(choicesToText(parameter.value(QStringLiteral("choices")).toList()));
    setChoicesQuery(row, parameter.value(QStringLiteral("choicesSql")).toString(),
                    parameter.value(QStringLiteral("choicesConnection")).toString());
    auto *type = qobject_cast<QComboBox *>(m_table->cellWidget(row, TypeColumn));
    type->setCurrentIndex(qMax(0, type->findData(parameter.value(QStringLiteral("type")).toString().toLower())));
}

void BrParametersEditor::setChoicesQuery(int row, const QString &sql, const QString &connection)
{
    QTableWidgetItem *item = m_table->item(row, ChoicesColumn);
    const bool hadQuery = !item->data(SqlRole).toString().isEmpty();
    item->setData(SqlRole, sql);
    item->setData(ConnectionRole, connection);
    if (sql.isEmpty()) {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if (hadQuery)
            item->setText(QString());
    } else {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setText(tr("SQL: %1").arg(sql.simplified()));
    }
}

void BrParametersEditor::editChoicesQuery()
{
#ifdef BRREPORT_HAS_SQL
    const int row = m_table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, windowTitle(), tr("Select a parameter first."));
        return;
    }
    const QTableWidgetItem *item = m_table->item(row, ChoicesColumn);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Choices from SQL"));
    auto *connection = new QComboBox(&dialog);
    connection->setEditable(true);
    connection->addItem(QString());
    for (const QString &c : QSqlDatabase::connectionNames())
        if (c != QLatin1String(QSqlDatabase::defaultConnection))
            connection->addItem(c);
    connection->setCurrentText(item->data(ConnectionRole).toString());
    connection->setToolTip(tr("Empty = the application's default database connection"));
    auto *sql = new QPlainTextEdit(item->data(SqlRole).toString(), &dialog);
    sql->setFont(QFont(QStringLiteral("monospace")));
    sql->setPlaceholderText(QStringLiteral("SELECT id, name FROM customers ORDER BY name"));
    auto *test = new QPushButton(tr("Test"), &dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->addButton(test, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(test, &QPushButton::clicked, &dialog, [&]() {
        QString error;
        const QVariantList choices = m_report->parameterChoices(
            {{QStringLiteral("choicesSql"), sql->toPlainText()},
             {QStringLiteral("choicesConnection"), connection->currentText()}}, &error);
        QMessageBox::information(&dialog, tr("Test query"),
                                 error.isEmpty() ? tr("OK: %1 choice(s).").arg(choices.size()) : error);
    });

    auto *form = new QFormLayout;
    form->addRow(tr("Connection"), connection);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(new QLabel(tr("The first column is the value and the second, optional, is the caption. "
                                    "Leave it empty to remove the query."), &dialog));
    layout->addWidget(sql, 1);
    layout->addWidget(buttons);
    dialog.resize(560, 320);
    if (dialog.exec() == QDialog::Accepted)
        setChoicesQuery(row, sql->toPlainText().trimmed(), connection->currentText());
#endif
}

void BrParametersEditor::addRow(const QVariantMap &parameter)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    for (int column : {NameColumn, CaptionColumn, DefaultColumn, ChoicesColumn})
        m_table->setItem(row, column, new QTableWidgetItem);
    auto *type = new QComboBox(m_table);
    type->addItem(tr("Text"), QStringLiteral("string"));
    type->addItem(tr("Integer"), QStringLiteral("integer"));
    type->addItem(tr("Number"), QStringLiteral("number"));
    type->addItem(tr("Date"), QStringLiteral("date"));
    type->addItem(tr("Yes/No"), QStringLiteral("boolean"));
    m_table->setCellWidget(row, TypeColumn, type);
    setRowValues(row, parameter);
}

void BrParametersEditor::moveCurrentRow(int delta)
{
    const int row = m_table->currentRow();
    const int other = row + delta;
    if (row < 0 || other < 0 || other >= m_table->rowCount())
        return;
    const QVariantMap a = rowValues(row);
    const QVariantMap b = rowValues(other);
    setRowValues(row, b);
    setRowValues(other, a);
    m_table->setCurrentCell(other, m_table->currentColumn());
}

QVariantList BrParametersEditor::parameters() const
{
    QVariantList result;
    for (int row = 0; row < m_table->rowCount(); ++row)
        result << rowValues(row);
    return result;
}

QString BrParametersEditor::validate() const
{
    static const QRegularExpression identifier(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    QSet<QString> names;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QVariantMap p = rowValues(row);
        const QString name = p.value(QStringLiteral("name")).toString();
        if (!identifier.match(name).hasMatch())
            return tr("Line %1: the name must start with a letter and have only letters, digits and '_'.").arg(row + 1);
        if (names.contains(name.toLower()))
            return tr("The name '%1' is used more than once.").arg(name);
        names.insert(name.toLower());

        const QString type = p.value(QStringLiteral("type")).toString();
        const QString value = p.value(QStringLiteral("default")).toString();
        if (value.isEmpty())
            continue;
        bool ok = true;
        if (type == QLatin1String("date"))
            ok = value.compare(QLatin1String("today"), Qt::CaseInsensitive) == 0
                 || QDate::fromString(value, Qt::ISODate).isValid();
        else if (type == QLatin1String("integer"))
            value.toLongLong(&ok);
        else if (type == QLatin1String("number"))
            value.toDouble(&ok);
        else if (type == QLatin1String("boolean"))
            ok = value == QLatin1String("true") || value == QLatin1String("false");
        if (!ok)
            return tr("'%1' is not a valid default value for the parameter '%2'.").arg(value, name);
    }
    return {};
}

void BrParametersEditor::accept()
{
    const QString error = validate();
    if (!error.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), error);
        return;
    }
    QDialog::accept();
}
