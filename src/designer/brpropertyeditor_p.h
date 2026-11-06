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

#ifndef BRPROPERTYEDITOR_P_H
#define BRPROPERTYEDITOR_P_H

//
//  W A R N I N G
//  -------------
// This file is not part of the BrReport API. It may change without notice.
//

#include "brglobal.h"

#include <QMetaProperty>
#include <QPointer>
#include <QScrollArea>

#include <functional>

class QFormLayout;

/*!
 * Edits the Q_PROPERTY values of the selected report objects. The editors are
 * generated from the property types; a change is applied to every selected
 * object that has the property.
 */
class BrPropertyEditor : public QScrollArea
{
    Q_OBJECT
public:
    explicit BrPropertyEditor(QWidget *parent = nullptr);

    void setObjects(const QList<QObject *> &objects);
    QList<QObject *> objects() const;

    /*! Shows the protection of the objects as editors, instead of obeying it. */
    void setDeveloperMode(bool on) { m_developerMode = on; }

    /*! Rebuilds the editors from the current values. */
    void refresh();

    /*! Friendly label for a property name ("dataField" -> "Data field"). */
    static QString labelFor(const QString &propertyName);

signals:
    /*! A value was changed by the user (already written to the objects). */
    void edited(const QString &propertyName);

private:
    QWidget *createEditor(const QMetaProperty &property, QObject *object);
    void apply(const char *name, const QVariant &value);
    void addProtectionEditors(QFormLayout *form, QObject *object);
    void applyProtection(const std::function<void(BrProtection &)> &change);
    void editHiddenProperties(QObject *object);

    QList<QPointer<QObject>> m_objects;
    QWidget *m_panel = nullptr;
    bool m_developerMode = false;
};

#endif // BRPROPERTYEDITOR_P_H
