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

#ifndef BRREPORTFILE_H
#define BRREPORTFILE_H

#include "brglobal.h"

#include <QByteArray>
#include <QJsonValue>
#include <QMetaProperty>
#include <QStringList>
#include <QVariant>

#include <functional>

class BrReport;

/*!
 * Report files: the layout of a report (bands, groups, sub-details and objects
 * with their properties) stored as JSON, usually with the ".brt" extension.
 *
 * Every Q_PROPERTY of the objects is saved when it differs from the default
 * value, so files stay small and readable. Data sources are not stored: they are
 * referred to by name and registered by the application with
 * BrReport::addDataSource() / setDataSource(name, ...).
 *
 * See docs/report-file-format.md.
 */
class BRREPORT_EXPORT BrReportFile
{
public:
    static constexpr int FormatVersion = 1;

    using Factory = std::function<QObject *()>;

    /*!
     * Registers a custom object type (a BrControl, BrBand or BrPager subclass).
     * \a typeName is the class name without a leading "Br" (e.g. "MyStamp").
     */
    static void registerType(const QString &typeName, const Factory &factory);
    static QStringList typeNames();
    /*! Type name used in files for \a object ("Label", "Band", "Group"...). */
    static QString typeName(const QObject *object);
    /*! Creates an object of a registered type, or nullptr. */
    static QObject *create(const QString &typeName);

    static QByteArray toJson(const BrReport *report, bool compact = false);
    /*! Replaces the layout of \a report. Returns false (and sets \a error) on failure. */
    static bool fromJson(BrReport *report, const QByteArray &json, QString *error = nullptr,
                         QStringList *warnings = nullptr);

    // Property conversion, also useful for property editors.
    static QJsonValue propertyToJson(const QVariant &value, const QMetaProperty &property);
    static QVariant propertyFromJson(const QJsonValue &value, const QMetaProperty &property,
                                     bool *ok = nullptr);
};

#endif // BRREPORTFILE_H
