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

#ifndef BRREPORTWIZARD_P_H
#define BRREPORTWIZARD_P_H

#include "brdesignerglobal.h"

#include <QMap>
#include <QVariantMap>
#include <QWizard>

class BrDataSource;

/*!
 * Designer wizard for a new list report: pick a data source (registered by the
 * application or a SQL query) and the fields, and get a report with a header,
 * column headers, detail, optional group, totals and page numbers.
 */
class BRREPORT_DESIGNER_PRIVATE_EXPORT BrReportWizard : public QWizard
{
    Q_OBJECT
public:
    enum class FieldKind { Unknown, Text, Integer, Decimal, Date };

    struct Column
    {
        QString field;
        QString caption;
        FieldKind kind = FieldKind::Unknown;
        int size = 0;   //!< Longest sampled value, in characters.
        int total = -1; //!< A Br::ResultType, or -1 for no total.
    };

    struct Options
    {
        QString title;
        QString dataSourceName;
        QVariantMap query; //!< Query that provides dataSourceName, if it is not an application one.
        QList<Column> columns;
        Column group; //!< Group by this field, when it is set. It leaves the columns.
        bool landscape = false;
        bool zebra = true;
    };

    /*! The fields of \a dataSource, with their kind and size taken from the first records. */
    static QList<Column> inspect(BrDataSource *dataSource);
    /*! "customer_name" and "customerName" give "Customer name". */
    static QString captionFor(const QString &fieldName);
    /*! Contents of a report file with the layout described by \a options. */
    static QByteArray buildLayout(const Options &options);

    /*! \a dataSources are those provided by the application. */
    explicit BrReportWizard(const QMap<QString, BrDataSource *> &dataSources, QWidget *parent = nullptr);

    /*! What was chosen in the pages. */
    Options options() const;

private:
    class DataPage;
    class FieldsPage;
    class LayoutPage;
    DataPage *m_dataPage = nullptr;
    FieldsPage *m_fieldsPage = nullptr;
    LayoutPage *m_layoutPage = nullptr;
};

#endif // BRREPORTWIZARD_P_H
