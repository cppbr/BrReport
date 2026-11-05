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

#include "brexpressioneditor_p.h"

#include "brpropertyeditor_p.h"

#include "brdatasource.h"
#include "brexpression.h"
#include "brpager.h"
#include "brreport.h"

#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBlock>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

enum ItemRole { NameRole = Qt::UserRole, FunctionRole, ArgumentsRole, DescriptionRole };

BrPager *pagerOf(QObject *object)
{
    for (QObject *o = object; o; o = o->parent())
        if (auto *pager = qobject_cast<BrPager *>(o))
            return pager;
    return nullptr;
}

/*! Fields of every data source open while \a object prints, inner sources included. */
QStringList fieldsFor(QObject *object)
{
    QStringList fields;
    for (BrPager *p = pagerOf(object); p; p = p->parentPager()) {
        BrDataSource *ds = nullptr;
        if (auto *sub = qobject_cast<BrSubDetail *>(p))
            ds = sub->dataSource();
        else if (auto *report = qobject_cast<BrReport *>(p))
            ds = report->dataSource();
        if (ds) {
            ds->first();
            fields += ds->fieldNames();
        }
    }
    fields.removeDuplicates();
    fields.sort(Qt::CaseInsensitive);
    return fields;
}

BrReport *reportOf(QObject *object)
{
    for (QObject *o = object; o; o = o->parent())
        if (auto *report = qobject_cast<BrReport *>(o))
            return report;
    return nullptr;
}

QTreeWidgetItem *addGroup(QTreeWidget *tree, const QString &title, bool expanded)
{
    auto *group = new QTreeWidgetItem(tree);
    group->setText(0, title);
    group->setFlags(Qt::ItemIsEnabled);
    group->setExpanded(expanded);
    return group;
}

} // namespace

QList<BrExpressionEditor::Function> BrExpressionEditor::functionCatalog()
{
    const QString text = tr("Text functions");
    const QString numbers = tr("Number functions");
    const QString dates = tr("Date functions");
    const QString logic = tr("Logic functions");
    const QString report = tr("Report functions");
    return {
        {text, QStringLiteral("upper"), tr("upper(text)"), tr("Converts the text to upper case.")},
        {text, QStringLiteral("lower"), tr("lower(text)"), tr("Converts the text to lower case.")},
        {text, QStringLiteral("trim"), tr("trim(text)"), tr("Removes the spaces at both ends.")},
        {text, QStringLiteral("len"), tr("len(text)"), tr("Number of characters of the text.")},
        {text, QStringLiteral("left"), tr("left(text, count)"), tr("The first characters of the text.")},
        {text, QStringLiteral("right"), tr("right(text, count)"), tr("The last characters of the text.")},
        {text, QStringLiteral("substr"), tr("substr(text, start, count)"), tr("Part of the text. The first character is number 1.")},
        {text, QStringLiteral("replace"), tr("replace(text, old, new)"), tr("Replaces every occurrence of a text by another.")},
        {text, QStringLiteral("padleft"), tr("padleft(text, size, character)"), tr("Fills the left side up to the size: padleft(code, 6, '0').")},
        {text, QStringLiteral("padright"), tr("padright(text, size, character)"), tr("Fills the right side up to the size.")},
        {text, QStringLiteral("str"), tr("str(value)"), tr("Converts a value to text.")},
        {numbers, QStringLiteral("round"), tr("round(value, decimals)"), tr("Rounds the value.")},
        {numbers, QStringLiteral("trunc"), tr("trunc(value)"), tr("Removes the decimals.")},
        {numbers, QStringLiteral("abs"), tr("abs(value)"), tr("The value without its sign.")},
        {numbers, QStringLiteral("min"), tr("min(a, b, ...)"), tr("The lowest of the values.")},
        {numbers, QStringLiteral("max"), tr("max(a, b, ...)"), tr("The highest of the values.")},
        {numbers, QStringLiteral("num"), tr("num(text)"), tr("Converts a text to a number.")},
        {numbers, QStringLiteral("formatnumber"), tr("formatnumber(value, mask)"), tr("Formats a number: formatnumber(price, '#,##0.00').")},
        {dates, QStringLiteral("today"), tr("today()"), tr("The current date.")},
        {dates, QStringLiteral("now"), tr("now()"), tr("The current date and time.")},
        {dates, QStringLiteral("year"), tr("year(date)"), tr("The year of a date.")},
        {dates, QStringLiteral("month"), tr("month(date)"), tr("The month of a date (1 to 12).")},
        {dates, QStringLiteral("day"), tr("day(date)"), tr("The day of a date.")},
        {dates, QStringLiteral("formatdate"), tr("formatdate(date, mask)"), tr("Formats a date: formatdate(issued, 'dd/MM/yyyy').")},
        {logic, QStringLiteral("if"), tr("if(condition, value if true, value if false)"), tr("Chooses a value: if(total > 100, 'High', 'Low').")},
        {logic, QStringLiteral("isnull"), tr("isnull(value)"), tr("True if the value is null or empty.")},
        {logic, QStringLiteral("coalesce"), tr("coalesce(a, b, ...)"), tr("The first value that is not empty.")},
        {report, QStringLiteral("page"), tr("page()"), tr("The current page number.")},
        {report, QStringLiteral("pages"), tr("pages()"), tr("The total number of pages.")},
        {report, QStringLiteral("documentpage"), tr("documentpage()"), tr("The page number in the whole document.")},
        {report, QStringLiteral("documentpages"), tr("documentpages()"), tr("The total number of pages of the whole document.")},
        {report, QStringLiteral("recno"), tr("recno()"), tr("The number of the current record.")},
        {report, QStringLiteral("detailcount"), tr("detailcount()"), tr("How many details were printed.")},
        {report, QStringLiteral("title"), tr("title()"), tr("The title of the report.")},
        {report, QStringLiteral("reportdate"), tr("reportdate()"), tr("The date and time the report was generated.")},
        {report, QStringLiteral("var"), tr("var(name)"), tr("The value of a report variable: var('company').")},
        {report, QStringLiteral("field"), tr("field(name)"), tr("The value of a field: field('name').")},
    };
}

QStringList BrExpressionEditor::knownFunctions()
{
    QStringList names = {QStringLiteral("iif"),     QStringLiteral("length"),
                         QStringLiteral("copy"),    QStringLiteral("int"),
                         QStringLiteral("isempty"), QStringLiteral("nvl"),
                         QStringLiteral("formatdatetime"), QStringLiteral("pagecount"),
                         QStringLiteral("variable")};
    const QList<Function> catalog = functionCatalog();
    for (const Function &f : catalog)
        names << f.name;
    return names;
}

QString BrExpressionEditor::quotedName(const QString &name)
{
    const QString lower = name.toLower();
    bool plain = !name.isEmpty() && (name.at(0).isLetter() || name.at(0) == QLatin1Char('_'))
                 && lower != QLatin1String("and") && lower != QLatin1String("or")
                 && lower != QLatin1String("not");
    for (const QChar c : name)
        plain = plain && (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.'));
    return plain ? name : QStringLiteral("[%1]").arg(name);
}

BrExpressionEditor::BrExpressionEditor(QObject *object, const QByteArray &property,
                                       const QString &text, QWidget *parent)
    : QDialog(parent), m_object(object), m_report(reportOf(object))
{
    setWindowTitle(BrPropertyEditor::labelFor(QString::fromLatin1(property)));

    QString hint;
    if (property == "printIf")
        hint = tr("The object is printed only when this formula is true. Leave it empty to always print.");
    else if (property == "highlightIf")
        hint = tr("The highlight is applied when this formula is true.");
    else if (qobject_cast<BrPager *>(object))
        hint = tr("A new group starts when the value of this formula changes.");
    else
        hint = tr("The result of this formula is printed.");
    auto *hintLabel = new QLabel(hint, this);
    hintLabel->setWordWrap(true);

    m_edit = new QPlainTextEdit(this);
    m_edit->setPlainText(text);
    m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_edit->setTabChangesFocus(true);
    m_edit->setMaximumHeight(110);

    auto *operators = new QHBoxLayout;
    operators->setSpacing(2);
    const QStringList ops = {QStringLiteral("+"),  QStringLiteral("-"),  QStringLiteral("*"),
                             QStringLiteral("/"),  QStringLiteral("%"),  QStringLiteral("&"),
                             QStringLiteral("="),  QStringLiteral("<>"), QStringLiteral("<"),
                             QStringLiteral(">"),  QStringLiteral("<="), QStringLiteral(">="),
                             QStringLiteral("("),  QStringLiteral(")"),  QStringLiteral("and"),
                             QStringLiteral("or"), QStringLiteral("not")};
    for (const QString &op : ops) {
        auto *button = new QToolButton(this);
        button->setText(QString(op).replace(QLatin1Char('&'), QStringLiteral("&&")));
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QToolButton::clicked, this, [this, op]() { insertOperator(op); });
        operators->addWidget(button);
    }
    operators->addStretch();

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setToolTip(tr("Double-click to insert into the formula"));
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *item) { activateItem(item); });
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &BrExpressionEditor::showDescription);

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setMinimumHeight(m_description->fontMetrics().lineSpacing() * 2);
    m_description->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(hintLabel);
    layout->addWidget(m_edit);
    layout->addLayout(operators);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_description);
    layout->addWidget(m_status);
    layout->addWidget(m_buttons);
    resize(560, 560);

    buildTree();
    connect(m_edit, &QPlainTextEdit::textChanged, this, &BrExpressionEditor::validate);
    validate();
    m_edit->moveCursor(QTextCursor::End);
    m_edit->setFocus();
}

QString BrExpressionEditor::formula() const
{
    QString text = m_edit->toPlainText();
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return text.trimmed();
}

void BrExpressionEditor::buildTree()
{
    auto addNames = [this](const QString &title, const QStringList &names) {
        if (names.isEmpty())
            return;
        QTreeWidgetItem *group = addGroup(m_tree, title, true);
        for (const QString &name : names) {
            auto *item = new QTreeWidgetItem(group);
            item->setText(0, name);
            item->setData(0, NameRole, name);
        }
        for (const QString &name : names)
            m_names.insert(name.toLower());
    };

    const QStringList fields = fieldsFor(m_object);
    m_hasFields = !fields.isEmpty();
    addNames(tr("Fields"), fields);

    if (m_report) {
        QStringList parameters;
        const QVariantList declared = m_report->parameters();
        for (const QVariant &p : declared)
            parameters << p.toMap().value(QStringLiteral("name")).toString();
        addNames(tr("Parameters"), parameters);
        QStringList variables = m_report->variables().keys();
        for (const QString &name : parameters)
            variables.removeAll(name);
        addNames(tr("Variables"), variables);
    }

    QTreeWidgetItem *group = nullptr;
    QString category;
    const QList<Function> catalog = functionCatalog();
    for (const Function &f : catalog) {
        if (f.category != category) {
            category = f.category;
            group = addGroup(m_tree, category, false);
        }
        auto *item = new QTreeWidgetItem(group);
        item->setText(0, f.signature);
        item->setData(0, FunctionRole, f.name);
        item->setData(0, ArgumentsRole, !f.signature.endsWith(QLatin1String("()")));
        item->setData(0, DescriptionRole, f.description);
    }
    if (m_report && !m_report->functionNames().isEmpty()) {
        group = addGroup(m_tree, tr("Application functions"), false);
        const QStringList names = m_report->functionNames();
        for (const QString &name : names) {
            auto *item = new QTreeWidgetItem(group);
            item->setText(0, name + QStringLiteral("()"));
            item->setData(0, FunctionRole, name);
            item->setData(0, ArgumentsRole, true);
        }
    }
}

bool BrExpressionEditor::isKnownName(const QString &name) const
{
    if (m_names.contains(name.toLower())) // the engine ignores the case of field names
        return true;
    // Qualified name: "<data source>.<field>"
    const int dot = name.indexOf(QLatin1Char('.'));
    BrDataSource *ds = dot > 0 && m_report ? m_report->findDataSource(name.left(dot)) : nullptr;
    if (!ds)
        return false;
    ds->first();
    return ds->hasField(name.mid(dot + 1));
}

QStringList BrExpressionEditor::unknownNames() const
{
    const BrExpression expression(formula());
    QStringList unknown;
    if (m_hasFields) { // without a data source the fields cannot be checked
        const QStringList names = expression.identifiers();
        for (const QString &name : names)
            if (!isKnownName(name))
                unknown << name;
    }
    const QStringList known = knownFunctions();
    const QStringList functions = expression.functions();
    for (const QString &name : functions)
        if (!known.contains(name) && !(m_report && m_report->function(name)))
            unknown << name + QStringLiteral("()");
    return unknown;
}

void BrExpressionEditor::validate()
{
    static const char *const errors[][2] = {
        {"invalid number", QT_TR_NOOP("Invalid number.")},
        {"unterminated string", QT_TR_NOOP("A text is missing its closing quote.")},
        {"missing ']'", QT_TR_NOOP("A name is missing its closing ']'.")},
        {"unexpected character", QT_TR_NOOP("The formula has an invalid character.")},
        {"unexpected token", QT_TR_NOOP("The formula is incomplete or has something out of place.")},
        {"missing ')'", QT_TR_NOOP("A ')' is missing.")},
        {"missing ')' in function call", QT_TR_NOOP("A function is missing its closing ')'.")},
    };

    const BrExpression expression(formula());
    QString message;
    QString color;
    if (!expression.isEmpty() && !expression.isValid()) {
        message = expression.errorString();
        for (const auto &error : errors)
            if (message == QLatin1String(error[0]))
                message = tr(error[1]);
        color = QStringLiteral("#c62828");
    } else if (!expression.isEmpty()) {
        const QStringList unknown = unknownNames();
        if (unknown.isEmpty()) {
            message = tr("The formula is correct.");
            color = QStringLiteral("#2e7d32");
        } else {
            message = tr("Not found: %1. It must exist when the report runs.")
                          .arg(unknown.join(QStringLiteral(", ")));
            color = QStringLiteral("#b26a00");
        }
    }
    m_status->setStyleSheet(color.isEmpty() ? QString() : QStringLiteral("color: %1;").arg(color));
    m_status->setText(message);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(expression.isEmpty() || expression.isValid());
}

void BrExpressionEditor::insertName(const QString &name)
{
    m_edit->insertPlainText(quotedName(name));
    m_edit->setFocus();
}

void BrExpressionEditor::insertFunction(const QString &name, bool hasArguments)
{
    // The selected text becomes the argument of the function.
    QTextCursor cursor = m_edit->textCursor();
    const QString selected = cursor.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char(' '));
    cursor.insertText(QStringLiteral("%1(%2)").arg(name, selected));
    if (hasArguments && selected.isEmpty())
        cursor.movePosition(QTextCursor::Left);
    m_edit->setTextCursor(cursor);
    m_edit->setFocus();
}

void BrExpressionEditor::insertOperator(const QString &op)
{
    QTextCursor cursor = m_edit->textCursor();
    QString text = op;
    if (op != QLatin1String("(") && op != QLatin1String(")")) {
        const QString before = cursor.block().text().left(cursor.positionInBlock());
        const bool needsSpace = !before.isEmpty() && !before.endsWith(QLatin1Char(' '))
                                && !before.endsWith(QLatin1Char('('));
        text = (needsSpace ? QStringLiteral(" ") : QString()) + op + QLatin1Char(' ');
    }
    cursor.insertText(text);
    m_edit->setTextCursor(cursor);
    m_edit->setFocus();
}

void BrExpressionEditor::activateItem(QTreeWidgetItem *item)
{
    if (item->data(0, FunctionRole).isValid())
        insertFunction(item->data(0, FunctionRole).toString(), item->data(0, ArgumentsRole).toBool());
    else if (item->data(0, NameRole).isValid())
        insertName(item->data(0, NameRole).toString());
}

void BrExpressionEditor::showDescription()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    const QString description = item ? item->data(0, DescriptionRole).toString() : QString();
    m_description->setText(description.isEmpty() ? QString()
                                                 : item->text(0) + QStringLiteral(": ") + description);
}
