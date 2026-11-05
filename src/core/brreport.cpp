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

#include "brreport.h"

#include "brband.h"
#include "brdatasource.h"
#include "brengine_p.h"
#include "brexport.h"
#include "brreportfile.h"
#ifdef BRREPORT_HAS_SQL
#include "brsqldatasource.h"
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlRecord>
#endif

#include <QFile>
#include <QSaveFile>

#ifdef BRREPORT_HAS_SQL
namespace {

// Binds the ":variable" placeholders of \a sql to the report variables.
void bindVariables(QSqlQuery &query, const QString &sql, const BrReport *report)
{
    static const QRegularExpression placeholder(QStringLiteral(":([A-Za-z_][A-Za-z0-9_]*)"));
    auto it = placeholder.globalMatch(sql);
    while (it.hasNext()) {
        const QString var = it.next().captured(1);
        query.bindValue(QLatin1Char(':') + var, report->variable(var));
    }
}

} // namespace
#endif

BrReport::BrReport(QObject *parent) : BrPager(nullptr), m_font(QStringLiteral("Arial"), 10)
{
    setParent(parent);
}

BrReport::~BrReport()
{
    qDeleteAll(m_queryDataSources);
}

void BrReport::setPageSizeMm(double width, double height)
{
    m_pageSize = QPageSize(QSizeF(width, height), QPageSize::Millimeter, QString(),
                           QPageSize::ExactMatch);
}

QSizeF BrReport::pageSizeMm() const
{
    QSizeF s = m_pageSize.size(QPageSize::Millimeter);
    const bool landscape = m_orientation == QPageLayout::Landscape;
    if ((landscape && s.width() < s.height()) || (!landscape && s.width() > s.height()))
        s.transpose();
    return s;
}

QRectF BrReport::contentRect() const
{
    const QSizeF s = pageSizeMm();
    return QRectF(m_margins.left(), m_margins.top(),
                  s.width() - m_margins.left() - m_margins.right(),
                  s.height() - m_margins.top() - m_margins.bottom());
}

void BrReport::setVariable(const QString &name, const QVariant &value)
{
    m_variables.insert(name, value);
}

QVariant BrReport::variable(const QString &name, bool *found) const
{
    bool ok = false;
    QVariant value = brMapValue(m_variables, name, &ok);
    for (const QVariant &item : m_parameters) {
        const QVariantMap parameter = item.toMap();
        if (parameter.value(QStringLiteral("name")).toString().compare(name, Qt::CaseInsensitive) == 0) {
            value = parameterValue(parameter, ok ? value : parameter.value(QStringLiteral("default")));
            ok = true;
            break;
        }
    }
    if (found)
        *found = ok;
    return value;
}

void BrReport::addParameter(const QString &name, const QString &caption, const QString &type,
                            const QVariant &defaultValue, const QVariantList &choices)
{
    QVariantMap p{{QStringLiteral("name"), name}, {QStringLiteral("type"), type}};
    if (!caption.isEmpty())
        p.insert(QStringLiteral("caption"), caption);
    if (defaultValue.isValid())
        p.insert(QStringLiteral("default"), defaultValue);
    if (!choices.isEmpty())
        p.insert(QStringLiteral("choices"), choices);
    m_parameters << p;
}

QVariantList BrReport::parameterChoices(const QVariantMap &parameter, QString *error) const
{
    if (error)
        error->clear();
    const QString sql = parameter.value(QStringLiteral("choicesSql")).toString();
    if (sql.trimmed().isEmpty())
        return parameter.value(QStringLiteral("choices")).toList();
#ifdef BRREPORT_HAS_SQL
    const QString connection = parameter.value(QStringLiteral("choicesConnection")).toString();
    QSqlDatabase db = connection.isEmpty() ? QSqlDatabase::database() : QSqlDatabase::database(connection);
    QSqlQuery query(db);
    QString failure;
    if (!db.isOpen())
        failure = QStringLiteral("database connection '%1' is not open.")
                      .arg(connection.isEmpty() ? QStringLiteral("default") : connection);
    else if (!query.prepare(sql))
        failure = query.lastError().text();
    if (failure.isEmpty()) {
        bindVariables(query, sql, this);
        if (!query.exec())
            failure = query.lastError().text();
    }
    if (!failure.isEmpty()) {
        if (error)
            *error = failure;
        return {};
    }
    const bool hasCaption = query.record().count() > 1;
    QVariantList choices;
    while (query.next()) {
        QVariantMap choice{{QStringLiteral("value"), query.value(0)}};
        if (hasCaption)
            choice.insert(QStringLiteral("caption"), query.value(1).toString());
        choices << choice;
    }
    return choices;
#else
    if (error)
        *error = QStringLiteral("This BrReport build has no SQL support (BRREPORT_WITH_SQL=OFF).");
    return {};
#endif
}

QVariant BrReport::parameterValue(const QVariantMap &parameter, const QVariant &value)
{
    const QString type = parameter.value(QStringLiteral("type")).toString().toLower();
    if (type == QLatin1String("date")) {
        const QDate date = value.toDate(); // also from "yyyy-MM-dd"; "today" is not a date
        return date.isValid() ? date : QDate::currentDate();
    }
    if (type == QLatin1String("integer"))
        return value.toLongLong();
    if (type == QLatin1String("number"))
        return value.toDouble();
    if (type == QLatin1String("boolean"))
        return value.toBool();
    return value.toString();
}

void BrReport::registerFunction(const QString &name, const CustomFunction &function)
{
    m_functions.insert(name.toLower(), function);
}

BrReport::CustomFunction BrReport::function(const QString &name) const
{
    return m_functions.value(name.toLower());
}

bool BrReport::prepare()
{
    m_canceled = false;
    m_error.clear();
    if (!openQueries()) {
        m_prepared = false;
        return false;
    }
    BrEngine engine(this);
    m_prepared = engine.run(&m_document, &m_error);
    return m_prepared;
}

void BrReport::clearDocument()
{
    m_document.clear();
    m_prepared = false;
}

bool BrReport::ensurePrepared()
{
    return m_prepared || prepare();
}

bool BrReport::exportToPdf(const QString &fileName)
{
    if (!ensurePrepared())
        return false;
    if (!BrExport::toPdf(m_document, fileName)) {
        m_error = QStringLiteral("Could not write PDF file '%1'.").arg(fileName);
        return false;
    }
    return true;
}

bool BrReport::exportToHtml(const QString &fileName)
{
    if (!ensurePrepared())
        return false;
    if (!BrExport::toHtml(m_document, fileName)) {
        m_error = QStringLiteral("Could not write HTML file '%1'.").arg(fileName);
        return false;
    }
    return true;
}

QStringList BrReport::exportToImages(const QString &filePattern, double dpi)
{
    if (!ensurePrepared())
        return {};
    const QStringList files = BrExport::toImages(m_document, filePattern, dpi);
    if (files.isEmpty())
        m_error = QStringLiteral("Could not write images '%1'.").arg(filePattern);
    return files;
}

bool BrReport::saveDocument(const QString &fileName)
{
    if (!ensurePrepared())
        return false;
    if (!m_document.save(fileName)) {
        m_error = QStringLiteral("Could not write file '%1'.").arg(fileName);
        return false;
    }
    return true;
}

BrDataSource *BrReport::dataSource() const
{
    if (m_dataSource)
        return m_dataSource;
    return m_dataSourceName.isEmpty() ? nullptr : findDataSource(m_dataSourceName);
}

void BrReport::setDataSource(const QString &name, BrDataSource *dataSource)
{
    addDataSource(name, dataSource);
    m_dataSource = nullptr;
    m_dataSourceName = name;
}

void BrReport::addDataSource(const QString &name, BrDataSource *dataSource)
{
    if (!dataSource) {
        m_dataSources.remove(name.toLower());
        m_dataSourceLabels.remove(name.toLower());
        return;
    }
    if (dataSource->objectName().isEmpty())
        dataSource->setObjectName(name);
    m_dataSources.insert(name.toLower(), dataSource);
    m_dataSourceLabels.insert(name.toLower(), name);
}

BrDataSource *BrReport::findDataSource(const QString &name) const
{
    return m_dataSources.value(name.toLower());
}

void BrReport::setPaperSize(const QSizeF &mm)
{
    m_pageSize = QPageSize(mm, QPageSize::Millimeter, QString(), QPageSize::FuzzyMatch);
}

void BrReport::setLocaleName(const QString &name)
{
    m_localeName = name;
    m_locale = name.isEmpty() ? QLocale() : QLocale(name);
}

void BrReport::clearLayout()
{
    const QObjectList kids = children();
    for (QObject *o : kids)
        if (qobject_cast<BrBand *>(o) || qobject_cast<BrPager *>(o))
            delete o;
    clearDocument();
}

bool BrReport::loadFromJson(const QByteArray &json)
{
    clearDocument();
    if (!BrReportFile::fromJson(this, json, &m_error))
        return false;
    return true;
}

QByteArray BrReport::toJson() const
{
    return BrReportFile::toJson(this);
}

bool BrReport::load(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Could not open '%1'.").arg(fileName);
        return false;
    }
    return loadFromJson(file.readAll());
}

bool BrReport::save(const QString &fileName) const
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly) || file.write(toJson()) < 0)
        return false;
    return file.commit();
}

QStringList BrReport::dataSourceNames() const
{
    QStringList names;
    for (const QString &label : m_dataSourceLabels)
        names << label;
    for (const QVariant &q : m_queries) {
        const QString name = q.toMap().value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive))
            names << name;
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

void BrReport::addQuery(const QString &name, const QString &sql, const QString &connection)
{
    QVariantMap q{{QStringLiteral("name"), name}, {QStringLiteral("sql"), sql}};
    if (!connection.isEmpty())
        q.insert(QStringLiteral("connection"), connection);
    m_queries << q;
}

bool BrReport::openQueries()
{
    if (m_queries.isEmpty())
        return true;
#ifdef BRREPORT_HAS_SQL
    for (const QVariant &item : m_queries) {
        const QVariantMap q = item.toMap();
        const QString name = q.value(QStringLiteral("name")).toString();
        const QString key = name.toLower();
        if (name.isEmpty())
            continue;
        BrDataSource *existing = m_dataSources.value(key);
        if (existing && existing != m_queryDataSources.value(key))
            continue; // registered by the application

        const QString connection = q.value(QStringLiteral("connection")).toString();
        QSqlDatabase db = connection.isEmpty() ? QSqlDatabase::database()
                                               : QSqlDatabase::database(connection);
        if (!db.isOpen()) {
            m_error = QStringLiteral("Query '%1': database connection '%2' is not open.")
                          .arg(name, connection.isEmpty() ? QStringLiteral("default") : connection);
            return false;
        }
        const QString sql = q.value(QStringLiteral("sql")).toString();
        QSqlQuery query(db);
        if (!query.prepare(sql)) {
            m_error = QStringLiteral("Query '%1': %2").arg(name, query.lastError().text());
            return false;
        }
        bindVariables(query, sql, this);
        if (!query.exec()) {
            m_error = QStringLiteral("Query '%1': %2").arg(name, query.lastError().text());
            return false;
        }
        BrSqlDataSource *ds = static_cast<BrSqlDataSource *>(m_queryDataSources.value(key));
        if (!ds) {
            ds = new BrSqlDataSource;
            m_queryDataSources.insert(key, ds);
        }
        ds->setQuery(std::move(query));
        addDataSource(name, ds);
    }
    return true;
#else
    m_error = QStringLiteral("This BrReport build has no SQL support (BRREPORT_WITH_SQL=OFF).");
    return false;
#endif
}
