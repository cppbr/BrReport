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

#include "brparametersdialog.h"

#include "brreport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

namespace {

QWidget *createEditor(const QVariantMap &parameter, const QVariantList &choices, const QVariant &value,
                      QWidget *parent)
{
    if (!choices.isEmpty()) {
        auto *combo = new QComboBox(parent);
        for (const QVariant &choice : choices) {
            const QVariantMap map = choice.toMap();
            const QVariant raw = map.isEmpty() ? choice : map.value(QStringLiteral("value"));
            const QString caption = map.value(QStringLiteral("caption")).toString();
            combo->addItem(caption.isEmpty() ? raw.toString() : caption,
                           BrReport::parameterValue(parameter, raw));
        }
        combo->setCurrentIndex(qMax(0, combo->findData(value)));
        return combo;
    }
    const QString type = parameter.value(QStringLiteral("type")).toString().toLower();
    if (type == QLatin1String("date")) {
        auto *edit = new QDateEdit(value.toDate(), parent);
        edit->setCalendarPopup(true);
        return edit;
    }
    if (type == QLatin1String("integer")) {
        auto *spin = new QSpinBox(parent);
        spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        spin->setValue(value.toInt());
        return spin;
    }
    if (type == QLatin1String("number")) {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setDecimals(4);
        spin->setRange(-1e12, 1e12);
        spin->setValue(value.toDouble());
        return spin;
    }
    if (type == QLatin1String("boolean")) {
        auto *check = new QCheckBox(parent);
        check->setChecked(value.toBool());
        return check;
    }
    return new QLineEdit(value.toString(), parent);
}

QVariant editorValue(const QWidget *editor)
{
    if (auto *combo = qobject_cast<const QComboBox *>(editor))
        return combo->currentData();
    if (auto *date = qobject_cast<const QDateEdit *>(editor))
        return date->date();
    if (auto *spin = qobject_cast<const QSpinBox *>(editor))
        return spin->value();
    if (auto *spin = qobject_cast<const QDoubleSpinBox *>(editor))
        return spin->value();
    if (auto *check = qobject_cast<const QCheckBox *>(editor))
        return check->isChecked();
    return qobject_cast<const QLineEdit *>(editor)->text();
}

} // namespace

BrParametersDialog::BrParametersDialog(BrReport *report, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Report parameters"));
    auto *form = new QFormLayout;
    QStringList errors;
    for (const QVariant &item : report->parameters()) {
        const QVariantMap parameter = item.toMap();
        const QString name = parameter.value(QStringLiteral("name")).toString();
        if (name.isEmpty())
            continue;
        const QString caption = parameter.value(QStringLiteral("caption")).toString();
        const QString label = caption.isEmpty() ? name : caption;
        // If the list cannot be loaded, the user can still type the value.
        QString error;
        const QVariantList choices = report->parameterChoices(parameter, &error);
        if (!error.isEmpty())
            errors << tr("Could not load the choices of '%1': %2").arg(label, error);
        QWidget *editor = createEditor(parameter, choices, report->variable(name), this);
        editor->setObjectName(name);
        form->addRow(label, editor);
        m_fields << Field{parameter, editor};
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    if (!errors.isEmpty()) {
        auto *message = new QLabel(errors.join(QLatin1Char('\n')), this);
        message->setObjectName(QStringLiteral("errors"));
        message->setWordWrap(true);
        layout->addWidget(message);
    }
    layout->addWidget(buttons);
    setMinimumWidth(360);
}

QVariantMap BrParametersDialog::values() const
{
    QVariantMap result;
    for (const Field &field : m_fields)
        result.insert(field.parameter.value(QStringLiteral("name")).toString(), editorValue(field.editor));
    return result;
}
