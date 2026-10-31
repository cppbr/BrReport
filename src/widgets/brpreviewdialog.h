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

#ifndef BRPREVIEWDIALOG_H
#define BRPREVIEWDIALOG_H

#include "brdocument.h"
#include "brwidgetsglobal.h"

#include <QDialog>

class BrPreviewWidget;
class QComboBox;
class QLineEdit;
class QLabel;
class QSpinBox;
class QToolBar;

/*!
 * Ready-to-use preview window: page navigation, zoom,
 * printing and export to PDF, HTML, PNG and the BrReport document format.
 */
class BRREPORT_WIDGETS_EXPORT BrPreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BrPreviewDialog(QWidget *parent = nullptr);

    void setDocument(const BrDocument &document);
    const BrDocument &document() const;

    BrPreviewWidget *previewWidget() const { return m_view; }
    QToolBar *toolBar() const { return m_toolBar; }

public slots:
    void print();
    void exportDocument();
    void openDocument();

private:
    void updatePageControls(int page);
    void updateZoomCombo(double zoom);

    BrPreviewWidget *m_view;
    QToolBar *m_toolBar;
    QSpinBox *m_pageSpin;
    QLabel *m_pageCountLabel;
    QComboBox *m_zoomCombo;
    QLineEdit *m_findEdit;
    QLabel *m_findLabel;
    QString m_lastSearch;
};

#endif // BRPREVIEWDIALOG_H
