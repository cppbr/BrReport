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

#include "brrenderer.h"

#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

QRectF toUnits(const QRectF &mm)
{
    const double k = BrRenderer::UnitsPerMm;
    return QRectF(mm.x() * k, mm.y() * k, mm.width() * k, mm.height() * k);
}

QPen makePen(const BrPageItem &item)
{
    if (item.penStyle == Qt::NoPen || !item.penColor.isValid())
        return QPen(Qt::NoPen);
    QPen pen(item.penColor, item.penWidth * BrRenderer::UnitsPerMm, item.penStyle);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    if (item.penWidth <= 0.0)
        pen.setCosmetic(true); // hairline
    return pen;
}

QBrush makeBrush(const BrPageItem &item)
{
    return item.brushColor.isValid() ? QBrush(item.brushColor) : QBrush(Qt::NoBrush);
}

} // namespace

void BrRenderer::renderItem(QPainter *p, const BrPageItem &item, double deviceDpi)
{
    switch (item.kind) {
    case BrPageItem::Text: {
        if (item.text.isEmpty())
            return;
        QFont font = item.font;
        // Font sizes are in points; convert them to painter units for this device.
        font.setPointSizeF(qMax(0.01, item.font.pointSizeF()) * 25.4 * UnitsPerMm / deviceDpi);
        font.setHintingPreference(QFont::PreferNoHinting);
        p->setFont(font);
        p->setPen(item.textColor);
        p->setBrush(Qt::NoBrush);
        const int flags = item.alignment | Qt::TextSingleLine | Qt::TextDontClip;
        QRectF r = toUnits(item.rect);
        if (qFuzzyIsNull(item.angle)) {
            p->drawText(r, flags, item.text);
        } else {
            p->save();
            p->translate(r.center());
            p->rotate(-item.angle);
            const double a = std::fmod(std::abs(item.angle), 180.0);
            QSizeF sz = r.size();
            if (std::abs(a - 90.0) < 1.0)
                sz.transpose();
            p->drawText(QRectF(-sz.width() / 2, -sz.height() / 2, sz.width(), sz.height()), flags,
                        item.text);
            p->restore();
        }
        break;
    }
    case BrPageItem::Rect:
        p->setPen(makePen(item));
        p->setBrush(makeBrush(item));
        if (item.radius > 0.0)
            p->drawRoundedRect(toUnits(item.rect), item.radius * UnitsPerMm,
                               item.radius * UnitsPerMm);
        else
            p->drawRect(toUnits(item.rect));
        break;
    case BrPageItem::Line:
        if (item.points.size() >= 2) {
            p->setPen(makePen(item));
            p->drawLine(item.points.at(0) * UnitsPerMm, item.points.at(1) * UnitsPerMm);
        }
        break;
    case BrPageItem::Ellipse:
        p->setPen(makePen(item));
        p->setBrush(makeBrush(item));
        p->drawEllipse(toUnits(item.rect));
        break;
    case BrPageItem::Polygon: {
        QPolygonF poly;
        for (const QPointF &pt : item.points)
            poly << pt * UnitsPerMm;
        p->setPen(makePen(item));
        p->setBrush(makeBrush(item));
        p->drawPolygon(poly);
        break;
    }
    case BrPageItem::Image:
        if (!item.image.isNull())
            p->drawImage(toUnits(item.rect), item.image);
        break;
    }
}

void BrRenderer::renderPage(QPainter *painter, const BrPage &page, double pixelsPerMm,
                            const QPointF &offset)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->translate(offset);
    painter->scale(pixelsPerMm / UnitsPerMm, pixelsPerMm / UnitsPerMm);
    const double dpi = painter->device() ? painter->device()->logicalDpiY() : 96.0;
    for (const BrPageItem &item : page.items)
        renderItem(painter, item, dpi);
    painter->restore();
}

QImage BrRenderer::toImage(const BrPage &page, double dpi, const QColor &background)
{
    const double pxPerMm = dpi / 25.4;
    QImage image(qCeil(page.size.width() * pxPerMm), qCeil(page.size.height() * pxPerMm),
                 QImage::Format_ARGB32_Premultiplied);
    const int dpm = qRound(dpi / 0.0254);
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);
    image.fill(background);
    QPainter painter(&image);
    renderPage(&painter, page, pxPerMm);
    painter.end();
    return image;
}
