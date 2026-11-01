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

#include "brdesigncontext.h"

#include "brcontrol.h"
#include "brreport.h"

BrDesignContext::BrDesignContext(BrReport *report) : m_report(report) {}

QVariant BrDesignContext::fieldValue(const QString &name, bool *found) const
{
    if (found)
        *found = true;
    return QString(QLatin1Char('[') + name + QLatin1Char(']'));
}

QDateTime BrDesignContext::reportDateTime() const
{
    return QDateTime::currentDateTime();
}

QVariant BrDesignContext::variable(const QString &name, bool *found) const
{
    return fieldValue(name, found);
}

QLocale BrDesignContext::locale() const
{
    return m_report ? m_report->locale() : QLocale();
}

QList<BrPageItem> BrDesignContext::renderControl(BrControl *control, const QSizeF &size)
{
    BrDesignContext ctx(control->report());
    BrPage page;
    page.size = size;
    const QRectF rect(QPointF(0, 0), size);
    if (qobject_cast<BrContainer *>(control)) {
        control->BrControl::render(ctx, rect, page); // frame only
    } else {
        control->prepare(ctx);
        control->render(ctx, rect, page);
    }
    return page.items;
}

QList<QPair<BrControl *, QRectF>> BrDesignContext::layout(BrContainer *container, const QSizeF &size)
{
    BrDesignContext ctx(container->report());
    container->prepare(ctx);
    QList<BrContainer::Placement> placements;
    container->layoutChildren(ctx, size.width(), size.height(), false, &placements);
    QList<QPair<BrControl *, QRectF>> result;
    for (const BrContainer::Placement &p : placements)
        result << qMakePair(p.control, p.rect);
    return result;
}
