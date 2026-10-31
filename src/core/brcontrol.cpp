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

#include "brcontrol.h"

#include "brband.h"
#include "brrendercontext.h"
#include "brreport.h"

#include <algorithm>

BrBorders BrBorders::all(double width, const QColor &color)
{
    BrBorders b;
    b.left = b.top = b.right = b.bottom = true;
    b.width = width;
    b.color = color;
    return b;
}

BrBorders BrBorders::bottomOnly(double width, const QColor &color)
{
    BrBorders b;
    b.bottom = true;
    b.width = width;
    b.color = color;
    return b;
}

// ---------------------------------------------------------------------------
// BrControl
// ---------------------------------------------------------------------------

BrControl::BrControl(BrContainer *parent) : QObject(parent) {}

BrControl::BrControl(QObject *parent, bool) : QObject(parent) {}

BrControl::~BrControl() = default;

QFont BrControl::font() const
{
    if (m_hasFont)
        return m_font;
    for (QObject *p = parent(); p; p = p->parent()) {
        if (auto *c = qobject_cast<BrControl *>(p))
            return c->font();
        if (auto *r = qobject_cast<BrReport *>(p))
            return r->font();
    }
    return QFont(QStringLiteral("Arial"), 10);
}

void BrControl::setFont(const QFont &font)
{
    m_font = font;
    m_hasFont = true;
}

BrContainer *BrControl::parentContainer() const
{
    return qobject_cast<BrContainer *>(parent());
}

BrBand *BrControl::band() const
{
    for (QObject *p = parent(); p; p = p->parent())
        if (auto *b = qobject_cast<BrBand *>(p))
            return b;
    return nullptr;
}

BrReport *BrControl::report() const
{
    for (QObject *p = parent(); p; p = p->parent())
        if (auto *r = qobject_cast<BrReport *>(p))
            return r;
    return nullptr;
}

bool BrControl::checkPrint(BrRenderContext &ctx)
{
    bool printIt = m_visible;
    if (printIt && m_printIf.isValid())
        printIt = ctx.evaluate(m_printIf).toBool();
    if (printIt && m_beforePrint)
        m_beforePrint(*this, printIt);
    m_willPrint = printIt;
    return printIt;
}

void BrControl::prepare(BrRenderContext &) {}

double BrControl::measureHeight(BrRenderContext &, double)
{
    return height();
}

void BrControl::render(BrRenderContext &, const QRectF &rect, BrPage &page)
{
    renderFrame(rect, page);
}

void BrControl::renderFrame(const QRectF &rect, BrPage &page) const
{
    if (m_background.isValid() && m_background.alpha() > 0)
        page.items << BrPageItem::makeRect(rect, QColor(), 0, m_background);
    if (!m_borders.any())
        return;
    // Draw each side as a line so that partial borders look the same everywhere.
    const QColor c = m_borders.color;
    const double w = m_borders.width;
    const Qt::PenStyle s = m_borders.style;
    if (m_borders.left && m_borders.top && m_borders.right && m_borders.bottom) {
        page.items << BrPageItem::makeRect(rect, c, w, QColor(), s);
        return;
    }
    if (m_borders.top)
        page.items << BrPageItem::makeLine(rect.topLeft(), rect.topRight(), c, w, s);
    if (m_borders.bottom)
        page.items << BrPageItem::makeLine(rect.bottomLeft(), rect.bottomRight(), c, w, s);
    if (m_borders.left)
        page.items << BrPageItem::makeLine(rect.topLeft(), rect.bottomLeft(), c, w, s);
    if (m_borders.right)
        page.items << BrPageItem::makeLine(rect.topRight(), rect.bottomRight(), c, w, s);
}

// ---------------------------------------------------------------------------
// BrContainer
// ---------------------------------------------------------------------------

BrContainer::BrContainer(BrContainer *parent) : BrControl(parent) {}

BrContainer::BrContainer(QObject *parent, bool) : BrControl(parent, true) {}

QList<BrControl *> BrContainer::controls() const
{
    QList<BrControl *> list;
    for (QObject *o : children())
        if (auto *c = qobject_cast<BrControl *>(o))
            list << c;
    return list;
}

bool BrContainer::checkPrint(BrRenderContext &ctx)
{
    if (!BrControl::checkPrint(ctx))
        return false;
    for (BrControl *c : controls())
        c->checkPrint(ctx);
    return true;
}

void BrContainer::prepare(BrRenderContext &ctx)
{
    for (BrControl *c : controls())
        if (c->willPrint())
            c->prepare(ctx);
}

double BrContainer::layoutChildren(BrRenderContext &ctx, double width, double height,
                                   bool expand, QList<Placement> *placements)
{
    QList<Placement> placed;
    QList<BrControl *> kids;
    for (BrControl *c : controls())
        if (c->willPrint())
            kids << c;

    auto heightOf = [&](BrControl *c, double w) {
        return std::max(c->height(), c->measureHeight(ctx, w));
    };

    double top = 0.0;
    double maxBottom = 0.0;
    QList<QPair<BrControl *, double>> bottoms;

    // Free (non-aligned) controls, top to bottom, so that growth can push the ones below.
    struct Free
    {
        BrControl *control;
        QRectF rect;
        double growth;
    };
    QList<Free> free;
    QList<BrControl *> freeOrder;
    for (BrControl *c : kids)
        if (c->align() == Br::Align::None)
            freeOrder << c;
    std::stable_sort(freeOrder.begin(), freeOrder.end(),
                     [](BrControl *a, BrControl *b) { return a->y() < b->y(); });
    for (BrControl *c : freeOrder) {
        const double h = heightOf(c, c->width());
        double shift = 0.0;
        if (shiftOnStretch()) {
            for (const Free &f : free) {
                const BrControl *o = f.control;
                const bool below = c->y() >= o->y() + o->height() - 0.001;
                const bool overlaps = c->x() < o->x() + o->width() && o->x() < c->x() + c->width();
                const double pushed = f.rect.bottom() - (o->y() + o->height());
                if (below && overlaps && pushed > 0.0)
                    shift = std::max(shift, pushed);
            }
        }
        const QRectF r(c->x(), c->y() + shift, c->width(), h);
        free << Free{c, r, h - c->height()};
        maxBottom = std::max(maxBottom, r.bottom());
    }

    for (BrControl *c : kids) {
        if (c->align() == Br::Align::Top) {
            const double h = heightOf(c, width);
            placed << Placement{c, QRectF(0, top, width, h)};
            top += h;
            maxBottom = std::max(maxBottom, top);
        } else if (c->align() == Br::Align::Bottom) {
            bottoms << qMakePair(c, heightOf(c, width));
        }
    }

    double bottomSum = 0.0;
    for (const auto &b : bottoms)
        bottomSum += b.second;

    double finalHeight = height;
    if (expand)
        finalHeight = std::max(height, std::max(maxBottom, top + bottomSum));

    for (Free &f : free) {
        if (f.control->stretchToBand())
            f.rect.setBottom(std::max(f.rect.bottom(), finalHeight));
        placed << Placement{f.control, f.rect};
    }

    double b = finalHeight;
    for (int i = int(bottoms.size()) - 1; i >= 0; --i) {
        b -= bottoms.at(i).second;
        placed << Placement{bottoms.at(i).first, QRectF(0, b, width, bottoms.at(i).second)};
    }

    double left = 0.0, right = width;
    const double fillHeight = std::max(0.0, b - top);
    for (BrControl *c : kids) {
        if (c->align() == Br::Align::Left) {
            placed << Placement{c, QRectF(left, top, c->width(), fillHeight)};
            left += c->width();
        } else if (c->align() == Br::Align::Right) {
            right -= c->width();
            placed << Placement{c, QRectF(right, top, c->width(), fillHeight)};
        }
    }
    for (BrControl *c : kids)
        if (c->align() == Br::Align::Client)
            placed << Placement{c, QRectF(left, top, std::max(0.0, right - left), fillHeight)};

    // Keep the creation order for painting (z-order).
    std::stable_sort(placed.begin(), placed.end(), [&](const Placement &a, const Placement &b2) {
        return kids.indexOf(a.control) < kids.indexOf(b2.control);
    });
    if (placements)
        *placements = placed;
    return finalHeight;
}

double BrContainer::measureHeight(BrRenderContext &ctx, double width)
{
    return layoutChildren(ctx, width, height(), autoExpand(), nullptr);
}

void BrContainer::render(BrRenderContext &ctx, const QRectF &rect, BrPage &page)
{
    renderFrame(rect, page);
    QList<Placement> placements;
    layoutChildren(ctx, rect.width(), rect.height(), false, &placements);
    for (const Placement &p : placements)
        p.control->render(ctx, p.rect.translated(rect.topLeft()), page);
}

// ---------------------------------------------------------------------------

BrPanel::BrPanel(BrContainer *parent) : BrContainer(parent) {}
