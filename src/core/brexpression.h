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

#ifndef BREXPRESSION_H
#define BREXPRESSION_H

#include "brglobal.h"

#include <QLocale>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>

#include <functional>
#include <memory>

/*!
 * Environment used to evaluate an expression: resolves identifiers (data fields,
 * variables) and custom functions.
 */
class BRREPORT_EXPORT BrExpressionContext
{
public:
    virtual ~BrExpressionContext();
    /*! Returns the value of \a name. Set \a found to false if unknown. */
    virtual QVariant variable(const QString &name, bool *found) const;
    /*! Calls a non built-in function. Set \a found to false if unknown. */
    virtual QVariant function(const QString &name, const QVariantList &args, bool *found) const;
    /*! Locale used by formatting functions. */
    virtual QLocale locale() const;
};

/*!
 * Small expression language used by data-aware controls, groups and formulas
 * ("data formulas").
 *
 * Syntax:
 * - Literals: 12, 3.5, 'text', "text", true, false, null
 * - Identifiers resolve to data fields: Price, customer.name, [Field With Spaces]
 * - Operators: + - * / % (numbers), + or & (string concatenation),
 *   = == <> != < > <= >=, and or not (also && || !)
 * - Functions (case-insensitive): if/iif, upper, lower, trim, len, left, right,
 *   substr(s, start1, len), replace, padleft, padright, round(x[, n]), trunc, abs,
 *   min, max, str, num, isnull, coalesce, formatnumber(x, '#,##0.00'),
 *   formatdate(d, 'dd/MM/yyyy'), today, now, year, month, day.
 */
class BRREPORT_EXPORT BrExpression
{
public:
    struct Node;

    BrExpression();
    explicit BrExpression(const QString &source);

    void setSource(const QString &source);
    QString source() const { return m_source; }

    bool isEmpty() const { return m_source.trimmed().isEmpty(); }
    bool isValid() const { return m_root != nullptr; }
    QString errorString() const { return m_error; }

    /*! Fields and variables the expression refers to, without duplicates. */
    QStringList identifiers() const;
    /*! Functions the expression calls (in lower case), without duplicates. */
    QStringList functions() const;

    QVariant evaluate(const BrExpressionContext &context) const;
    /*! Convenience: evaluate with a simple identifier resolver. */
    QVariant evaluate(const std::function<QVariant(const QString &)> &resolver) const;

    static QVariant evaluate(const QString &source, const BrExpressionContext &context,
                             QString *error = nullptr);

private:
    QString m_source;
    QString m_error;
    std::shared_ptr<const Node> m_root;
};

#endif // BREXPRESSION_H
