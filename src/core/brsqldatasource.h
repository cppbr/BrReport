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

#ifndef BRSQLDATASOURCE_H
#define BRSQLDATASOURCE_H

#include "brdatasource.h"

#include <QSqlDatabase>
#include <QSqlQuery>

/*!
 * Data source reading an executed QSqlQuery.
 *
 * The query is moved into the data source (or executed from SQL text). Do not
 * call setForwardOnly(true) if the report has to be prepared more than once.
 * Available only when BrReport is built with BRREPORT_WITH_SQL (default ON).
 */
class BRREPORT_EXPORT BrSqlDataSource : public BrDataSource
{
    Q_OBJECT
public:
    explicit BrSqlDataSource(QObject *parent = nullptr);
    explicit BrSqlDataSource(QSqlQuery &&query, QObject *parent = nullptr);

    void setQuery(QSqlQuery &&query);
    /*! Executes \a sql on \a database. Returns false (see lastError()) on failure. */
    bool exec(const QString &sql, const QSqlDatabase &database = QSqlDatabase::database());
    QString lastError() const;

    QSqlQuery &query() { return m_query; }

    bool first() override;
    bool next() override;
    bool eof() const override;
    QStringList fieldNames() const override;
    QVariant value(const QString &field) const override;
    int recordCount() const override;

private:
    QSqlQuery m_query;
    bool m_eof = true;
};

#endif // BRSQLDATASOURCE_H
