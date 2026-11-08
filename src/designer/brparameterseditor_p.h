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

#ifndef BRPARAMETERSEDITOR_P_H
#define BRPARAMETERSEDITOR_P_H

#include "brdesignerglobal.h"

#include <QDialog>
#include <QVariantList>

class BrReport;
class QTableWidget;

/*! Designer dialog that declares the parameters of a report. */
class BRREPORT_DESIGNER_PRIVATE_EXPORT BrParametersEditor : public QDialog
{
    Q_OBJECT
public:
    /*! \a report is used to test the queries that fill the choices. */
    explicit BrParametersEditor(const QVariantList &parameters, const BrReport *report,
                                QWidget *parent = nullptr);

    /*! Same format as BrReport::parameters(). */
    QVariantList parameters() const;

    void accept() override;

private:
    QVariantMap rowValues(int row) const;
    void setRowValues(int row, const QVariantMap &parameter);
    void addRow(const QVariantMap &parameter);
    void moveCurrentRow(int delta);
    void setChoicesQuery(int row, const QString &sql, const QString &connection);
    void editChoicesQuery();
    QString validate() const;

    const BrReport *m_report = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // BRPARAMETERSEDITOR_P_H
