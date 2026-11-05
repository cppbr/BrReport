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

#include "brdesignview_p.h"

#include "brdesignscene_p.h"

#include <QMouseEvent>
#include <QPainter>

#include <cmath>

BrRuler::BrRuler(Qt::Orientation orientation, BrDesignView *view)
    : QWidget(view), m_orientation(orientation), m_view(view)
{
    setCursor(orientation == Qt::Horizontal ? Qt::SplitVCursor : Qt::SplitHCursor);
    setToolTip(tr("Drag from here to create a guide"));
}

int BrRuler::labelStep(double pixelsPerMm)
{
    for (int step : {1, 5, 10, 20, 50, 100, 200, 500})
        if (step * pixelsPerMm >= 40.0)
            return step;
    return 1000;
}

void BrRuler::setMarker(int position)
{
    if (m_marker != position) {
        m_marker = position;
        update();
    }
}

double BrRuler::sceneValue(int pixel) const
{
    const QPoint p = m_orientation == Qt::Horizontal ? QPoint(pixel, 0) : QPoint(0, pixel);
    const QPointF scenePos = m_view->mapToScene(p);
    return m_orientation == Qt::Horizontal ? scenePos.x() : scenePos.y();
}

void BrRuler::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());
    painter.setPen(palette().color(QPalette::Mid));
    if (m_orientation == Qt::Horizontal)
        painter.drawLine(rect().bottomLeft(), rect().bottomRight());
    else
        painter.drawLine(rect().topRight(), rect().bottomRight());

    const int thickness = m_orientation == Qt::Horizontal ? height() : width();
    const int length = m_orientation == Qt::Horizontal ? width() : height();
    const double scale = m_view->transform().m11();
    const int step = labelStep(scale);
    const double tick = step / 5.0; // 5 ticks per label
    const bool drawSmallTicks = tick * scale >= 4.0;

    QFont font = painter.font();
    font.setPointSizeF(7);
    painter.setFont(font);
    painter.setPen(palette().color(QPalette::WindowText));

    const double start = sceneValue(0);
    for (qint64 n = qint64(std::floor(start / tick));; ++n) {
        const double value = n * tick;
        const int pixel = qRound((value - start) * scale);
        if (pixel > length)
            break;
        const bool labeled = n % 5 == 0;
        if (!labeled && !drawSmallTicks)
            continue;
        const int size = labeled ? thickness - 2 : thickness / 4;
        if (m_orientation == Qt::Horizontal) {
            painter.drawLine(pixel, thickness - size, pixel, thickness);
            if (labeled)
                painter.drawText(pixel + 3, 10, QString::number(qRound(value)));
        } else {
            painter.drawLine(thickness - size, pixel, thickness, pixel);
            if (labeled) {
                painter.save();
                painter.translate(10, pixel - 3);
                painter.rotate(-90);
                painter.drawText(0, 0, QString::number(qRound(value)));
                painter.restore();
            }
        }
    }

    if (m_marker >= 0) {
        painter.setPen(palette().color(QPalette::Highlight));
        if (m_orientation == Qt::Horizontal)
            painter.drawLine(m_marker, 0, m_marker, thickness);
        else
            painter.drawLine(0, m_marker, thickness, m_marker);
    }
}

QPoint BrRuler::viewportPos(QMouseEvent *event) const
{
    return m_view->viewport()->mapFromGlobal(mapToGlobal(event->pos()));
}

void BrRuler::moveGuide(const QPoint &viewportPos)
{
    if (!m_guide)
        return;
    const QPointF p = m_view->mapToScene(viewportPos);
    m_guide->setPosition(m_view->designScene()->snap(m_orientation == Qt::Horizontal ? p.y() : p.x()));
}

void BrRuler::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_guide = m_view->designScene()->addGuide(m_orientation, 0);
    moveGuide(viewportPos(event));
}

void BrRuler::mouseMoveEvent(QMouseEvent *event)
{
    moveGuide(viewportPos(event));
}

void BrRuler::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_guide)
        return;
    BrGuideItem *guide = m_guide;
    m_guide = nullptr;
    m_view->designScene()->finishGuide(guide, m_view->viewport()->rect().contains(viewportPos(event)));
}

BrDesignView::BrDesignView(BrDesignScene *scene, QWidget *parent)
    : QGraphicsView(scene, parent),
      m_top(new BrRuler(Qt::Horizontal, this)),
      m_left(new BrRuler(Qt::Vertical, this)),
      m_corner(new QWidget(this))
{
    m_corner->setAutoFillBackground(true);
    setViewportMargins(RulerSize, RulerSize, 0, 0);
    connect(scene, &QGraphicsScene::sceneRectChanged, this, &BrDesignView::updateRulers);
}

BrDesignScene *BrDesignView::designScene() const
{
    return static_cast<BrDesignScene *>(scene());
}

void BrDesignView::setRulersVisible(bool visible)
{
    m_rulersVisible = visible;
    m_top->setVisible(visible);
    m_left->setVisible(visible);
    m_corner->setVisible(visible);
    const int margin = visible ? RulerSize : 0;
    setViewportMargins(margin, margin, 0, 0);
    placeRulers();
}

void BrDesignView::updateRulers()
{
    m_top->update();
    m_left->update();
}

void BrDesignView::placeRulers()
{
    const QRect area = viewport()->geometry();
    m_top->setGeometry(area.left(), area.top() - RulerSize, area.width(), RulerSize);
    m_left->setGeometry(area.left() - RulerSize, area.top(), RulerSize, area.height());
    m_corner->setGeometry(area.left() - RulerSize, area.top() - RulerSize, RulerSize, RulerSize);
}

void BrDesignView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    placeRulers();
}

void BrDesignView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    updateRulers();
}

void BrDesignView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
    m_top->setMarker(event->pos().x());
    m_left->setMarker(event->pos().y());
}

void BrDesignView::leaveEvent(QEvent *event)
{
    QGraphicsView::leaveEvent(event);
    m_top->setMarker(-1);
    m_left->setMarker(-1);
}
