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

#ifndef BRRENDERCONTEXT_H
#define BRRENDERCONTEXT_H

#include "brexpression.h"

#include <QDateTime>

class BrReport;
class BrDataSource;

/*!
 * Information available to controls while a report is being prepared.
 * It is also the BrExpressionContext used to evaluate expressions: identifiers
 * resolve to data fields (innermost data source first) and report functions
 * such as page(), pages(), recno() and title() are available.
 */
class BRREPORT_EXPORT BrRenderContext : public BrExpressionContext
{
public:
    ~BrRenderContext() override;

    virtual BrReport *report() const = 0;
    /*! Current page number (1-based). */
    virtual int pageNumber() const = 0;
    /*!
     * Placeholder text for the total number of pages. The real number is only
     * known at the end, so the placeholder is replaced in the prepared document.
     */
    virtual QString pageCountText() const = 0;
    /*! Record number (1-based) of the innermost data pager. */
    virtual int recNo() const = 0;
    /*! Number of detail records processed so far. */
    virtual int detailCount() const = 0;
    /*! Current carbon copy of the band being printed (1-based). */
    virtual int carbonCopy() const = 0;
    /*! Value of a data field; looks up the innermost data source first. */
    virtual QVariant fieldValue(const QString &name, bool *found = nullptr) const = 0;
    /*! Date/time of the report (BrReport::reportDateTime()). */
    virtual QDateTime reportDateTime() const = 0;

    QVariant evaluate(const BrExpression &expression) const { return expression.evaluate(*this); }
};

#endif // BRRENDERCONTEXT_H
