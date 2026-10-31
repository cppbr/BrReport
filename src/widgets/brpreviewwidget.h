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

#ifndef BRPREVIEWWIDGET_H
#define BRPREVIEWWIDGET_H

#include "brdocument.h"
#include "brwidgetsglobal.h"

#include <QAbstractScrollArea>

/*!
 * Scrollable, zoomable view of a prepared document.
 * Pages are painted directly from the vector items, so text stays sharp at any zoom.
 */
class BRREPORT_WIDGETS_EXPORT BrPreviewWidget : public QAbstractScrollArea
{
    Q_OBJECT
public:
    enum class ZoomMode { Custom, FitWidth, FitPage };

    explicit BrPreviewWidget(QWidget *parent = nullptr);

    void setDocument(const BrDocument &document);
    const BrDocument &document() const { return m_document; }

    int pageCount() const { return m_document.pageCount(); }
    /*! Current page (0-based): the page at the top of the view. */
    int currentPage() const;

    /*!
     * Searches \a text (case-insensitive) in every page, highlights the matches and
     * shows the first one. Returns the number of matches.
     */
    int find(const QString &text);
    int matchCount() const { return int(m_matches.size()); }
    int currentMatch() const { return m_matchIndex; }

    /*! Zoom factor (1.0 = real size). */
    double zoom() const { return m_zoom; }
    ZoomMode zoomMode() const { return m_zoomMode; }

public slots:
    void setZoom(double zoom);
    void setZoomMode(ZoomMode mode);
    void zoomIn();
    void zoomOut();
    void goToPage(int page);
    void firstPage();
    void previousPage();
    void nextPage();
    void lastPage();
    void findNext();
    void findPrevious();
    void clearFind();

signals:
    /*! Emitted after a search or when moving between matches (\a index is 0-based, -1 = none). */
    void findResultChanged(int index, int count);
    void currentPageChanged(int page);
    void zoomChanged(double zoom);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    double pixelsPerMm() const;
    QRectF pageRect(int page) const; // in content coordinates
    void updateLayout();
    void applyZoomMode();

    BrDocument m_document;
    double m_zoom = 1.0;
    ZoomMode m_zoomMode = ZoomMode::FitWidth;
    QList<double> m_pageTops;
    double m_contentWidth = 0.0;
    double m_contentHeight = 0.0;
    int m_lastPage = -1;

    struct Match
    {
        int page;
        QRectF rect; // mm
    };
    QList<Match> m_matches;
    int m_matchIndex = -1;
    void showMatch(int index);
};

#endif // BRPREVIEWWIDGET_H
