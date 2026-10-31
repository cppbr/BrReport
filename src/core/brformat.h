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

#ifndef BRFORMAT_H
#define BRFORMAT_H

#include "brglobal.h"

#include <QLocale>
#include <QString>
#include <QVariant>

/*!
 * Value formatting used by data-aware controls (DisplayFormat).
 */
class BRREPORT_EXPORT BrFormat
{
public:
    /*!
     * Formats a number with a picture mask, using the locale separators:
     * - '0' mandatory digit, '#' optional digit, '.' decimal point, ',' thousands separator
     * - Literal text before/after the mask is kept; quote it to be safe: "'R$ '#,##0.00"
     * - Up to three sections separated by ';': positive;negative;zero
     */
    static QString formatNumber(double value, const QString &mask,
                                const QLocale &locale = QLocale());

    /*!
     * Formats any value. Numbers use formatNumber(), dates/times use Qt date
     * patterns (e.g. "dd/MM/yyyy HH:mm"). Without a format, a sensible default
     * representation in \a locale is used.
     */
    static QString format(const QVariant &value, const QString &displayFormat = QString(),
                          const QLocale &locale = QLocale());

    /*! True if the variant holds a number (or a string that is a valid number). */
    static bool isNumeric(const QVariant &value);
    /*! True if the variant holds a QDate, QTime or QDateTime. */
    static bool isDateTime(const QVariant &value);
};

#endif // BRFORMAT_H
