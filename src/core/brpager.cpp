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

#include "brpager.h"

#include "brband.h"
#include "brrendercontext.h"
#include "brreport.h"

// ---------------------------------------------------------------------------
// BrPager
// ---------------------------------------------------------------------------

BrPager::BrPager(BrPager *parent) : QObject(parent) {}

BrPager::~BrPager() = default;

BrBand *BrPager::addBand(Br::BandType type, double height)
{
    return new BrBand(this, type, height);
}

QList<BrBand *> BrPager::bands() const
{
    QList<BrBand *> list;
    for (QObject *o : children())
        if (auto *b = qobject_cast<BrBand *>(o))
            list << b;
    return list;
}

QList<BrBand *> BrPager::bands(Br::BandType type) const
{
    QList<BrBand *> list;
    for (BrBand *b : bands())
        if (b->bandType() == type)
            list << b;
    return list;
}

BrGroup *BrPager::group() const
{
    for (QObject *o : children())
        if (auto *g = qobject_cast<BrGroup *>(o))
            if (g->isEnabled())
                return g;
    return nullptr;
}

BrPager *BrPager::parentPager() const
{
    return qobject_cast<BrPager *>(parent());
}

BrReport *BrPager::report() const
{
    for (QObject *p = parent(); p; p = p->parent())
        if (auto *r = qobject_cast<BrReport *>(p))
            return r;
    return nullptr;
}

// ---------------------------------------------------------------------------
// BrGroup
// ---------------------------------------------------------------------------

BrGroup::BrGroup(BrPager *parent) : BrPager(parent) {}

void BrGroup::setDataFields(const QString &fields)
{
    m_dataFields = fields;
    if (!fields.isEmpty())
        m_dataFormula.clear();
}

void BrGroup::setDataFormula(const QString &expression)
{
    m_dataFormula = expression;
    if (!expression.isEmpty())
        m_dataFields.clear();
}

QString BrGroup::key(BrRenderContext &ctx) const
{
    if (m_keyFunction)
        return m_keyFunction(ctx).toString();
    if (!m_dataFormula.isEmpty())
        return BrExpression::evaluate(m_dataFormula, ctx).toString();
    QStringList parts;
    const QStringList fields = m_dataFields.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &field : fields)
        parts << ctx.fieldValue(field.trimmed()).toString();
    // Unit separator avoids accidental collisions such as "ab"+"c" vs "a"+"bc".
    return parts.join(QChar(0x1F));
}

// ---------------------------------------------------------------------------
// BrSubDetail
// ---------------------------------------------------------------------------

BrSubDetail::BrSubDetail(BrPager *parent, BrDataSource *dataSource)
    : BrPager(parent), m_dataSource(dataSource)
{
}

BrDataSource *BrSubDetail::dataSource() const
{
    if (m_dataSource)
        return m_dataSource;
    BrReport *r = report();
    return r && !m_dataSourceName.isEmpty() ? r->findDataSource(m_dataSourceName) : nullptr;
}

void BrSubDetail::callBeforeOpen(BrRenderContext &ctx)
{
    if (m_beforeOpen)
        m_beforeOpen(*this, ctx);
}
