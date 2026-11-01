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

#include "brgraphics.h"

#include "brrendercontext.h"

#include <QPixmap>

// ---------------------------------------------------------------------------
// BrImage
// ---------------------------------------------------------------------------

BrImage::BrImage(BrContainer *parent) : BrControl(parent) {}

bool BrImage::load(const QString &fileName)
{
    return m_image.load(fileName);
}

QImage BrImage::currentImage(BrRenderContext &)
{
    return m_image;
}

void BrImage::render(BrRenderContext &ctx, const QRectF &rect, BrPage &page)
{
    renderFrame(rect, page);
    const QImage img = currentImage(ctx);
    if (img.isNull() || rect.isEmpty())
        return;

    // "Natural" size of the image: use its own resolution, or 96 dpi if unknown.
    const double dpiX = img.dotsPerMeterX() > 0 ? img.dotsPerMeterX() * 0.0254 : 96.0;
    const double dpiY = img.dotsPerMeterY() > 0 ? img.dotsPerMeterY() * 0.0254 : 96.0;
    const QSizeF natural(img.width() * 25.4 / dpiX, img.height() * 25.4 / dpiY);

    QRectF target = rect;
    switch (m_scaling) {
    case Br::ImageScale::Stretch:
        break;
    case Br::ImageScale::KeepAspect: {
        QSizeF s = natural;
        s.scale(rect.size(), Qt::KeepAspectRatio);
        target = QRectF(QPointF(), s);
        target.moveCenter(rect.center());
        break;
    }
    case Br::ImageScale::None:
        target = QRectF(rect.topLeft(), natural);
        break;
    case Br::ImageScale::Center:
        target = QRectF(QPointF(), natural);
        target.moveCenter(rect.center());
        break;
    }

    // Clip images larger than the control by cropping the source.
    const QRectF visible = target.intersected(rect);
    if (visible.isEmpty())
        return;
    if (visible != target) {
        const double sx = img.width() / target.width();
        const double sy = img.height() / target.height();
        const QRect src(int((visible.left() - target.left()) * sx),
                        int((visible.top() - target.top()) * sy), int(visible.width() * sx),
                        int(visible.height() * sy));
        page.items << BrPageItem::makeImage(visible, img.copy(src));
    } else {
        page.items << BrPageItem::makeImage(target, img);
    }
}

// ---------------------------------------------------------------------------
// BrDBImage
// ---------------------------------------------------------------------------

BrDBImage::BrDBImage(BrContainer *parent) : BrImage(parent) {}

BrDBImage::BrDBImage(const QString &dataField, BrContainer *parent)
    : BrImage(parent), m_dataField(dataField)
{
}

QImage BrDBImage::currentImage(BrRenderContext &ctx)
{
    if (ctx.isDesignMode()) {
        QImage img(64, 48, QImage::Format_ARGB32);
        img.fill(QColor(0xe8, 0xec, 0xf2));
        return img;
    }
    const QVariant v = ctx.fieldValue(m_dataField);
    switch (v.userType()) {
    case QMetaType::QImage:
        return v.value<QImage>();
    case QMetaType::QPixmap:
        return v.value<QPixmap>().toImage();
    case QMetaType::QByteArray:
        return QImage::fromData(v.toByteArray());
    case QMetaType::QString: {
        QImage img;
        img.load(v.toString());
        return img;
    }
    default:
        return {};
    }
}

// ---------------------------------------------------------------------------
// BrDraw
// ---------------------------------------------------------------------------

BrDraw::BrDraw(BrContainer *parent) : BrControl(parent) {}

BrDraw::BrDraw(Br::DrawKind kind, BrContainer *parent) : BrControl(parent), m_kind(kind) {}

void BrDraw::render(BrRenderContext &, const QRectF &rect, BrPage &page)
{
    renderFrame(rect, page);
    // Keep the stroke inside the control rectangle.
    const double h = m_penWidth / 2.0;
    const QRectF r = rect.adjusted(h, h, -h, -h);
    BrPageItem item;
    item.penColor = m_penColor;
    item.penWidth = m_penWidth;
    item.penStyle = m_penColor.isValid() ? m_penStyle : Qt::NoPen;
    item.brushColor = m_fillColor;
    item.rect = r;
    switch (m_kind) {
    case Br::DrawKind::Rectangle:
        item.kind = BrPageItem::Rect;
        break;
    case Br::DrawKind::RoundedRectangle:
        item.kind = BrPageItem::Rect;
        item.radius = m_radius;
        break;
    case Br::DrawKind::Ellipse:
        item.kind = BrPageItem::Ellipse;
        break;
    case Br::DrawKind::HorizontalLine:
        item.kind = BrPageItem::Line;
        item.points = QPolygonF({QPointF(rect.left(), rect.center().y()),
                                 QPointF(rect.right(), rect.center().y())});
        break;
    case Br::DrawKind::VerticalLine:
        item.kind = BrPageItem::Line;
        item.points = QPolygonF({QPointF(rect.center().x(), rect.top()),
                                 QPointF(rect.center().x(), rect.bottom())});
        break;
    case Br::DrawKind::DiagonalDown:
        item.kind = BrPageItem::Line;
        item.points = QPolygonF({r.topLeft(), r.bottomRight()});
        break;
    case Br::DrawKind::DiagonalUp:
        item.kind = BrPageItem::Line;
        item.points = QPolygonF({r.bottomLeft(), r.topRight()});
        break;
    case Br::DrawKind::Triangle:
        item.kind = BrPageItem::Polygon;
        item.points = QPolygonF({QPointF(r.center().x(), r.top()), r.bottomRight(), r.bottomLeft()});
        break;
    }
    page.items << item;
}
