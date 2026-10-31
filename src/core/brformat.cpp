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

#include "brformat.h"

#include <QDate>
#include <QDateTime>
#include <QStringList>
#include <QTime>

#include <cmath>

namespace {

struct MaskParts
{
    QString prefix, intPart, fracPart, suffix;
    bool hasDecimal = false;
};

bool isMaskChar(QChar c)
{
    return c == QLatin1Char('0') || c == QLatin1Char('#') || c == QLatin1Char('.')
           || c == QLatin1Char(',');
}

QString unquote(const QString &s)
{
    QString out;
    bool inQuote = false;
    QChar quote;
    for (QChar c : s) {
        if (inQuote) {
            if (c == quote)
                inQuote = false;
            else
                out += c;
        } else if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
            inQuote = true;
            quote = c;
        } else {
            out += c;
        }
    }
    return out;
}

MaskParts splitMask(const QString &section)
{
    // Find the numeric mask outside of quotes.
    int first = -1, last = -1;
    bool inQuote = false;
    QChar quote;
    for (int i = 0; i < section.size(); ++i) {
        const QChar c = section.at(i);
        if (inQuote) {
            if (c == quote)
                inQuote = false;
            continue;
        }
        if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
            inQuote = true;
            quote = c;
            continue;
        }
        if (isMaskChar(c)) {
            if (first < 0)
                first = i;
            last = i;
        }
    }
    MaskParts parts;
    if (first < 0) {
        parts.prefix = unquote(section);
        return parts;
    }
    parts.prefix = unquote(section.left(first));
    parts.suffix = unquote(section.mid(last + 1));
    const QString mask = section.mid(first, last - first + 1);
    const int dot = mask.indexOf(QLatin1Char('.'));
    parts.hasDecimal = dot >= 0;
    parts.intPart = parts.hasDecimal ? mask.left(dot) : mask;
    parts.fracPart = parts.hasDecimal ? mask.mid(dot + 1) : QString();
    parts.fracPart.remove(QLatin1Char(','));
    return parts;
}

} // namespace

QString BrFormat::formatNumber(double value, const QString &mask, const QLocale &locale)
{
    if (mask.isEmpty())
        return format(QVariant(value), QString(), locale);

    const QStringList sections = mask.split(QLatin1Char(';'));
    QString section = sections.first();
    bool showSign = value < 0;
    if (value < 0 && sections.size() > 1 && !sections.at(1).isEmpty()) {
        section = sections.at(1);
        showSign = false; // the negative section defines its own sign
    } else if (value == 0.0 && sections.size() > 2 && !sections.at(2).isEmpty()) {
        section = sections.at(2);
    }

    const MaskParts parts = splitMask(section);
    if (parts.intPart.isEmpty() && parts.fracPart.isEmpty())
        return parts.prefix + parts.suffix;

    const int maxDec = parts.fracPart.count(QLatin1Char('0')) + parts.fracPart.count(QLatin1Char('#'));
    int minDec = 0;
    for (QChar c : parts.fracPart) {
        if (c == QLatin1Char('0'))
            ++minDec;
        else
            break;
    }
    const bool grouping = parts.intPart.contains(QLatin1Char(','));
    const int minInt = parts.intPart.count(QLatin1Char('0'));

    QString digits = QString::number(std::abs(value), 'f', maxDec);
    QString intDigits = digits, fracDigits;
    const int dot = digits.indexOf(QLatin1Char('.'));
    if (dot >= 0) {
        intDigits = digits.left(dot);
        fracDigits = digits.mid(dot + 1);
    }
    while (fracDigits.size() > minDec && fracDigits.endsWith(QLatin1Char('0')))
        fracDigits.chop(1);
    if (intDigits == QLatin1String("0") && minInt == 0)
        intDigits.clear();
    while (intDigits.size() < minInt)
        intDigits.prepend(QLatin1Char('0'));

    // A value that rounds to zero must not show a minus sign.
    if (showSign) {
        bool allZero = true;
        for (QChar c : intDigits + fracDigits)
            if (c != QLatin1Char('0'))
                allZero = false;
        if (allZero)
            showSign = false;
    }

    if (grouping && intDigits.size() > 3) {
        const QString sep = QString(locale.groupSeparator());
        QString grouped;
        int count = 0;
        for (int i = intDigits.size() - 1; i >= 0; --i) {
            grouped.prepend(intDigits.at(i));
            if (++count % 3 == 0 && i > 0)
                grouped.prepend(sep);
        }
        intDigits = grouped;
    }

    QString result = intDigits;
    if (!fracDigits.isEmpty())
        result += QString(locale.decimalPoint()) + fracDigits;
    if (result.isEmpty())
        result = QStringLiteral("0");
    return (showSign ? QString(locale.negativeSign()) : QString()) + parts.prefix + result
           + parts.suffix;
}

bool BrFormat::isNumeric(const QVariant &value)
{
    switch (value.userType()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Long:
    case QMetaType::ULong:
        return true;
    case QMetaType::QString: {
        bool ok = false;
        value.toString().trimmed().toDouble(&ok);
        return ok;
    }
    default:
        return false;
    }
}

bool BrFormat::isDateTime(const QVariant &value)
{
    const int t = value.userType();
    return t == QMetaType::QDate || t == QMetaType::QDateTime || t == QMetaType::QTime;
}

QString BrFormat::format(const QVariant &value, const QString &displayFormat,
                         const QLocale &locale)
{
    if (!value.isValid() || value.isNull())
        return QString();

    const int type = value.userType();
    if (type == QMetaType::QDate) {
        return displayFormat.isEmpty() ? locale.toString(value.toDate(), QLocale::ShortFormat)
                                       : locale.toString(value.toDate(), displayFormat);
    }
    if (type == QMetaType::QDateTime) {
        return displayFormat.isEmpty() ? locale.toString(value.toDateTime(), QLocale::ShortFormat)
                                       : locale.toString(value.toDateTime(), displayFormat);
    }
    if (type == QMetaType::QTime) {
        return displayFormat.isEmpty() ? locale.toString(value.toTime(), QLocale::ShortFormat)
                                       : locale.toString(value.toTime(), displayFormat);
    }
    if (type == QMetaType::Bool)
        return value.toBool() ? QStringLiteral("True") : QStringLiteral("False");

    if (type == QMetaType::QString) {
        const QString s = value.toString();
        if (displayFormat.isEmpty())
            return s;
        if (isNumeric(value))
            return formatNumber(s.trimmed().toDouble(), displayFormat, locale);
        // Databases such as SQLite return dates as ISO 8601 text.
        const QString t = s.trimmed();
        if (t.size() == 10) {
            const QDate d = QDate::fromString(t, Qt::ISODate);
            if (d.isValid())
                return locale.toString(d, displayFormat);
        } else if (t.size() > 10) {
            const QDateTime dt = QDateTime::fromString(t, Qt::ISODate);
            if (dt.isValid())
                return locale.toString(dt, displayFormat);
        }
        return s;
    }

    if (isNumeric(value)) {
        if (!displayFormat.isEmpty())
            return formatNumber(value.toDouble(), displayFormat, locale);
        if (type == QMetaType::Double || type == QMetaType::Float) {
            QLocale l = locale;
            l.setNumberOptions(l.numberOptions() | QLocale::OmitGroupSeparator);
            return l.toString(value.toDouble(), 'f', QLocale::FloatingPointShortest);
        }
        return value.toString();
    }
    return value.toString();
}
