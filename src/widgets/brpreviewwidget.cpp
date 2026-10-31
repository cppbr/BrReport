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

#include "brpreviewwidget.h"

#include "brrenderer.h"

#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

namespace {
const int kMargin = 16;    // px around and between pages
const double kMinZoom = 0.1;
const double kMaxZoom = 8.0;
} // namespace

BrPreviewWidget::BrPreviewWidget(QWidget *parent) : QAbstractScrollArea(parent)
{
    viewport()->setBackgroundRole(QPalette::Dark);
    viewport()->setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus);
    horizontalScrollBar()->setSingleStep(20);
    verticalScrollBar()->setSingleStep(20);
}

void BrPreviewWidget::setDocument(const BrDocument &document)
{
    m_document = document;
    applyZoomMode();
    updateLayout();
    verticalScrollBar()->setValue(0);
    m_lastPage = m_document.isEmpty() ? -1 : 0;
    viewport()->update();
    emit currentPageChanged(m_lastPage);
}

double BrPreviewWidget::pixelsPerMm() const
{
    return logicalDpiX() / 25.4 * m_zoom;
}

void BrPreviewWidget::updateLayout()
{
    const double k = pixelsPerMm();
    m_pageTops.clear();
    double y = kMargin;
    double maxWidth = 0.0;
    for (const BrPage &page : m_document.pages) {
        m_pageTops << y;
        y += page.size.height() * k + kMargin;
        maxWidth = std::max(maxWidth, page.size.width() * k);
    }
    m_contentWidth = maxWidth + 2 * kMargin;
    m_contentHeight = y;

    const QSize vp = viewport()->size();
    horizontalScrollBar()->setRange(0, std::max(0, int(std::ceil(m_contentWidth)) - vp.width()));
    horizontalScrollBar()->setPageStep(vp.width());
    verticalScrollBar()->setRange(0, std::max(0, int(std::ceil(m_contentHeight)) - vp.height()));
    verticalScrollBar()->setPageStep(vp.height());
}

QRectF BrPreviewWidget::pageRect(int page) const
{
    const double k = pixelsPerMm();
    const QSizeF size = m_document.pages.at(page).size * k;
    const double areaWidth = std::max(m_contentWidth, double(viewport()->width()));
    const double x = (areaWidth - size.width()) / 2.0;
    return QRectF(x, m_pageTops.value(page), size.width(), size.height());
}

int BrPreviewWidget::currentPage() const
{
    if (m_document.isEmpty())
        return -1;
    const double y = verticalScrollBar()->value() + viewport()->height() / 3.0;
    int page = 0;
    for (int i = 0; i < m_pageTops.size(); ++i)
        if (m_pageTops.at(i) <= y)
            page = i;
    return page;
}

void BrPreviewWidget::paintEvent(QPaintEvent *)
{
    QPainter p(viewport());
    const QPointF scroll(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const QRectF visible(scroll, QSizeF(viewport()->size()));
    const double k = pixelsPerMm();
    for (int i = 0; i < m_document.pageCount(); ++i) {
        const QRectF r = pageRect(i);
        if (!r.intersects(visible))
            continue;
        const QRectF onScreen = r.translated(-scroll);
        p.fillRect(onScreen.translated(3, 3), QColor(0, 0, 0, 80));
        p.fillRect(onScreen, Qt::white);
        p.save();
        p.setClipRect(onScreen);
        BrRenderer::renderPage(&p, m_document.pages.at(i), k, onScreen.topLeft());
        p.restore();
    }
}

void BrPreviewWidget::scrollContentsBy(int, int)
{
    viewport()->update();
    const int page = currentPage();
    if (page != m_lastPage) {
        m_lastPage = page;
        emit currentPageChanged(page);
    }
}

void BrPreviewWidget::applyZoomMode()
{
    if (m_zoomMode == ZoomMode::Custom || m_document.isEmpty())
        return;
    double maxW = 0.0, maxH = 0.0;
    for (const BrPage &page : m_document.pages) {
        maxW = std::max(maxW, page.size.width());
        maxH = std::max(maxH, page.size.height());
    }
    const double base = logicalDpiX() / 25.4;
    // Reserve room for the vertical scroll bar to avoid oscillation.
    const double availW = viewport()->width() - 2 * kMargin
                          - (verticalScrollBar()->isVisible() ? 0 : verticalScrollBar()->sizeHint().width());
    const double availH = viewport()->height() - 2 * kMargin;
    double z = availW / (maxW * base);
    if (m_zoomMode == ZoomMode::FitPage)
        z = std::min(z, availH / (maxH * base));
    z = std::max(kMinZoom, std::min(kMaxZoom, z));
    if (!qFuzzyCompare(z, m_zoom)) {
        m_zoom = z;
        emit zoomChanged(m_zoom);
    }
}

void BrPreviewWidget::setZoom(double zoom)
{
    zoom = std::max(kMinZoom, std::min(kMaxZoom, zoom));
    const int page = currentPage();
    m_zoomMode = ZoomMode::Custom;
    if (qFuzzyCompare(zoom, m_zoom))
        return;
    // Keep the relative position inside the document.
    const double rel = m_contentHeight > 0 ? verticalScrollBar()->value() / m_contentHeight : 0.0;
    m_zoom = zoom;
    updateLayout();
    verticalScrollBar()->setValue(int(rel * m_contentHeight));
    if (page >= 0 && rel == 0.0)
        goToPage(page);
    viewport()->update();
    emit zoomChanged(m_zoom);
}

void BrPreviewWidget::setZoomMode(ZoomMode mode)
{
    const int page = currentPage();
    m_zoomMode = mode;
    applyZoomMode();
    updateLayout();
    if (page >= 0)
        goToPage(page);
    viewport()->update();
}

void BrPreviewWidget::zoomIn()
{
    setZoom(m_zoom * 1.25);
}

void BrPreviewWidget::zoomOut()
{
    setZoom(m_zoom / 1.25);
}

void BrPreviewWidget::goToPage(int page)
{
    if (page < 0 || page >= m_pageTops.size())
        return;
    verticalScrollBar()->setValue(int(m_pageTops.at(page)) - kMargin);
    viewport()->update();
    if (page != m_lastPage) {
        m_lastPage = page;
        emit currentPageChanged(page);
    }
}

void BrPreviewWidget::firstPage()
{
    goToPage(0);
}

void BrPreviewWidget::previousPage()
{
    goToPage(std::max(0, currentPage() - 1));
}

void BrPreviewWidget::nextPage()
{
    goToPage(std::min(pageCount() - 1, currentPage() + 1));
}

void BrPreviewWidget::lastPage()
{
    goToPage(pageCount() - 1);
}

void BrPreviewWidget::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    // The layout still reflects the old size here, so do not recompute the page.
    const int page = m_lastPage;
    const bool fit = m_zoomMode != ZoomMode::Custom;
    applyZoomMode();
    updateLayout();
    if (fit && page >= 0)
        goToPage(page);
}

void BrPreviewWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else if (event->angleDelta().y() < 0)
            zoomOut();
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void BrPreviewWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_PageDown:
        nextPage();
        break;
    case Qt::Key_PageUp:
        previousPage();
        break;
    case Qt::Key_Home:
        firstPage();
        break;
    case Qt::Key_End:
        lastPage();
        break;
    case Qt::Key_Plus:
        zoomIn();
        break;
    case Qt::Key_Minus:
        zoomOut();
        break;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    event->accept();
}
