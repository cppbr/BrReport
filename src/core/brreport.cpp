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

#include "brdatasource.h"
#include "brengine_p.h"
#include "brexport.h"

BrReport::BrReport(QObject *parent) : BrPager(nullptr), m_font(QStringLiteral("Arial"), 10)
{
    setParent(parent);
}

BrReport::~BrReport() = default;

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

QVariant BrReport::variable(const QString &name) const
{
    return brMapValue(m_variables, name);
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
