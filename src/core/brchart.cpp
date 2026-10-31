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

#include "brchart.h"

#include "brdatasource.h"
#include "brformat.h"
#include "brrendercontext.h"
#include "brreport.h"
#include "brtextmetrics.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

const QColor kGrid(0xd0, 0xd5, 0xdd);
const QColor kAxis(0x60, 0x6a, 0x78);

double niceStep(double range, int ticks)
{
    if (range <= 0)
        return 1.0;
    const double raw = range / ticks;
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (raw <= m * magnitude)
            return m * magnitude;
    return 10.0 * magnitude;
}

QFont scaled(const QFont &f, double factor)
{
    QFont r = f;
    r.setPointSizeF(std::max(4.0, (f.pointSizeF() > 0 ? f.pointSizeF() : 10.0) * factor));
    return r;
}

struct Scale
{
    double min = 0, max = 1, step = 1;
};

Scale computeScale(const QList<BrChart::Series> &series)
{
    double lo = 0.0, hi = 0.0;
    for (const auto &s : series)
        for (double v : s.values) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    if (qFuzzyCompare(lo + 1.0, hi + 1.0))
        hi = lo + 1.0;
    Scale sc;
    sc.step = niceStep(hi - lo, 5);
    sc.min = std::floor(lo / sc.step) * sc.step;
    sc.max = std::ceil(hi / sc.step) * sc.step;
    return sc;
}

} // namespace

BrChart::BrChart(BrContainer *parent) : BrControl(parent)
{
    setGeometry(0, 0, 120, 70);
    m_palette = {QColor(0x1f, 0x77, 0xb4), QColor(0xff, 0x7f, 0x0e), QColor(0x2c, 0xa0, 0x2c),
                 QColor(0xd6, 0x27, 0x28), QColor(0x94, 0x67, 0xbd), QColor(0x8c, 0x56, 0x4b),
                 QColor(0xe3, 0x77, 0xc2), QColor(0x7f, 0x7f, 0x7f), QColor(0xbc, 0xbd, 0x22),
                 QColor(0x17, 0xbe, 0xcf)};
}

BrChart::BrChart(Br::ChartType type, BrContainer *parent) : BrChart(parent)
{
    m_type = type;
}

void BrChart::addSeries(const QString &name, const QList<double> &values, const QColor &color)
{
    m_series << Series{name, values, color};
}

void BrChart::setDataSource(BrDataSource *dataSource, const QString &labelField,
                            const QStringList &valueFields)
{
    m_dataSource = dataSource;
    m_labelField = labelField;
    m_valueFields = valueFields;
}

QColor BrChart::colorAt(int index) const
{
    if (m_palette.isEmpty())
        return Qt::gray;
    return m_palette.at(index % m_palette.size());
}

void BrChart::prepare(BrRenderContext &ctx)
{
    m_curLabels = m_labels;
    m_curSeries = m_series;
    if (!m_dataSource)
        return;
    if (ctx.report() && m_dataSource == ctx.report()->dataSource()) {
        qWarning("BrChart: the chart cannot iterate the report's own data source");
        return;
    }
    m_curLabels.clear();
    m_curSeries.clear();
    for (const QString &f : m_valueFields)
        m_curSeries << Series{f, {}, QColor()};
    for (bool ok = m_dataSource->first(); ok && !m_dataSource->eof(); ok = m_dataSource->next()) {
        m_curLabels << BrFormat::format(m_dataSource->value(m_labelField), QString(), ctx.locale());
        for (int i = 0; i < m_valueFields.size(); ++i)
            m_curSeries[i].values << m_dataSource->value(m_valueFields.at(i)).toDouble();
    }
}

QRectF BrChart::renderLegend(const QRectF &area, const QStringList &names, BrPage &page)
{
    if (!m_showLegend || names.isEmpty())
        return area;
    const QFont f = scaled(font(), 0.9);
    const double lh = BrTextMetrics::lineHeight(f) * 1.25;
    const double swatch = std::min(3.0, lh * 0.7);
    double textWidth = 0.0;
    for (const QString &n : names)
        textWidth = std::max(textWidth, BrTextMetrics::width(f, n));
    const double width = std::min(area.width() * 0.4, swatch + 1.5 + textWidth + 1.0);
    const double height = names.size() * lh;
    double y = area.center().y() - height / 2;
    const double x = area.right() - width;
    for (int i = 0; i < names.size(); ++i, y += lh) {
        QColor c = colorAt(i);
        if (m_type != Br::ChartType::Pie && i < m_curSeries.size() && m_curSeries.at(i).color.isValid())
            c = m_curSeries.at(i).color;
        page.items << BrPageItem::makeRect(QRectF(x, y + (lh - swatch) / 2, swatch, swatch),
                                           QColor(), 0, c);
        page.items << BrPageItem::makeText(QRectF(x + swatch + 1.5, y, width - swatch - 1.5, lh),
                                           names.at(i), f, color(), Qt::AlignLeft | Qt::AlignVCenter);
    }
    return area.adjusted(0, 0, -(width + 3.0), 0);
}

void BrChart::render(BrRenderContext &ctx, const QRectF &rect, BrPage &page)
{
    renderFrame(rect, page);
    QRectF area = rect.adjusted(2, 2, -2, -2);
    if (!m_title.isEmpty()) {
        QFont tf = font();
        tf.setBold(true);
        tf = scaled(tf, 1.15);
        const double th = BrTextMetrics::lineHeight(tf);
        page.items << BrPageItem::makeText(QRectF(area.left(), area.top(), area.width(), th),
                                           m_title, tf, color(), Qt::AlignHCenter | Qt::AlignVCenter);
        area.setTop(area.top() + th + 1.5);
    }
    if (m_curSeries.isEmpty() || area.width() <= 5 || area.height() <= 5)
        return;

    switch (m_type) {
    case Br::ChartType::Pie:
        renderPie(ctx, renderLegend(area, m_curLabels, page), page);
        break;
    case Br::ChartType::Bar:
    case Br::ChartType::HorizontalBar:
    case Br::ChartType::Line: {
        QStringList names;
        for (const Series &s : m_curSeries)
            names << s.name;
        const QRectF plot = renderLegend(area, m_curSeries.size() > 1 ? names : QStringList(), page);
        if (m_type == Br::ChartType::Line)
            renderLines(ctx, plot, page);
        else
            renderBars(ctx, plot, page, m_type == Br::ChartType::HorizontalBar);
        break;
    }
    }
}

void BrChart::renderPie(BrRenderContext &ctx, const QRectF &area, BrPage &page)
{
    const QList<double> &values = m_curSeries.first().values;
    double total = 0.0;
    for (double v : values)
        if (v > 0)
            total += v;
    if (total <= 0.0)
        return;
    const double r = std::min(area.width(), area.height()) / 2.0 * 0.95;
    const QPointF c = area.center();
    const QFont f = scaled(font(), 0.85);
    double angle = 90.0; // start at 12 o'clock, clockwise
    for (int i = 0; i < values.size(); ++i) {
        const double v = values.at(i);
        if (v <= 0)
            continue;
        const double sweep = 360.0 * v / total;
        BrPageItem slice;
        slice.kind = BrPageItem::Polygon;
        slice.brushColor = colorAt(i);
        slice.penColor = Qt::white;
        slice.penWidth = 0.3;
        slice.points << c;
        const int steps = std::max(2, int(std::ceil(sweep / 2.0)));
        for (int k = 0; k <= steps; ++k) {
            const double a = qDegreesToRadians(angle - sweep * k / steps);
            slice.points << QPointF(c.x() + r * std::cos(a), c.y() - r * std::sin(a));
        }
        slice.rect = QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r);
        page.items << slice;

        if (m_showValues && sweep >= 14.0) {
            const double mid = qDegreesToRadians(angle - sweep / 2);
            const QPointF p(c.x() + r * 0.65 * std::cos(mid), c.y() - r * 0.65 * std::sin(mid));
            const QString text = BrFormat::formatNumber(100.0 * v / total, QStringLiteral("0.#"),
                                                        ctx.locale())
                                 + QLatin1Char('%');
            page.items << BrPageItem::makeText(QRectF(p.x() - 10, p.y() - 3, 20, 6), text, f,
                                               Qt::white, Qt::AlignCenter);
        }
        angle -= sweep;
    }
}

void BrChart::renderBars(BrRenderContext &ctx, const QRectF &area, BrPage &page, bool horizontal)
{
    const Scale sc = computeScale(m_curSeries);
    const QFont f = scaled(font(), 0.85);
    const double lh = BrTextMetrics::lineHeight(f);
    int categories = m_curLabels.size();
    for (const Series &s : m_curSeries)
        categories = std::max(categories, int(s.values.size()));
    if (categories == 0)
        return;

    auto fmt = [&](double v) { return BrFormat::format(v, m_valueFormat, ctx.locale()); };
    double tickWidth = 0.0;
    for (double t = sc.min; t <= sc.max + sc.step / 2; t += sc.step)
        tickWidth = std::max(tickWidth, BrTextMetrics::width(f, fmt(t)));
    double catWidth = 0.0;
    for (const QString &l : m_curLabels)
        catWidth = std::max(catWidth, BrTextMetrics::width(f, l));

    QRectF plot;
    if (horizontal)
        plot = area.adjusted(std::min(catWidth, area.width() * 0.35) + 1.5, 0, -tickWidth / 2, -(lh + 1));
    else
        plot = area.adjusted(tickWidth + 1.5, lh * 0.5, 0, -(lh + 1));
    if (plot.width() <= 1 || plot.height() <= 1)
        return;

    auto valuePos = [&](double v) {
        const double t = (v - sc.min) / (sc.max - sc.min);
        return horizontal ? plot.left() + t * plot.width() : plot.bottom() - t * plot.height();
    };

    // Grid and value axis labels
    for (double t = sc.min; t <= sc.max + sc.step / 2; t += sc.step) {
        const double p = valuePos(t);
        if (horizontal) {
            page.items << BrPageItem::makeLine(QPointF(p, plot.top()), QPointF(p, plot.bottom()), kGrid, 0.15);
            page.items << BrPageItem::makeText(QRectF(p - 15, plot.bottom() + 0.5, 30, lh), fmt(t), f,
                                               kAxis, Qt::AlignHCenter | Qt::AlignTop);
        } else {
            page.items << BrPageItem::makeLine(QPointF(plot.left(), p), QPointF(plot.right(), p), kGrid, 0.15);
            page.items << BrPageItem::makeText(QRectF(area.left(), p - lh / 2, tickWidth, lh), fmt(t), f,
                                               kAxis, Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    const int nSeries = int(m_curSeries.size());
    const double band = (horizontal ? plot.height() : plot.width()) / categories;
    const double barSize = band * 0.75 / nSeries;
    const double zero = valuePos(std::max(sc.min, std::min(0.0, sc.max)));
    for (int c = 0; c < categories; ++c) {
        const double bandStart = (horizontal ? plot.top() : plot.left()) + c * band;
        for (int s = 0; s < nSeries; ++s) {
            const Series &series = m_curSeries.at(s);
            if (c >= series.values.size())
                continue;
            const double v = series.values.at(c);
            const double p = valuePos(v);
            const double offset = bandStart + band * 0.125 + s * barSize;
            const QColor col = series.color.isValid() ? series.color : colorAt(s);
            QRectF bar = horizontal ? QRectF(QPointF(std::min(zero, p), offset),
                                             QPointF(std::max(zero, p), offset + barSize))
                                    : QRectF(QPointF(offset, std::min(zero, p)),
                                             QPointF(offset + barSize, std::max(zero, p)));
            page.items << BrPageItem::makeRect(bar, QColor(), 0, col);
            if (m_showValues) {
                const QString text = fmt(v);
                if (horizontal)
                    page.items << BrPageItem::makeText(QRectF(bar.right() + 0.8, bar.top(), 30, bar.height()),
                                                       text, f, color(), Qt::AlignLeft | Qt::AlignVCenter);
                else
                    page.items << BrPageItem::makeText(QRectF(bar.center().x() - 15, bar.top() - lh, 30, lh),
                                                       text, f, color(), Qt::AlignHCenter | Qt::AlignBottom);
            }
        }
        const QString label = m_curLabels.value(c);
        if (horizontal)
            page.items << BrPageItem::makeText(QRectF(area.left(), bandStart, plot.left() - area.left() - 1.5, band),
                                               label, f, color(), Qt::AlignRight | Qt::AlignVCenter);
        else
            page.items << BrPageItem::makeText(QRectF(bandStart, plot.bottom() + 0.5, band, lh), label, f,
                                               color(), Qt::AlignHCenter | Qt::AlignTop);
    }
    // Base line
    if (horizontal)
        page.items << BrPageItem::makeLine(QPointF(zero, plot.top()), QPointF(zero, plot.bottom()), kAxis, 0.25);
    else
        page.items << BrPageItem::makeLine(QPointF(plot.left(), zero), QPointF(plot.right(), zero), kAxis, 0.25);
}

void BrChart::renderLines(BrRenderContext &ctx, const QRectF &area, BrPage &page)
{
    const Scale sc = computeScale(m_curSeries);
    const QFont f = scaled(font(), 0.85);
    const double lh = BrTextMetrics::lineHeight(f);
    int categories = m_curLabels.size();
    for (const Series &s : m_curSeries)
        categories = std::max(categories, int(s.values.size()));
    if (categories == 0)
        return;
    auto fmt = [&](double v) { return BrFormat::format(v, m_valueFormat, ctx.locale()); };
    double tickWidth = 0.0;
    for (double t = sc.min; t <= sc.max + sc.step / 2; t += sc.step)
        tickWidth = std::max(tickWidth, BrTextMetrics::width(f, fmt(t)));
    const QRectF plot = area.adjusted(tickWidth + 1.5, lh * 0.5, 0, -(lh + 1));
    if (plot.width() <= 1 || plot.height() <= 1)
        return;
    auto yOf = [&](double v) { return plot.bottom() - (v - sc.min) / (sc.max - sc.min) * plot.height(); };
    const double band = plot.width() / categories;
    auto xOf = [&](int c) { return plot.left() + band * (c + 0.5); };

    for (double t = sc.min; t <= sc.max + sc.step / 2; t += sc.step) {
        const double y = yOf(t);
        page.items << BrPageItem::makeLine(QPointF(plot.left(), y), QPointF(plot.right(), y), kGrid, 0.15);
        page.items << BrPageItem::makeText(QRectF(area.left(), y - lh / 2, tickWidth, lh), fmt(t), f,
                                           kAxis, Qt::AlignRight | Qt::AlignVCenter);
    }
    for (int c = 0; c < categories; ++c)
        page.items << BrPageItem::makeText(QRectF(xOf(c) - band / 2, plot.bottom() + 0.5, band, lh),
                                           m_curLabels.value(c), f, color(), Qt::AlignHCenter | Qt::AlignTop);
    page.items << BrPageItem::makeLine(plot.bottomLeft(), plot.bottomRight(), kAxis, 0.25);

    for (int s = 0; s < m_curSeries.size(); ++s) {
        const Series &series = m_curSeries.at(s);
        const QColor col = series.color.isValid() ? series.color : colorAt(s);
        for (int c = 0; c + 1 < series.values.size(); ++c)
            page.items << BrPageItem::makeLine(QPointF(xOf(c), yOf(series.values.at(c))),
                                               QPointF(xOf(c + 1), yOf(series.values.at(c + 1))), col, 0.5);
        for (int c = 0; c < series.values.size(); ++c) {
            const QPointF p(xOf(c), yOf(series.values.at(c)));
            BrPageItem dot;
            dot.kind = BrPageItem::Ellipse;
            dot.rect = QRectF(p.x() - 0.8, p.y() - 0.8, 1.6, 1.6);
            dot.brushColor = col;
            dot.penColor = Qt::white;
            dot.penWidth = 0.2;
            page.items << dot;
            if (m_showValues)
                page.items << BrPageItem::makeText(QRectF(p.x() - 15, p.y() - lh - 0.8, 30, lh),
                                                   fmt(series.values.at(c)), f, color(),
                                                   Qt::AlignHCenter | Qt::AlignBottom);
        }
    }
}
