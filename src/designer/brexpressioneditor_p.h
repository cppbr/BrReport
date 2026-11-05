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

#ifndef BREXPRESSIONEDITOR_P_H
#define BREXPRESSIONEDITOR_P_H

//
//  W A R N I N G
//  -------------
// This file is not part of the BrReport API. It may change without notice.
//

#include <QDialog>
#include <QList>
#include <QSet>
#include <QStringList>

class BrReport;
class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

/*!
 * Designer dialog that writes a formula: lists the fields, parameters, variables,
 * functions and operators available to an object and checks the text as it is typed.
 *
 * A syntax error blocks the OK button. A name that cannot be found is only a warning,
 * because the application may provide it when the report runs.
 */
class BrExpressionEditor : public QDialog
{
    Q_OBJECT
public:
    struct Function
    {
        QString category;
        QString name;
        QString signature;
        QString description;
    };

    /*! Edits the formula \a text of the property \a property of \a object. */
    BrExpressionEditor(QObject *object, const QByteArray &property, const QString &text,
                       QWidget *parent = nullptr);

    /*! The formula in a single line. */
    QString formula() const;

    /*! Functions shown to the user, grouped by category. */
    static QList<Function> functionCatalog();
    /*! Every function name the report engine knows, including aliases. */
    static QStringList knownFunctions();
    /*! Writes a field name as a formula expects it ([Field With Spaces]). */
    static QString quotedName(const QString &name);

private:
    void buildTree();
    void validate();
    void insertName(const QString &name);
    void insertFunction(const QString &name, bool hasArguments);
    void insertOperator(const QString &op);
    void activateItem(QTreeWidgetItem *item);
    void showDescription();
    bool isKnownName(const QString &name) const;
    QStringList unknownNames() const;

    QObject *m_object = nullptr;
    BrReport *m_report = nullptr;
    QSet<QString> m_names;
    bool m_hasFields = false;
    QPlainTextEdit *m_edit = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_description = nullptr;
    QLabel *m_status = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
};

#endif // BREXPRESSIONEDITOR_P_H
