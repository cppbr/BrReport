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

#ifndef BRPARAMETERSDIALOG_H
#define BRPARAMETERSDIALOG_H

#include "brwidgetsglobal.h"

#include <QDialog>
#include <QList>
#include <QVariantMap>

class BrReport;

/*!
 * Asks the user for the parameters declared by a report: one editor per parameter,
 * filled with the current value of the report variable or the default value.
 * BrPrinting::preview() and print() show it; use it directly for your own flow.
 */
class BRREPORT_WIDGETS_EXPORT BrParametersDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BrParametersDialog(BrReport *report, QWidget *parent = nullptr);

    /*! Values typed by the user, by parameter name. The report is not changed. */
    QVariantMap values() const;

private:
    struct Field
    {
        QVariantMap parameter;
        QWidget *editor;
    };
    QList<Field> m_fields;
};

#endif // BRPARAMETERSDIALOG_H
