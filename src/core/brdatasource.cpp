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

#include "brdatasource.h"

#include <QAbstractItemModel>

QVariant brMapValue(const QVariantMap &map, const QString &key, bool *found)
{
    auto it = map.constFind(key);
    if (it == map.constEnd()) {
        for (auto i = map.constBegin(); i != map.constEnd(); ++i) {
            if (i.key().compare(key, Qt::CaseInsensitive) == 0) {
                it = i;
                break;
            }
        }
    }
    if (found)
        *found = it != map.constEnd();
    return it != map.constEnd() ? it.value() : QVariant();
}

// ---------------------------------------------------------------------------

BrDataSource::BrDataSource(QObject *parent) : QObject(parent) {}

BrDataSource::~BrDataSource() = default;

bool BrDataSource::hasField(const QString &field) const
{
    return fieldNames().contains(field, Qt::CaseInsensitive);
}

// ---------------------------------------------------------------------------

BrListDataSource::BrListDataSource(QObject *parent) : BrDataSource(parent) {}

BrListDataSource::BrListDataSource(const QList<QVariantMap> &rows, QObject *parent)
    : BrDataSource(parent), m_rows(rows)
{
}

void BrListDataSource::setRows(const QList<QVariantMap> &rows)
{
    m_rows = rows;
    m_index = -1;
}

void BrListDataSource::addRow(const QVariantMap &row)
{
    m_rows.append(row);
}

void BrListDataSource::clear()
{
    m_rows.clear();
    m_index = -1;
}

void BrListDataSource::setFilter(const std::function<bool(const QVariantMap &)> &filter)
{
    m_filter = filter;
}

void BrListDataSource::skipFiltered()
{
    if (!m_filter)
        return;
    while (m_index < m_rows.size() && !m_filter(m_rows.at(m_index)))
        ++m_index;
}

bool BrListDataSource::first()
{
    m_index = 0;
    skipFiltered();
    return !eof();
}

bool BrListDataSource::next()
{
    if (m_index < m_rows.size())
        ++m_index;
    skipFiltered();
    return !eof();
}

bool BrListDataSource::eof() const
{
    return m_index < 0 || m_index >= m_rows.size();
}

QStringList BrListDataSource::fieldNames() const
{
    if (!eof())
        return m_rows.at(m_index).keys();
    return m_rows.isEmpty() ? QStringList() : m_rows.first().keys();
}

QVariant BrListDataSource::value(const QString &field) const
{
    if (eof())
        return {};
    return brMapValue(m_rows.at(m_index), field);
}

int BrListDataSource::recordCount() const
{
    if (!m_filter)
        return int(m_rows.size());
    int n = 0;
    for (const QVariantMap &row : m_rows)
        if (m_filter(row))
            ++n;
    return n;
}

QVariantMap BrListDataSource::currentRow() const
{
    return eof() ? QVariantMap() : m_rows.at(m_index);
}

// ---------------------------------------------------------------------------

BrModelDataSource::BrModelDataSource(QAbstractItemModel *model, QObject *parent)
    : BrDataSource(parent), m_model(model), m_role(Qt::EditRole)
{
}

void BrModelDataSource::setModel(QAbstractItemModel *model)
{
    m_model = model;
    m_row = -1;
}

bool BrModelDataSource::first()
{
    if (!m_model)
        return false;
    // Lazily populated models (e.g. QSqlQueryModel) need to fetch everything.
    while (m_model->canFetchMore(QModelIndex()))
        m_model->fetchMore(QModelIndex());
    m_row = 0;
    return !eof();
}

bool BrModelDataSource::next()
{
    if (!eof())
        ++m_row;
    return !eof();
}

bool BrModelDataSource::eof() const
{
    return !m_model || m_row < 0 || m_row >= m_model->rowCount();
}

QStringList BrModelDataSource::fieldNames() const
{
    QStringList names;
    if (!m_model)
        return names;
    for (int c = 0; c < m_model->columnCount(); ++c)
        names << m_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    return names;
}

int BrModelDataSource::columnOf(const QString &field) const
{
    if (!m_model)
        return -1;
    for (int c = 0; c < m_model->columnCount(); ++c)
        if (m_model->headerData(c, Qt::Horizontal, Qt::DisplayRole)
                .toString()
                .compare(field, Qt::CaseInsensitive)
            == 0)
            return c;
    return -1;
}

QVariant BrModelDataSource::value(const QString &field) const
{
    if (eof())
        return {};
    const int col = columnOf(field);
    if (col < 0)
        return {};
    return m_model->index(m_row, col).data(m_role);
}

int BrModelDataSource::recordCount() const
{
    return m_model ? m_model->rowCount() : 0;
}

// ---------------------------------------------------------------------------

BrCallbackDataSource::BrCallbackDataSource(QObject *parent) : BrDataSource(parent) {}

BrCallbackDataSource::BrCallbackDataSource(const FetchFunction &fetch, QObject *parent)
    : BrDataSource(parent), m_fetch(fetch)
{
}

bool BrCallbackDataSource::fetch()
{
    m_row.clear();
    m_eof = !m_fetch || !m_fetch(m_index, m_row);
    return !m_eof;
}

bool BrCallbackDataSource::first()
{
    m_index = 0;
    return fetch();
}

bool BrCallbackDataSource::next()
{
    if (m_eof)
        return false;
    ++m_index;
    return fetch();
}

bool BrCallbackDataSource::eof() const
{
    return m_eof;
}

QStringList BrCallbackDataSource::fieldNames() const
{
    return m_row.keys();
}

QVariant BrCallbackDataSource::value(const QString &field) const
{
    return brMapValue(m_row, field);
}
