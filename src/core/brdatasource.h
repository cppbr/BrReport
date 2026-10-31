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

#ifndef BRDATASOURCE_H
#define BRDATASOURCE_H

#include "brglobal.h"

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <functional>

#include <QAbstractItemModel>

/*!
 * Forward-only cursor over records, consumed by BrReport and BrSubDetail
 *
 * Field names are case-insensitive. Data sources are not owned by the report;
 * give them a QObject parent or manage their lifetime yourself.
 */
class BRREPORT_EXPORT BrDataSource : public QObject
{
    Q_OBJECT
public:
    explicit BrDataSource(QObject *parent = nullptr);
    ~BrDataSource() override;

    /*! Positions on the first record. Returns false if there are no records. */
    virtual bool first() = 0;
    /*! Moves to the next record. Returns false when the end is reached. */
    virtual bool next() = 0;
    /*! True when there is no current record. */
    virtual bool eof() const = 0;
    /*! Field names available in the current record. */
    virtual QStringList fieldNames() const = 0;
    /*! Value of a field (case-insensitive) in the current record. */
    virtual QVariant value(const QString &field) const = 0;
    /*! True if the field exists (case-insensitive). */
    virtual bool hasField(const QString &field) const;
    /*! Number of records, or -1 if unknown. */
    virtual int recordCount() const { return -1; }
};

/*!
 * Data source backed by a list of QVariantMap (one map per record).
 * Ideal for JSON data, in-memory lists and tests.
 */
class BRREPORT_EXPORT BrListDataSource : public BrDataSource
{
    Q_OBJECT
public:
    explicit BrListDataSource(QObject *parent = nullptr);
    explicit BrListDataSource(const QList<QVariantMap> &rows, QObject *parent = nullptr);

    void setRows(const QList<QVariantMap> &rows);
    QList<QVariantMap> rows() const { return m_rows; }
    void addRow(const QVariantMap &row);
    void clear();

    /*! Optional filter applied while iterating (e.g. master/detail). */
    void setFilter(const std::function<bool(const QVariantMap &)> &filter);

    bool first() override;
    bool next() override;
    bool eof() const override;
    QStringList fieldNames() const override;
    QVariant value(const QString &field) const override;
    int recordCount() const override;

    /*! The current record (empty when eof()). */
    QVariantMap currentRow() const;

private:
    void skipFiltered();
    QList<QVariantMap> m_rows;
    std::function<bool(const QVariantMap &)> m_filter;
    int m_index = -1;
};

/*!
 * Data source reading a QAbstractItemModel row by row (QSqlQueryModel,
 * QStandardItemModel, your own models...). Field names come from the horizontal
 * header (Qt::DisplayRole).
 */
class BRREPORT_EXPORT BrModelDataSource : public BrDataSource
{
    Q_OBJECT
public:
    explicit BrModelDataSource(QAbstractItemModel *model = nullptr, QObject *parent = nullptr);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const { return m_model; }
    /*! Role used to read values (default: Qt::EditRole, which keeps raw types). */
    void setRole(int role) { m_role = role; }

    bool first() override;
    bool next() override;
    bool eof() const override;
    QStringList fieldNames() const override;
    QVariant value(const QString &field) const override;
    int recordCount() const override;

private:
    int columnOf(const QString &field) const;
    QPointer<QAbstractItemModel> m_model;
    int m_row = -1;
    int m_role;
};

/*!
 * Data source driven by a callback (data on demand).
 *
 * The fetch function receives the zero-based record index and fills \a row;
 * it returns false when there are no more records.
 */
class BRREPORT_EXPORT BrCallbackDataSource : public BrDataSource
{
    Q_OBJECT
public:
    using FetchFunction = std::function<bool(int index, QVariantMap &row)>;

    explicit BrCallbackDataSource(QObject *parent = nullptr);
    explicit BrCallbackDataSource(const FetchFunction &fetch, QObject *parent = nullptr);

    void setFetchFunction(const FetchFunction &fetch) { m_fetch = fetch; }

    bool first() override;
    bool next() override;
    bool eof() const override;
    QStringList fieldNames() const override;
    QVariant value(const QString &field) const override;

private:
    bool fetch();
    FetchFunction m_fetch;
    QVariantMap m_row;
    int m_index = -1;
    bool m_eof = true;
};

/*! Case-insensitive lookup in a QVariantMap. Returns an invalid QVariant if missing. */
BRREPORT_EXPORT QVariant brMapValue(const QVariantMap &map, const QString &key, bool *found = nullptr);

#endif // BRDATASOURCE_H
