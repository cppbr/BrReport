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

#include "brdesignscene_p.h"

#include "brband.h"
#include "brcontrol.h"
#include "brdesigncontext.h"
#include "brpager.h"
#include "brrenderer.h"
#include "brreport.h"

#include <QCursor>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QJsonDocument>
#include <QMimeData>
#include <QPainter>
#include <QPaintDevice>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

const QColor kSelection(0x1f, 0x6f, 0xd1);
const QColor kOutline(0xa0, 0xaa, 0xb8);
const QColor kGuide(0x00, 0x9c, 0xb0);
const QColor kSnapLine(0xd8, 0x1b, 0x8c);

void paintItems(QPainter *painter, const QList<BrPageItem> &items)
{
    if (items.isEmpty())
        return;
    painter->save();
    painter->scale(1.0 / BrRenderer::UnitsPerMm, 1.0 / BrRenderer::UnitsPerMm);
    const double dpi = painter->device() ? painter->device()->logicalDpiY() : 96.0;
    for (const BrPageItem &item : items)
        BrRenderer::renderItem(painter, item, dpi);
    painter->restore();
}

QString bandTypeName(Br::BandType t)
{
    switch (t) {
    case Br::BandType::Header:
        return QObject::tr("Header");
    case Br::BandType::Title:
        return QObject::tr("Title");
    case Br::BandType::ColumnHeader:
        return QObject::tr("Column header");
    case Br::BandType::Detail:
        return QObject::tr("Detail");
    case Br::BandType::ColumnFooter:
        return QObject::tr("Column footer");
    case Br::BandType::Summary:
        return QObject::tr("Summary");
    case Br::BandType::Footer:
        return QObject::tr("Footer");
    }
    return QString();
}

} // namespace

// ---------------------------------------------------------------------------
// BrControlItem
// ---------------------------------------------------------------------------

BrControlItem::BrControlItem(BrControl *control, const QRectF &rect, bool movable, QGraphicsItem *parent)
    : QGraphicsItem(parent), m_control(control), m_size(rect.size()), m_movable(movable)
{
    setPos(rect.topLeft());
    setFlag(ItemIsSelectable);
    if (movable)
        setFlag(ItemIsMovable);
    setAcceptHoverEvents(true);
    refreshItems();
}

BrDesignScene *BrControlItem::designScene() const
{
    return qobject_cast<BrDesignScene *>(scene());
}

void BrControlItem::refreshItems()
{
    m_items = m_control ? BrDesignContext::renderControl(m_control, m_size) : QList<BrPageItem>();
}

double BrControlItem::handleSize() const
{
    const BrDesignScene *s = designScene();
    return 7.0 / (s ? std::max(0.5, s->viewScale()) : 4.0);
}

QRectF BrControlItem::handleRect(int handle) const
{
    const double w = m_size.width(), h = m_size.height(), hs = handleSize();
    static const double fx[] = {0, 0.5, 1, 1, 1, 0.5, 0, 0};
    static const double fy[] = {0, 0, 0, 0.5, 1, 1, 1, 0.5};
    return QRectF(fx[handle] * w - hs / 2, fy[handle] * h - hs / 2, hs, hs);
}

int BrControlItem::handleAt(const QPointF &pos) const
{
    if (!isSelected() || !m_movable)
        return -1;
    for (int i = 0; i < 8; ++i)
        if (handleRect(i).adjusted(-0.3, -0.3, 0.3, 0.3).contains(pos))
            return i;
    return -1;
}

QRectF BrControlItem::boundingRect() const
{
    const double hs = handleSize();
    return QRectF(QPointF(0, 0), m_size).adjusted(-hs, -hs, hs, hs);
}

void BrControlItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const QRectF r(QPointF(0, 0), m_size);
    paintItems(painter, m_items);
    QPen outline(kOutline, 0, Qt::DotLine);
    outline.setCosmetic(true);
    painter->setPen(outline);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(r);
    if (isSelected()) {
        QPen pen(kSelection, 1.5);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->drawRect(r);
        if (m_movable) {
            painter->setBrush(Qt::white);
            for (int i = 0; i < 8; ++i)
                painter->drawRect(handleRect(i));
        }
    }
}

void BrControlItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    static const Qt::CursorShape shapes[] = {Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor,
                                             Qt::SizeHorCursor,   Qt::SizeFDiagCursor, Qt::SizeVerCursor,
                                             Qt::SizeBDiagCursor, Qt::SizeHorCursor};
    const int h = handleAt(event->pos());
    setCursor(h >= 0 ? shapes[h] : (m_movable ? Qt::SizeAllCursor : Qt::ArrowCursor));
    QGraphicsItem::hoverMoveEvent(event);
}

void BrControlItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_handle = event->button() == Qt::LeftButton ? handleAt(event->pos()) : -1;
    if (m_handle >= 0) {
        m_pressPos = event->scenePos();
        m_startRect = QRectF(pos(), m_size);
        if (BrDesignScene *s = designScene())
            s->beginResize(this);
        event->accept();
        return;
    }
    QGraphicsItem::mousePressEvent(event); // selects the item
    m_moving = false;
}

void BrControlItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    BrDesignScene *s = designScene();
    if (m_handle < 0) {
        if (!s || !m_movable || !(event->buttons() & Qt::LeftButton))
            return;
        // A small slip of the mouse in a click must not pull the object to a snap line.
        if (!m_moving) {
            const QPoint moved = event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton);
            if (moved.manhattanLength() < 3)
                return;
            m_moving = true;
            s->beginMove(this);
        }
        s->moveSelection(event->scenePos() - event->buttonDownScenePos(Qt::LeftButton));
        return;
    }
    const QPointF d = event->scenePos() - m_pressPos;
    QRectF r = m_startRect;
    auto sn = [this, s](Qt::Orientation axis, double v) { return s ? s->snapEdge(this, axis, v) : v; };
    if (s)
        s->clearSnapLines();
    if (m_handle == 0 || m_handle == 6 || m_handle == 7)
        r.setLeft(std::min(sn(Qt::Horizontal, m_startRect.left() + d.x()), r.right() - 1.0));
    if (m_handle == 2 || m_handle == 3 || m_handle == 4)
        r.setRight(std::max(sn(Qt::Horizontal, m_startRect.right() + d.x()), r.left() + 1.0));
    if (m_handle == 0 || m_handle == 1 || m_handle == 2)
        r.setTop(std::min(sn(Qt::Vertical, m_startRect.top() + d.y()), r.bottom() - 1.0));
    if (m_handle == 4 || m_handle == 5 || m_handle == 6)
        r.setBottom(std::max(sn(Qt::Vertical, m_startRect.bottom() + d.y()), r.top() + 1.0));
    prepareGeometryChange();
    setPos(r.topLeft());
    m_size = r.size();
    refreshItems();
    update();
}

void BrControlItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_handle < 0) {
        QGraphicsItem::mouseReleaseEvent(event);
        return;
    }
    m_handle = -1;
    const QRectF r(pos(), m_size);
    if (m_control && r != m_startRect) {
        m_control->setGeometry(r);
        if (BrDesignScene *s = designScene())
            s->notifyLayoutEdited();
    }
}

// ---------------------------------------------------------------------------
// BrBandItem
// ---------------------------------------------------------------------------

BrBandItem::BrBandItem(BrBand *band, const QString &caption, double width, int depth)
    : m_band(band), m_caption(caption), m_width(width), m_height(band->height()), m_depth(depth)
{
    setFlag(ItemIsSelectable);
    setAcceptHoverEvents(true);
    m_items = BrDesignContext::renderControl(band, QSizeF(width, m_height));
}

BrDesignScene *BrBandItem::designScene() const
{
    return qobject_cast<BrDesignScene *>(scene());
}

QRectF BrBandItem::boundingRect() const
{
    // A little below the band, so that its bottom edge can be grabbed to resize it.
    return QRectF(0, -CaptionHeight, m_width, m_height + CaptionHeight + 1.5);
}

void BrBandItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // Caption strip
    static const QColor shades[] = {QColor(0xdc, 0xe6, 0xf2), QColor(0xd4, 0xec, 0xdc),
                                    QColor(0xf2, 0xe6, 0xd0), QColor(0xe8, 0xdc, 0xf0)};
    const QRectF strip(0, -CaptionHeight, m_width, CaptionHeight);
    QColor c = shades[m_depth % 4];
    if (isSelected())
        c = kSelection.lighter(160);
    painter->fillRect(strip, c);
    BrPageItem text = BrPageItem::makeText(strip.adjusted(1 + m_depth * 2.0, 0, -1, 0), m_caption,
                                           QFont(QStringLiteral("Arial"), 7), QColor(0x33, 0x3d, 0x4a),
                                           Qt::AlignLeft | Qt::AlignVCenter);
    paintItems(painter, {text});

    // Band area
    painter->fillRect(bandRect(), Qt::white);
    paintItems(painter, m_items);
    QPen pen(isSelected() ? kSelection : QColor(0x9a, 0xa9, 0xbb), isSelected() ? 1.5 : 0, Qt::DashLine);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->drawLine(QPointF(0, m_height), QPointF(m_width, m_height));
    if (isSelected()) {
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(bandRect());
    }
}

bool BrBandItem::onBottomEdge(const QPointF &pos) const
{
    const BrDesignScene *s = designScene();
    const double tol = 5.0 / (s ? std::max(0.5, s->viewScale()) : 4.0);
    return std::abs(pos.y() - m_height) <= std::min(tol, 1.5) && pos.x() >= 0 && pos.x() <= m_width;
}

void BrBandItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(onBottomEdge(event->pos()) ? Qt::SizeVerCursor : Qt::ArrowCursor);
    QGraphicsItem::hoverMoveEvent(event);
}

void BrBandItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && onBottomEdge(event->pos())) {
        m_resizing = true;
        event->accept();
        return;
    }
    QGraphicsItem::mousePressEvent(event);
}

void BrBandItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_resizing) {
        QGraphicsItem::mouseMoveEvent(event);
        return;
    }
    BrDesignScene *s = designScene();
    const double h = std::max(1.0, s ? s->snap(event->pos().y()) : event->pos().y());
    prepareGeometryChange();
    m_height = h;
    m_items = m_band ? BrDesignContext::renderControl(m_band, QSizeF(m_width, m_height)) : QList<BrPageItem>();
    update();
}

void BrBandItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_resizing) {
        QGraphicsItem::mouseReleaseEvent(event);
        return;
    }
    m_resizing = false;
    if (m_band && !qFuzzyCompare(m_band->height(), m_height)) {
        m_band->setHeight(m_height);
        if (BrDesignScene *s = designScene())
            s->notifyLayoutEdited();
    }
}

// ---------------------------------------------------------------------------
// BrGuideItem
// ---------------------------------------------------------------------------

BrGuideItem::BrGuideItem(Qt::Orientation orientation, double position, const QRectF &area)
    : m_orientation(orientation), m_area(area)
{
    setZValue(20000);
    setAcceptHoverEvents(true);
    setCursor(orientation == Qt::Vertical ? Qt::SplitHCursor : Qt::SplitVCursor);
    setPosition(position);
}

BrDesignScene *BrGuideItem::designScene() const
{
    return qobject_cast<BrDesignScene *>(scene());
}

double BrGuideItem::position() const
{
    return m_orientation == Qt::Vertical ? pos().x() : pos().y();
}

void BrGuideItem::setPosition(double position)
{
    if (m_orientation == Qt::Vertical)
        setPos(position, 0);
    else
        setPos(0, position);
}

QRectF BrGuideItem::boundingRect() const
{
    const BrDesignScene *s = designScene();
    const double grip = 3.0 / (s ? std::max(0.5, s->viewScale()) : 4.0);
    return m_orientation == Qt::Vertical
               ? QRectF(-grip, m_area.top(), 2 * grip, m_area.height())
               : QRectF(m_area.left(), -grip, m_area.width(), 2 * grip);
}

void BrGuideItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QPen pen(kGuide, 0);
    pen.setCosmetic(true);
    painter->setPen(pen);
    if (m_orientation == Qt::Vertical)
        painter->drawLine(QPointF(0, m_area.top()), QPointF(0, m_area.bottom()));
    else
        painter->drawLine(QPointF(m_area.left(), 0), QPointF(m_area.right(), 0));
}

void BrGuideItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        event->accept();
    else
        event->ignore();
}

void BrGuideItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    const double value = m_orientation == Qt::Vertical ? event->scenePos().x() : event->scenePos().y();
    const BrDesignScene *s = designScene();
    setPosition(s ? s->snap(value) : value);
}

void BrGuideItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (BrDesignScene *s = designScene())
        s->finishGuide(this, !s->isOutOfView(event->scenePos()));
}

// ---------------------------------------------------------------------------
// BrDesignScene
// ---------------------------------------------------------------------------

BrDesignScene::BrDesignScene(QObject *parent) : QGraphicsScene(parent)
{
    setBackgroundBrush(QColor(0x80, 0x88, 0x94));
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        if (!m_rebuilding)
            emit selectedObjectsChanged();
    });
}

void BrDesignScene::setReport(BrReport *report)
{
    m_report = report;
    rebuild();
}

double BrDesignScene::viewScale() const
{
    return views().isEmpty() ? 4.0 : views().first()->transform().m11();
}

double BrDesignScene::snap(double v) const
{
    return m_snap && m_grid > 0 ? std::round(v / m_grid) * m_grid : v;
}

QPointF BrDesignScene::snap(const QPointF &p) const
{
    return QPointF(snap(p.x()), snap(p.y()));
}

void BrDesignScene::notifyRelayout()
{
    emit layoutEdited();
}

QList<QObject *> BrDesignScene::selectedObjects() const
{
    QList<QObject *> list;
    for (QGraphicsItem *item : selectedItems()) {
        if (item->type() == BrControlItem::Type) {
            if (BrControl *c = static_cast<BrControlItem *>(item)->control())
                list << c;
        } else if (item->type() == BrBandItem::Type) {
            if (BrBand *b = static_cast<BrBandItem *>(item)->band())
                list << b;
        }
    }
    return list;
}

void BrDesignScene::selectObjects(const QList<QObject *> &objects)
{
    const bool wasRebuilding = m_rebuilding;
    m_rebuilding = true;
    clearSelection();
    for (QGraphicsItem *item : items()) {
        QObject *o = nullptr;
        if (item->type() == BrControlItem::Type)
            o = static_cast<BrControlItem *>(item)->control();
        else if (item->type() == BrBandItem::Type)
            o = static_cast<BrBandItem *>(item)->band();
        if (o && objects.contains(o))
            item->setSelected(true);
    }
    m_rebuilding = wasRebuilding;
    if (!wasRebuilding)
        emit selectedObjectsChanged();
}

void BrDesignScene::rebuild()
{
    QList<QPointer<QObject>> selected;
    for (QObject *o : selectedObjects())
        selected << o;

    m_rebuilding = true;
    m_moving.clear();
    m_snapExcluded.clear();
    m_snapBand = nullptr;
    m_snapLines.clear();
    clear();
    if (!m_report) {
        m_rebuilding = false;
        return;
    }
    const QSizeF page = m_report->pageSizeMm();
    const QMarginsF m = m_report->margins();
    double y = m.top();
    addPager(m_report, 0, QString(), y);
    const double pageHeight = std::max(page.height(), y + m.bottom());

    auto *shadow = addRect(QRectF(1.2, 1.2, page.width(), pageHeight), Qt::NoPen, QColor(0, 0, 0, 70));
    shadow->setZValue(-20);
    auto *paper = addRect(QRectF(0, 0, page.width(), pageHeight), Qt::NoPen, Qt::white);
    paper->setZValue(-10);
    QPen marginPen(QColor(0xc8, 0xd0, 0xda), 0, Qt::DashLine);
    marginPen.setCosmetic(true);
    auto *margins = addRect(QRectF(m.left(), m.top(), page.width() - m.left() - m.right(),
                                   pageHeight - m.top() - m.bottom()),
                            marginPen);
    margins->setZValue(-5);
    if (pageHeight > page.height() + 0.01 && !m_report->isContinuous()) {
        // Show where the first page ends when the layout is taller than the paper.
        QPen endPen(QColor(0xd0, 0x40, 0x40), 0, Qt::DashDotLine);
        endPen.setCosmetic(true);
        auto *end = addLine(0, page.height(), page.width(), page.height(), endPen);
        end->setZValue(-4);
    }
    setSceneRect(QRectF(-15, -15, page.width() + 30, pageHeight + 30));
    for (const QVariant &x : m_report->verticalGuides())
        addGuide(Qt::Vertical, x.toDouble());
    for (const QVariant &y : m_report->horizontalGuides())
        addGuide(Qt::Horizontal, y.toDouble());

    QList<QObject *> keep;
    for (const QPointer<QObject> &o : selected)
        if (o)
            keep << o;
    selectObjects(keep);
    m_rebuilding = false;
}

QList<double> BrDesignScene::snapTargets(Qt::Orientation axis) const
{
    const bool horizontal = axis == Qt::Horizontal;
    auto excluded = [this](QGraphicsItem *item) {
        for (; item; item = item->parentItem())
            if (m_snapExcluded.contains(item))
                return true;
        return false;
    };
    QList<double> targets;
    const QList<QGraphicsItem *> all = items();
    for (QGraphicsItem *item : all) {
        if (item->type() == BrGuideItem::Type) {
            const auto *guide = static_cast<BrGuideItem *>(item);
            if (guide->isVisible() && (guide->orientation() == Qt::Vertical) == horizontal)
                targets << guide->position();
            continue;
        }
        // Columns line up across bands, but each band has its own vertical positions.
        if (!horizontal && item->topLevelItem() != m_snapBand)
            continue;
        if (item->type() == BrControlItem::Type && !excluded(item)) {
            const auto *control = static_cast<BrControlItem *>(item);
            const QRectF r = control->mapRectToScene(QRectF(QPointF(0, 0), control->size()));
            targets << (horizontal ? QList<double>{r.left(), r.center().x(), r.right()}
                                   : QList<double>{r.top(), r.center().y(), r.bottom()});
        } else if (item->type() == BrBandItem::Type) {
            const auto *bandItem = static_cast<BrBandItem *>(item);
            const QRectF r = bandItem->mapRectToScene(bandItem->bandRect());
            targets << (horizontal ? QList<double>{r.left(), r.right()}
                                   : QList<double>{r.top(), r.bottom()});
        }
    }
    return targets;
}

bool BrDesignScene::snapToTarget(Qt::Orientation axis, const QList<double> &edges, double *offset)
{
    if (!m_snapObjects)
        return false;
    double best = 5.0 / std::max(0.5, viewScale()); // 5 pixels at any zoom
    bool found = false;
    double line = 0;
    const QList<double> targets = snapTargets(axis);
    for (double edge : edges) {
        for (double target : targets) {
            if (std::abs(target - edge) < best) {
                best = std::abs(target - edge);
                *offset = target - edge;
                line = target;
                found = true;
            }
        }
    }
    if (found) {
        const QRectF r = sceneRect();
        m_snapLines << (axis == Qt::Horizontal ? QLineF(line, r.top(), line, r.bottom())
                                               : QLineF(r.left(), line, r.right(), line));
    }
    return found;
}

void BrDesignScene::beginMove(BrControlItem *grabber)
{
    QList<QGraphicsItem *> selected = selectedItems();
    if (!selected.contains(grabber))
        selected.prepend(grabber);
    else
        selected.move(selected.indexOf(grabber), 0);

    m_moving.clear();
    m_snapExcluded.clear();
    m_movingBounds = QRectF();
    m_snapBand = grabber->topLevelItem();
    for (QGraphicsItem *item : selected) {
        if (item->type() != BrControlItem::Type)
            continue;
        auto *control = static_cast<BrControlItem *>(item);
        bool insideSelected = false; // moves with its parent
        for (QGraphicsItem *p = item->parentItem(); p; p = p->parentItem())
            insideSelected = insideSelected || p->isSelected();
        if (!control->isMovable() || insideSelected)
            continue;
        m_moving << MovingItem{control, control->pos()};
        m_snapExcluded.insert(control);
        m_movingBounds |= control->mapRectToScene(QRectF(QPointF(0, 0), control->size()));
    }
}

void BrDesignScene::moveSelection(const QPointF &delta)
{
    if (m_moving.isEmpty())
        return;
    const QPointF proposed = m_moving.first().start + delta;
    QPointF offset = snap(proposed) - proposed;
    const QRectF r = m_movingBounds.translated(delta);
    double x = 0, y = 0;
    m_snapLines.clear();
    if (snapToTarget(Qt::Horizontal, {r.left(), r.center().x(), r.right()}, &x))
        offset.setX(x);
    if (snapToTarget(Qt::Vertical, {r.top(), r.center().y(), r.bottom()}, &y))
        offset.setY(y);
    for (const MovingItem &m : std::as_const(m_moving))
        m.item->setPos(m.start + delta + offset);
    update();
}

void BrDesignScene::beginResize(BrControlItem *item)
{
    m_snapExcluded = {item};
    m_snapBand = item->topLevelItem();
}

double BrDesignScene::snapEdge(BrControlItem *item, Qt::Orientation axis, double value)
{
    const QPointF origin = item->parentItem() ? item->parentItem()->scenePos() : QPointF();
    const double position = value + (axis == Qt::Horizontal ? origin.x() : origin.y());
    double offset = 0;
    if (snapToTarget(axis, {position}, &offset))
        return value + offset;
    return snap(value);
}

void BrDesignScene::clearSnapLines()
{
    if (!m_snapLines.isEmpty()) {
        m_snapLines.clear();
        update();
    }
}

void BrDesignScene::endInteraction()
{
    m_moving.clear();
    m_snapExcluded.clear();
    m_snapBand = nullptr;
    clearSnapLines();
}

void BrDesignScene::drawForeground(QPainter *painter, const QRectF &)
{
    if (m_snapLines.isEmpty())
        return;
    QPen pen(kSnapLine, 1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->drawLines(m_snapLines.toVector());
}

BrGuideItem *BrDesignScene::addGuide(Qt::Orientation orientation, double position)
{
    auto *guide = new BrGuideItem(orientation, position, sceneRect());
    addItem(guide);
    return guide;
}

void BrDesignScene::storeGuides()
{
    if (!m_report)
        return;
    QList<double> vertical, horizontal;
    const QList<QGraphicsItem *> all = items();
    for (QGraphicsItem *item : all) {
        if (item->type() != BrGuideItem::Type || !item->isVisible())
            continue;
        const auto *guide = static_cast<BrGuideItem *>(item);
        (guide->orientation() == Qt::Vertical ? vertical : horizontal) << guide->position();
    }
    std::sort(vertical.begin(), vertical.end());
    std::sort(horizontal.begin(), horizontal.end());
    QVariantList verticalValues, horizontalValues;
    for (double v : std::as_const(vertical))
        verticalValues << v;
    for (double v : std::as_const(horizontal))
        horizontalValues << v;
    m_report->setVerticalGuides(verticalValues);
    m_report->setHorizontalGuides(horizontalValues);
}

void BrDesignScene::finishGuide(BrGuideItem *guide, bool keep)
{
    if (!keep)
        guide->hide(); // the rebuild after layoutEdited() deletes it
    storeGuides();
    emit layoutEdited();
}

void BrDesignScene::clearGuides()
{
    const QList<QGraphicsItem *> all = items();
    for (QGraphicsItem *item : all)
        if (item->type() == BrGuideItem::Type)
            item->hide();
    storeGuides();
    emit layoutEdited();
}

bool BrDesignScene::isOutOfView(const QPointF &scenePos) const
{
    if (views().isEmpty())
        return false;
    const QGraphicsView *view = views().first();
    return !view->viewport()->rect().contains(view->mapFromScene(scenePos));
}

void BrDesignScene::addBand(BrBand *band, int depth, const QString &label, double &y)
{
    const QRectF content = m_report->contentRect();
    y += BrBandItem::CaptionHeight;
    QString caption = bandTypeName(band->bandType());
    if (!label.isEmpty())
        caption = label + QStringLiteral(" – ") + caption;
    if (!band->objectName().isEmpty())
        caption += QStringLiteral(" (") + band->objectName() + QLatin1Char(')');
    if (band->columns() > 1)
        caption += QObject::tr(" – %1 columns").arg(band->columns());
    auto *item = new BrBandItem(band, caption, content.width(), depth);
    item->setPos(content.left(), y);
    // Upper bands above lower ones: the bottom edge of a band overlaps the next caption.
    item->setZValue(10000 - y);
    addItem(item);
    addControls(band, QSizeF(content.width(), band->height()), item);
    y += band->height();
}

void BrDesignScene::addControls(BrContainer *container, const QSizeF &size, QGraphicsItem *parentItem)
{
    for (const auto &placed : BrDesignContext::layout(container, size)) {
        BrControl *c = placed.first;
        const QRectF rect = placed.second;
        auto *item = new BrControlItem(c, rect, c->align() == Br::Align::None, parentItem);
        if (auto *child = qobject_cast<BrContainer *>(c))
            addControls(child, rect.size(), item);
    }
}

void BrDesignScene::addPager(QObject *pagerObject, int depth, const QString &label, double &y)
{
    auto *pager = qobject_cast<BrPager *>(pagerObject);
    if (!pager)
        return;
    auto addType = [&](Br::BandType t) {
        for (BrBand *b : pager->bands(t))
            addBand(b, depth, label, y);
    };
    addType(Br::BandType::Header);
    addType(Br::BandType::Title);
    addType(Br::BandType::ColumnHeader);
    for (QObject *child : pager->children()) {
        if (auto *b = qobject_cast<BrBand *>(child)) {
            if (b->bandType() == Br::BandType::Detail)
                addBand(b, depth, label, y);
        } else if (auto *g = qobject_cast<BrGroup *>(child)) {
            QString key = g->dataFields().isEmpty() ? g->dataFormula() : g->dataFields();
            QString name = g->objectName().isEmpty() ? key : g->objectName();
            addPager(g, depth + 1, QObject::tr("Group: %1").arg(name), y);
        } else if (auto *s = qobject_cast<BrSubDetail *>(child)) {
            QString name = s->objectName().isEmpty() ? s->dataSourceName() : s->objectName();
            addPager(s, depth + 1, QObject::tr("Sub-report: %1").arg(name), y);
        }
    }
    addType(Br::BandType::ColumnFooter);
    addType(Br::BandType::Summary);
    addType(Br::BandType::Footer);
}

bool BrDesignScene::containerAt(const QPointF &scenePos, BrContainer **container, QPointF *local) const
{
    for (QGraphicsItem *item : items(scenePos)) {
        if (item->type() == BrControlItem::Type) {
            auto *ci = static_cast<BrControlItem *>(item);
            if (auto *c = qobject_cast<BrContainer *>(ci->control())) {
                *container = c;
                *local = ci->mapFromScene(scenePos);
                return true;
            }
        } else if (item->type() == BrBandItem::Type) {
            auto *bi = static_cast<BrBandItem *>(item);
            const QPointF p = bi->mapFromScene(scenePos);
            if (bi->bandRect().contains(p) && bi->band()) {
                *container = bi->band();
                *local = p;
                return true;
            }
        }
    }
    return false;
}

void BrDesignScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_insertTemplate.isEmpty() && event->button() == Qt::LeftButton) {
        BrContainer *container = nullptr;
        QPointF local;
        if (containerAt(event->scenePos(), &container, &local)) {
            const QJsonObject json = m_insertTemplate;
            m_insertTemplate = QJsonObject();
            emit insertRequested(container, snap(local), json);
        }
        event->accept();
        return;
    }
    QGraphicsScene::mousePressEvent(event);
}

void BrDesignScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    endInteraction();
    bool changed = false;
    for (QGraphicsItem *item : selectedItems()) {
        if (item->type() != BrControlItem::Type)
            continue;
        auto *ci = static_cast<BrControlItem *>(item);
        BrControl *c = ci->control();
        if (!c || !ci->isMovable())
            continue;
        const QRectF r(ci->pos(), ci->size());
        if (!qFuzzyCompare(r.x() + 1, c->x() + 1) || !qFuzzyCompare(r.y() + 1, c->y() + 1)) {
            c->setGeometry(QRectF(r.topLeft(), c->geometry().size()));
            changed = true;
        }
    }
    if (changed)
        emit layoutEdited();
}

void BrDesignScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    event->setAccepted(event->mimeData()->hasFormat(QStringLiteral(BRREPORT_OBJECT_MIME)));
}

void BrDesignScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    BrContainer *container = nullptr;
    QPointF local;
    event->setAccepted(event->mimeData()->hasFormat(QStringLiteral(BRREPORT_OBJECT_MIME))
                       && containerAt(event->scenePos(), &container, &local));
}

void BrDesignScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    BrContainer *container = nullptr;
    QPointF local;
    if (!containerAt(event->scenePos(), &container, &local))
        return;
    const QJsonObject json =
        QJsonDocument::fromJson(event->mimeData()->data(QStringLiteral(BRREPORT_OBJECT_MIME))).object();
    if (json.isEmpty())
        return;
    event->acceptProposedAction();
    emit insertRequested(container, snap(local), json);
}
