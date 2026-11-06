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

#include "brpropertyeditor_p.h"

#include "brdesignerlabels_p.h"
#include "brexpressioneditor_p.h"

#include "brchart.h"
#include "brcontrol.h"
#include "brgraphics.h"
#include "brreport.h"
#include "brreportfile.h"
#include "brdatasource.h"
#include "brpager.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMarginsF>
#include <QMetaEnum>
#include <QPageSize>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

template<class SpinBox = QDoubleSpinBox>
SpinBox *mmSpin(double value, QWidget *parent, double min = -10000.0)
{
    auto *s = new SpinBox(parent);
    s->setRange(min, 10000.0);
    s->setDecimals(2);
    s->setSingleStep(0.5);
    s->setValue(value);
    s->setKeyboardTracking(false);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    s->setMinimumWidth(48);
    return s;
}

/*! Spin box that can be blank, meaning that the selected objects have different values. */
class BrMixedSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;

    void setMixed(double start)
    {
        m_start = start;
        setMinimum(minimum() - 0.01); // the lowest value shows as blank
        setSpecialValueText(QStringLiteral(" "));
        setValue(minimum());
    }
    bool isBlank() const { return !specialValueText().isEmpty() && value() == minimum(); }

    void stepBy(int steps) override
    {
        if (isBlank())
            setValue(m_start + steps * singleStep());
        else
            QDoubleSpinBox::stepBy(steps);
    }

private:
    double m_start = 0.0;
};

/*! The objects that have a position of their own (bands are stacked by the report). */
QList<BrControl *> positionedControls(const QList<QObject *> &objects)
{
    QList<BrControl *> controls;
    for (QObject *o : objects)
        if (auto *c = qobject_cast<BrControl *>(o))
            if (!c->inherits("BrBand"))
                controls << c;
    return controls;
}

double geometryValue(const QRectF &r, int index)
{
    const double values[] = {r.x(), r.y(), r.width(), r.height()};
    return values[index];
}

void setGeometryValue(QRectF &r, int index, double value)
{
    switch (index) {
    case 0: r.moveLeft(value); break;
    case 1: r.moveTop(value); break;
    case 2: r.setWidth(value); break;
    default: r.setHeight(value); break;
    }
}

QString colorText(const QColor &c)
{
    return c.isValid() ? c.name(c.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb) : QObject::tr("(none)");
}

void paintSwatch(QPushButton *button, const QColor &c)
{
    button->setText(colorText(c));
    QPixmap pm(14, 14);
    pm.fill(c.isValid() ? c : QColor(Qt::transparent));
    button->setIcon(QIcon(pm));
}

QString fontText(const QFont &f)
{
    QString s = QStringLiteral("%1 %2").arg(f.family()).arg(f.pointSizeF());
    if (f.bold())
        s += QObject::tr(" bold");
    if (f.italic())
        s += QObject::tr(" italic");
    return s;
}

/*! Multi-line text dialog; returns true if accepted. */
bool editText(QWidget *parent, const QString &title, QString &text, bool monospace = false)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto *edit = new QPlainTextEdit(&dialog);
    edit->setPlainText(text);
    if (monospace)
        edit->setFont(QFont(QStringLiteral("monospace")));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(edit);
    layout->addWidget(buttons);
    dialog.resize(520, 320);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    text = edit->toPlainText();
    return true;
}

// Properties edited elsewhere (Data menu) or not meant for end users.
const QSet<QByteArray> &hiddenProperties()
{
    static const QSet<QByteArray> hidden = {"queries", "parameters", "maxPages", "geometry",
                                              "verticalGuides", "horizontalGuides", "protection", "svg"};
    return hidden;
}

// Properties shown for \a object, most specific class first, without duplicates.
QList<QMetaProperty> listedProperties(const QObject *object)
{
    static const QSet<QByteArray> pagerOnly = {"pageBreak", "enabled", "keepHeaderWithDetail", "resetPageNumber"};
    QSet<QByteArray> seen;
    QList<QMetaProperty> props;
    const QMetaObject *mo = object->metaObject();
    for (int i = mo->propertyCount() - 1; i >= QObject::staticMetaObject.propertyCount(); --i) {
        const QMetaProperty p = mo->property(i);
        if (seen.contains(p.name()) || !p.isWritable() || hiddenProperties().contains(p.name()))
            continue;
        if (qobject_cast<const BrReport *>(object) && pagerOnly.contains(p.name()))
            continue;
        seen.insert(p.name());
        props.prepend(p);
    }
    return props;
}

/*!
 * Field names available to \a object: those of the data source of the pager it
 * belongs to (or of the parent pager, for master fields of a sub-report).
 */
QStringList fieldsFor(QObject *object, bool parentPager)
{
    BrPager *pager = nullptr;
    for (QObject *o = object; o && !pager; o = o->parent())
        pager = qobject_cast<BrPager *>(o);
    if (pager && parentPager)
        pager = pager->parentPager();
    if (auto *chart = qobject_cast<BrChart *>(object)) {
        BrReport *r = chart->report();
        BrDataSource *ds = r ? r->findDataSource(chart->dataSourceName()) : nullptr;
        if (ds) {
            ds->first();
            return ds->fieldNames();
        }
    }
    for (BrPager *p = pager; p; p = p->parentPager()) {
        BrDataSource *ds = nullptr;
        if (auto *sub = qobject_cast<BrSubDetail *>(p))
            ds = sub->dataSource();
        else if (auto *r = qobject_cast<BrReport *>(p))
            ds = r->dataSource();
        if (ds) {
            ds->first();
            QStringList fields = ds->fieldNames();
            fields.sort(Qt::CaseInsensitive);
            return fields;
        }
    }
    return {};
}

} // namespace

BrPropertyEditor::BrPropertyEditor(QWidget *parent) : QScrollArea(parent)
{
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    refresh();
}

QList<QObject *> BrPropertyEditor::objects() const
{
    QList<QObject *> list;
    for (const QPointer<QObject> &o : m_objects)
        if (o)
            list << o;
    return list;
}

void BrPropertyEditor::setObjects(const QList<QObject *> &objects)
{
    m_objects.clear();
    for (QObject *o : objects)
        m_objects << o;
    refresh();
}

QString BrPropertyEditor::labelFor(const QString &name)
{
    const QHash<QString, QString> special = {
        {QStringLiteral("dataField"), tr("Field")},
        {QStringLiteral("dataFormula"), tr("Formula")},
        {QStringLiteral("dataFields"), tr("Group by fields")},
        {QStringLiteral("dataSourceName"), tr("Data source")},
        {QStringLiteral("printIf"), tr("Print if")},
        {QStringLiteral("highlightIf"), tr("Highlight if")},
        {QStringLiteral("allowHtml"), tr("HTML text")},
        {QStringLiteral("qrErrorCorrection"), tr("QR error correction")},
        {QStringLiteral("objectName"), tr("Name")},
        {QStringLiteral("protection"), tr("Protection")},
    };
    if (special.contains(name))
        return special.value(name);
    return BrDesignerLabels::label(name);
}

void BrPropertyEditor::apply(const char *name, const QVariant &value)
{
    for (QObject *o : objects()) {
        const int index = o->metaObject()->indexOfProperty(name);
        if (index >= 0)
            o->metaObject()->property(index).write(o, value);
    }
    emit edited(QString::fromLatin1(name));
}

void BrPropertyEditor::refresh()
{
    delete m_panel;
    m_panel = new QWidget;
    auto *outer = new QVBoxLayout(m_panel);
    outer->setContentsMargins(6, 6, 6, 6);
    const QList<QObject *> objs = objects();
    if (objs.isEmpty()) {
        auto *hint = new QLabel(tr("Select an object to edit its properties."), m_panel);
        hint->setWordWrap(true);
        outer->addWidget(hint);
        outer->addStretch();
        setWidget(m_panel);
        return;
    }

    QObject *first = objs.first();
    auto *title = new QLabel(QStringLiteral("<b>%1</b>%2")
                                 .arg(BrDesignerLabels::typeLabel(qobject_cast<BrReport *>(first) ? QStringLiteral("Report") : BrReportFile::typeName(first)),
                                      objs.size() > 1 ? tr(" (%1 selected)").arg(objs.size()) : QString()),
                             m_panel);
    outer->addWidget(title);

    // Several objects can be selected: what is locked in one is locked for all.
    bool readOnly = false, positionLocked = false;
    QSet<QString> hidden;
    if (!m_developerMode) {
        for (QObject *o : objs) {
            const BrProtection protection = BrProtection::of(o);
            readOnly = readOnly || protection.properties;
            positionLocked = positionLocked || protection.move;
            for (const QString &name : protection.hiddenProperties)
                hidden.insert(name);
        }
    }

    // Above the long list of properties, so that they are seen without scrolling.
    if (m_developerMode) {
        auto *protectionForm = new QFormLayout;
        outer->addLayout(protectionForm);
        addProtectionEditors(protectionForm, first);
    }

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight);
    outer->addLayout(form);

    // Name
    auto *nameEdit = new QLineEdit(first->objectName(), m_panel);
    nameEdit->setEnabled(objs.size() == 1);
    connect(nameEdit, &QLineEdit::editingFinished, this, [this, nameEdit]() {
        const QList<QObject *> list = objects();
        if (list.size() == 1 && list.first()->objectName() != nameEdit->text()) {
            list.first()->setObjectName(nameEdit->text().trimmed());
            emit edited(QStringLiteral("objectName"));
        }
    });
    if (hidden.contains(QStringLiteral("objectName"))) {
        delete nameEdit;
    } else {
        form->addRow(tr("Name"), nameEdit);
    }

    // Position and size for objects (not bands/report)
    const QList<BrControl *> positioned = positionedControls(objs);
    {
        if (!positioned.isEmpty() && !hidden.contains(QStringLiteral("geometry"))) {
            auto *row = new QWidget(m_panel);
            auto *h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            const QStringList tips = {tr("X"), tr("Y"), tr("W", "width"), tr("H", "height")};
            for (int i = 0; i < 4; ++i) {
                const double value = geometryValue(positioned.first()->geometry(), i);
                auto *s = mmSpin<BrMixedSpinBox>(value, row, i >= 2 ? 0.5 : -10000.0);
                s->setPrefix(tips.at(i) + QLatin1Char(' '));
                s->setObjectName(QStringLiteral("geometry%1").arg(i));
                for (BrControl *c : positioned)
                    if (qAbs(geometryValue(c->geometry(), i) - value) >= 0.005) {
                        s->setMixed(value);
                        break;
                    }
                h->addWidget(s);
                // Only the edited value changes, so that the objects keep the others.
                connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, s, i](double v) {
                    if (s->isBlank())
                        return;
                    for (BrControl *c : positionedControls(objects())) {
                        QRectF g = c->geometry();
                        setGeometryValue(g, i, v);
                        c->setGeometry(g);
                    }
                    emit edited(QStringLiteral("geometry"));
                });
            }
            row->setEnabled(!positionLocked);
            form->addRow(tr("Position (mm)"), row);
        }
    }

    static const QSet<QByteArray> positionProperties = {"align", "height", "bandType"};
    for (const QMetaProperty &p : listedProperties(first)) {
        if (hidden.contains(QString::fromLatin1(p.name())))
            continue;
        if (QWidget *editor = createEditor(p, first)) {
            editor->setEnabled(!positionLocked || !positionProperties.contains(p.name()));
            form->addRow(labelFor(QString::fromLatin1(p.name())), editor);
        }
    }
    if (readOnly) {
        for (QWidget *w : m_panel->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
            if (w != title)
                w->setEnabled(false);
        auto *note = new QLabel(tr("This object is protected: its properties cannot be changed."), m_panel);
        note->setWordWrap(true);
        outer->insertWidget(1, note);
    }
    outer->addStretch();
    setWidget(m_panel);
}

void BrPropertyEditor::applyProtection(const std::function<void(BrProtection &)> &change)
{
    for (QObject *o : objects()) {
        if (o->metaObject()->indexOfProperty("protection") < 0)
            continue;
        BrProtection protection = BrProtection::of(o);
        change(protection);
        o->setProperty("protection", QVariant::fromValue(protection));
    }
    emit edited(QStringLiteral("protection"));
}

void BrPropertyEditor::addProtectionEditors(QFormLayout *form, QObject *object)
{
    const BrProtection protection = BrProtection::of(object);
    auto addCheck = [this, form](const QString &name, const QString &text, bool value, bool BrProtection::*field) {
        auto *check = new QCheckBox(text, m_panel);
        check->setObjectName(name);
        check->setChecked(value);
        connect(check, &QCheckBox::toggled, this, [this, field](bool on) {
            applyProtection([field, on](BrProtection &p) { p.*field = on; });
        });
        form->addRow(QString(), check);
    };

    form->addRow(new QLabel(QStringLiteral("<b>%1</b>").arg(tr("Protection for end users")), m_panel));
    if (!qobject_cast<BrReport *>(object)) {
        addCheck(QStringLiteral("protectMove"), tr("Lock position and size"), protection.move, &BrProtection::move);
        addCheck(QStringLiteral("protectRemove"), tr("Cannot be deleted"), protection.remove, &BrProtection::remove);
    }
    addCheck(QStringLiteral("protectProperties"), tr("Read-only properties"), protection.properties,
             &BrProtection::properties);
    if (qobject_cast<BrContainer *>(object) || qobject_cast<BrPager *>(object))
        addCheck(QStringLiteral("protectContents"), tr("Lock contents (add and delete)"), protection.contents,
                 &BrProtection::contents);

    auto *button = new QPushButton(tr("Hidden properties... (%1)").arg(protection.hiddenProperties.size()), m_panel);
    button->setObjectName(QStringLiteral("protectHidden"));
    connect(button, &QPushButton::clicked, this, [this, object]() { editHiddenProperties(object); });
    form->addRow(QString(), button);
}

void BrPropertyEditor::editHiddenProperties(QObject *object)
{
    QList<QPair<QString, QString>> names = {{QStringLiteral("objectName"), tr("Name")}};
    if (qobject_cast<BrControl *>(object) && !object->inherits("BrBand"))
        names.append({QStringLiteral("geometry"), tr("Position (mm)")});
    if (qobject_cast<BrReport *>(object)) {
        names.append({QStringLiteral("queries"), tr("SQL queries")});
        names.append({QStringLiteral("parameters"), tr("Report parameters")});
    }
    for (const QMetaProperty &p : listedProperties(object)) {
        const QString name = QString::fromLatin1(p.name());
        names.append({name, labelFor(name)});
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Hidden properties"));
    auto *list = new QListWidget(&dialog);
    const QStringList current = BrProtection::of(object).hiddenProperties;
    for (const auto &name : std::as_const(names)) {
        auto *item = new QListWidgetItem(name.second, list);
        item->setData(Qt::UserRole, name.first);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(current.contains(name.first) ? Qt::Checked : Qt::Unchecked);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("End users do not see the selected properties:"), &dialog));
    layout->addWidget(list);
    layout->addWidget(buttons);
    dialog.resize(340, 440);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QStringList hidden;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->checkState() == Qt::Checked)
            hidden << list->item(i)->data(Qt::UserRole).toString();
    applyProtection([&hidden](BrProtection &p) { p.hiddenProperties = hidden; });
}

QWidget *BrPropertyEditor::createEditor(const QMetaProperty &p, QObject *object)
{
    const QVariant value = p.read(object);
    const QByteArray name = p.name();
    const char *cname = p.name(); // stable: points into the meta-object

    if (p.isEnumType()) {
        auto *combo = new QComboBox(m_panel);
        const QMetaEnum e = p.enumerator();
        for (int i = 0; i < e.keyCount(); ++i)
            combo->addItem(labelFor(QString::fromLatin1(e.key(i))), e.value(i));
        combo->setCurrentIndex(combo->findData(value.toInt()));
        connect(combo, QOverload<int>::of(&QComboBox::activated), this,
                [this, combo, cname]() { apply(cname, combo->currentData()); });
        return combo;
    }

    const int type = p.userType();
    if (type == qMetaTypeId<BrBorders>()) {
        const BrBorders b = value.value<BrBorders>();
        auto *row = new QWidget(m_panel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(2);
        QList<QCheckBox *> sides;
        const QStringList labels = {tr("L", "left"), tr("T", "top"), tr("R", "right"), tr("B", "bottom")};
        const bool values[] = {b.left, b.top, b.right, b.bottom};
        for (int i = 0; i < 4; ++i) {
            auto *cb = new QCheckBox(labels.at(i), row);
            cb->setChecked(values[i]);
            h->addWidget(cb);
            sides << cb;
        }
        auto *width = mmSpin(b.width, row, 0.0);
        width->setMaximumWidth(56);
        h->addWidget(width);
        auto *color = new QPushButton(row);
        paintSwatch(color, b.color);
        color->setProperty("color", b.color);
        h->addWidget(color);
        auto commit = [this, sides, width, color]() {
            BrBorders nb;
            nb.left = sides[0]->isChecked();
            nb.top = sides[1]->isChecked();
            nb.right = sides[2]->isChecked();
            nb.bottom = sides[3]->isChecked();
            nb.width = width->value();
            nb.color = color->property("color").value<QColor>();
            apply("borders", QVariant::fromValue(nb));
        };
        for (QCheckBox *cb : sides)
            connect(cb, &QCheckBox::toggled, this, commit);
        connect(width, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, commit);
        connect(color, &QPushButton::clicked, this, [this, color, commit]() {
            const QColor c = QColorDialog::getColor(color->property("color").value<QColor>(), this,
                                                    tr("Border color"));
            if (c.isValid()) {
                color->setProperty("color", c);
                commit();
            }
        });
        return row;
    }

    switch (type) {
    case QMetaType::Bool: {
        auto *cb = new QCheckBox(m_panel);
        cb->setChecked(value.toBool());
        connect(cb, &QCheckBox::toggled, this, [this, cname](bool on) { apply(cname, on); });
        return cb;
    }
    case QMetaType::Int: {
        auto *s = new QSpinBox(m_panel);
        s->setRange(0, 100000);
        s->setValue(value.toInt());
        s->setKeyboardTracking(false);
        connect(s, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, cname](int v) { apply(cname, v); });
        return s;
    }
    case QMetaType::Double: {
        auto *s = mmSpin(value.toDouble(), m_panel);
        s->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, cname](double v) { apply(cname, v); });
        return s;
    }
    case QMetaType::QString: {
        // Data source and field names: choose from the available ones (still editable).
        QStringList choices;
        if (name == "dataSourceName") {
            for (QObject *o = object; o; o = o->parent())
                if (auto *r = qobject_cast<BrReport *>(o)) {
                    choices = r->dataSourceNames();
                    break;
                }
        } else if (name == "dataField" || name == "dataFields" || name == "labelField"
                   || name == "masterFields" || name == "detailFields") {
            choices = fieldsFor(object, name == "masterFields");
        }
        if (!choices.isEmpty()) {
            auto *combo = new QComboBox(m_panel);
            combo->setEditable(true);
            combo->addItem(QString());
            combo->addItems(choices);
            combo->setCurrentText(value.toString());
            connect(combo, QOverload<int>::of(&QComboBox::activated), this,
                    [this, combo, cname]() { apply(cname, combo->currentText()); });
            connect(combo->lineEdit(), &QLineEdit::editingFinished, this, [this, combo, cname, object]() {
                const int idx = object->metaObject()->indexOfProperty(cname);
                if (idx >= 0 && object->metaObject()->property(idx).read(object).toString() != combo->currentText())
                    apply(cname, combo->currentText());
            });
            return combo;
        }
        const bool multiline = name == "text" || name == "sql";
        auto *row = new QWidget(m_panel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto *edit = new QLineEdit(value.toString(), row);
        h->addWidget(edit);
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, cname, object]() {
            const int idx = object->metaObject()->indexOfProperty(cname);
            if (idx >= 0 && object->metaObject()->property(idx).read(object).toString() != edit->text())
                apply(cname, edit->text());
        });
        if (name == "dataFormula" || name == "printIf" || name == "highlightIf") {
            auto *builder = new QToolButton(row);
            builder->setText(QStringLiteral("…"));
            builder->setToolTip(tr("Open the expression builder"));
            h->addWidget(builder);
            connect(builder, &QToolButton::clicked, this, [this, edit, cname, object]() {
                BrExpressionEditor dialog(object, QByteArray(cname), edit->text(), this);
                if (dialog.exec() != QDialog::Accepted)
                    return;
                edit->setText(dialog.formula());
                apply(cname, dialog.formula());
            });
        }
        if (multiline) {
            auto *more = new QToolButton(row);
            more->setText(QStringLiteral("…"));
            more->setToolTip(tr("Edit in a larger window"));
            h->addWidget(more);
            connect(more, &QToolButton::clicked, this, [this, edit, cname]() {
                QString text = edit->text();
                text.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
                if (editText(this, labelFor(QString::fromLatin1(cname)), text))
                    apply(cname, text);
            });
        }
        return row;
    }
    case QMetaType::QStringList: {
        auto *edit = new QLineEdit(value.toStringList().join(QStringLiteral("; ")), m_panel);
        edit->setToolTip(tr("Values separated by ';'"));
        connect(edit, &QLineEdit::editingFinished, this, [this, edit, cname]() {
            QStringList list;
            for (const QString &s : edit->text().split(QLatin1Char(';'), Qt::SkipEmptyParts))
                list << s.trimmed();
            apply(cname, list);
        });
        return edit;
    }
    case QMetaType::QColor: {
        auto *row = new QWidget(m_panel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto *button = new QPushButton(row);
        paintSwatch(button, value.value<QColor>());
        auto *clear = new QToolButton(row);
        clear->setText(QStringLiteral("✕"));
        clear->setToolTip(tr("No color"));
        h->addWidget(button, 1);
        h->addWidget(clear);
        const QColor current = value.value<QColor>();
        connect(button, &QPushButton::clicked, this, [this, cname, current]() {
            const QColor c = QColorDialog::getColor(current.isValid() ? current : Qt::white, this,
                                                    labelFor(QString::fromLatin1(cname)),
                                                    QColorDialog::ShowAlphaChannel);
            if (c.isValid())
                apply(cname, c);
        });
        connect(clear, &QToolButton::clicked, this, [this, cname]() { apply(cname, QColor()); });
        return row;
    }
    case QMetaType::QFont: {
        auto *row = new QWidget(m_panel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto *control = qobject_cast<BrControl *>(object);
        const bool inherited = control && !control->hasOwnFont();
        auto *button = new QPushButton(fontText(value.value<QFont>()) + (inherited ? tr(" (inherited)") : QString()), row);
        h->addWidget(button, 1);
        const QFont current = value.value<QFont>();
        connect(button, &QPushButton::clicked, this, [this, cname, current]() {
            bool ok = false;
            const QFont f = QFontDialog::getFont(&ok, current, this, labelFor(QString::fromLatin1(cname)));
            if (ok)
                apply(cname, f);
        });
        if (control) {
            auto *reset = new QToolButton(row);
            reset->setText(QStringLiteral("↺"));
            reset->setToolTip(tr("Use the font of the band/report"));
            reset->setEnabled(!inherited);
            h->addWidget(reset);
            connect(reset, &QToolButton::clicked, this, [this]() {
                for (QObject *o : objects())
                    if (auto *c = qobject_cast<BrControl *>(o))
                        c->resetFont();
                emit edited(QStringLiteral("font"));
            });
        }
        return row;
    }
    case QMetaType::QSizeF: { // paper size
        const QSizeF size = value.toSizeF();
        auto *row = new QWidget(m_panel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        auto *combo = new QComboBox(row);
        const QList<QPageSize::PageSizeId> ids = {QPageSize::A3, QPageSize::A4, QPageSize::A5,
                                                  QPageSize::A6, QPageSize::Letter, QPageSize::Legal};
        for (QPageSize::PageSizeId id : ids)
            combo->addItem(QPageSize::name(id), QPageSize::size(id, QPageSize::Millimeter));
        combo->addItem(tr("Custom"), QSizeF());
        int current = combo->count() - 1;
        for (int i = 0; i + 1 < combo->count(); ++i) {
            const QSizeF s = combo->itemData(i).toSizeF();
            if (std::abs(s.width() - size.width()) < 0.5 && std::abs(s.height() - size.height()) < 0.5)
                current = i;
        }
        combo->setCurrentIndex(current);
        auto *w = mmSpin(size.width(), row, 10.0);
        auto *hh = mmSpin(size.height(), row, 10.0);
        h->addWidget(combo, 1);
        h->addWidget(w);
        h->addWidget(hh);
        connect(combo, QOverload<int>::of(&QComboBox::activated), this, [this, combo, cname]() {
            const QSizeF s = combo->currentData().toSizeF();
            if (s.isValid())
                apply(cname, s);
        });
        auto commit = [this, w, hh, cname]() { apply(cname, QSizeF(w->value(), hh->value())); };
        connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, commit);
        connect(hh, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, commit);
        return row;
    }
    case QMetaType::QImage: {
        auto *row = new QWidget(m_panel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        const QImage img = value.value<QImage>();
        const bool hasSvg = !object->property("svg").toString().isEmpty();
        auto *info = new QLabel(hasSvg ? QStringLiteral("SVG")
                                : img.isNull() ? tr("(none)")
                                               : tr("%1 x %2").arg(img.width()).arg(img.height()),
                                row);
        auto *load = new QPushButton(tr("Load..."), row);
        auto *clear = new QToolButton(row);
        clear->setText(QStringLiteral("✕"));
        h->addWidget(info, 1);
        h->addWidget(load);
        h->addWidget(clear);
        connect(load, &QPushButton::clicked, this, [this, cname]() {
            const QString file = QFileDialog::getOpenFileName(this, tr("Image"), QString(),
                                                              tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.svg)"));
            if (file.isEmpty())
                return;
            BrImage loaded;
            if (loaded.load(file)) {
                apply("svg", loaded.svg());
                apply(cname, loaded.image());
            }
        });
        connect(clear, &QToolButton::clicked, this, [this, cname]() {
            apply("svg", QString());
            apply(cname, QImage());
        });
        return row;
    }
    default:
        break;
    }

    if (type == qMetaTypeId<QMarginsF>()) {
        const QMarginsF m = value.value<QMarginsF>();
        auto *row = new QWidget(m_panel);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        QList<QDoubleSpinBox *> spins;
        const double values[] = {m.left(), m.top(), m.right(), m.bottom()};
        const QStringList tips = {tr("L", "left"), tr("T", "top"), tr("R", "right"), tr("B", "bottom")};
        for (int i = 0; i < 4; ++i) {
            auto *s = mmSpin(values[i], row, 0.0);
            s->setPrefix(tips.at(i) + QLatin1Char(' '));
            h->addWidget(s);
            spins << s;
        }
        for (QDoubleSpinBox *s : spins)
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, spins, cname]() {
                apply(cname, QVariant::fromValue(QMarginsF(spins[0]->value(), spins[1]->value(),
                                                           spins[2]->value(), spins[3]->value())));
            });
        return row;
    }

    if (type == QMetaType::QVariantMap) { // report variables / parameters
        auto *button = new QPushButton(tr("Edit... (%1)").arg(value.toMap().size()), m_panel);
        connect(button, &QPushButton::clicked, this, [this, cname, value]() {
            QStringList lines;
            const QVariantMap map = value.toMap();
            for (auto it = map.begin(); it != map.end(); ++it)
                lines << it.key() + QStringLiteral(" = ") + it.value().toString();
            QString text = lines.join(QLatin1Char('\n'));
            if (!editText(this, tr("Variables (name = value, one per line)"), text))
                return;
            QVariantMap result;
            for (const QString &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const int eq = line.indexOf(QLatin1Char('='));
                if (eq > 0)
                    result.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
            }
            apply(cname, result);
        });
        return button;
    }

    if (type == QMetaType::QVariantList) { // chart series
        auto *button = new QPushButton(tr("Edit... (%1)").arg(value.toList().size()), m_panel);
        connect(button, &QPushButton::clicked, this, [this, cname, value]() {
            QString text = QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Indented));
            if (!editText(this, labelFor(QString::fromLatin1(cname)) + tr(" (JSON)"), text, true))
                return;
            const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
            if (doc.isArray())
                apply(cname, doc.toVariant());
        });
        return button;
    }
    return nullptr;
}
