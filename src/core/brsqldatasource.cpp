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

#include "brsqldatasource.h"

#include <QSqlDriver>
#include <QSqlError>
#include <QSqlRecord>

BrSqlDataSource::BrSqlDataSource(QObject *parent) : BrDataSource(parent) {}

BrSqlDataSource::BrSqlDataSource(QSqlQuery &&query, QObject *parent)
    : BrDataSource(parent), m_query(std::move(query))
{
}

void BrSqlDataSource::setQuery(QSqlQuery &&query)
{
    m_query = std::move(query);
    m_eof = true;
}

bool BrSqlDataSource::exec(const QString &sql, const QSqlDatabase &database)
{
    QSqlQuery q(database);
    const bool ok = q.exec(sql);
    setQuery(std::move(q));
    return ok;
}

QString BrSqlDataSource::lastError() const
{
    return m_query.lastError().text();
}

bool BrSqlDataSource::first()
{
    m_eof = !m_query.isActive() || !m_query.first();
    return !m_eof;
}

bool BrSqlDataSource::next()
{
    if (m_eof)
        return false;
    m_eof = !m_query.next();
    return !m_eof;
}

bool BrSqlDataSource::eof() const
{
    return m_eof;
}

QStringList BrSqlDataSource::fieldNames() const
{
    QStringList names;
    const QSqlRecord rec = m_query.record();
    for (int i = 0; i < rec.count(); ++i)
        names << rec.fieldName(i);
    return names;
}

QVariant BrSqlDataSource::value(const QString &field) const
{
    if (m_eof)
        return {};
    const QSqlRecord rec = m_query.record();
    int index = rec.indexOf(field);
    if (index < 0) {
        for (int i = 0; i < rec.count(); ++i) {
            if (rec.fieldName(i).compare(field, Qt::CaseInsensitive) == 0) {
                index = i;
                break;
            }
        }
    }
    return index >= 0 ? m_query.value(index) : QVariant();
}

int BrSqlDataSource::recordCount() const
{
    if (m_query.driver() && m_query.driver()->hasFeature(QSqlDriver::QuerySize))
        return m_query.size();
    return -1;
}
