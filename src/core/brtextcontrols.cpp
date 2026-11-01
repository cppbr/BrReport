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

#include "brtextcontrols.h"

#include "brband.h"
#include "brformat.h"
#include "brrendercontext.h"
#include "brreport.h"
#include "brtextmetrics.h"

#include <QDate>
#include <QDateTime>

namespace {

int horizontalFlag(Br::TextAlign a)
{
    switch (a) {
    case Br::TextAlign::Center:
        return Qt::AlignHCenter;
    case Br::TextAlign::Right:
        return Qt::AlignRight;
    default:
        return Qt::AlignLeft;
    }
}

int verticalFlag(Br::TextLayout l)
{
    switch (l) {
    case Br::TextLayout::Top:
        return Qt::AlignTop;
    case Br::TextLayout::Bottom:
        return Qt::AlignBottom;
    default:
        return Qt::AlignVCenter;
    }
}

int compareValues(const QVariant &a, const QVariant &b)
{
    if (BrFormat::isNumeric(a) && BrFormat::isNumeric(b)) {
        const double x = a.toDouble(), y = b.toDouble();
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    if (BrFormat::isDateTime(a) && BrFormat::isDateTime(b)) {
        const QDateTime x = a.toDateTime(), y = b.toDateTime();
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    return QString::compare(a.toString(), b.toString());
}

} // namespace

// ---------------------------------------------------------------------------
// BrCustomText
// ---------------------------------------------------------------------------

BrCustomText::BrCustomText(BrContainer *parent) : BrControl(parent) {}

void BrCustomText::prepare(BrRenderContext &ctx)
{
    if (ctx.isDesignMode()) {
        m_currentText = designText(ctx);
        m_highlighted = false;
        return;
    }
    m_currentText = computeText(ctx);
    m_highlighted = m_highlightIf.isValid() && ctx.evaluate(m_highlightIf).toBool();
}

double BrCustomText::measureHeight(BrRenderContext &, double width)
{
    if (!m_autoStretch || !qFuzzyIsNull(m_angle) || m_shrinkToFit)
        return height();
    if (m_currentText.isEmpty())
        return m_hideIfEmpty ? 0.0 : height();
    const QFont f = font();
    if (m_allowHtml)
        return BrTextMetrics::richTextHeight(f, m_currentText, width - 2 * m_padding) + 2 * m_padding;
    const QStringList lines =
        BrTextMetrics::wrap(f, m_currentText, width - 2 * m_padding, m_wordWrap);
    return lines.size() * BrTextMetrics::lineHeight(f) + 2 * m_padding;
}

QFont BrCustomText::fittingFont(const QRectF &inner) const
{
    QFont f = font();
    if (!m_shrinkToFit || m_currentText.isEmpty() || !qFuzzyIsNull(m_angle))
        return f;
    double size = f.pointSizeF() > 0 ? f.pointSizeF() : 10.0;
    for (; size > 4.0; size -= 0.5) {
        f.setPointSizeF(size);
        bool fits;
        if (m_allowHtml) {
            fits = BrTextMetrics::richTextHeight(f, m_currentText, inner.width()) <= inner.height() + 0.01;
        } else {
            const QStringList lines = BrTextMetrics::wrap(f, m_currentText, inner.width(), m_wordWrap);
            fits = lines.size() * BrTextMetrics::lineHeight(f) <= inner.height() + 0.01;
            for (const QString &line : lines)
                if (BrTextMetrics::width(f, line) > inner.width() + 0.01)
                    fits = false;
        }
        if (fits)
            return f;
    }
    f.setPointSizeF(4.0);
    return f;
}

void BrCustomText::render(BrRenderContext &, const QRectF &rect, BrPage &page)
{
    if (m_currentText.isEmpty()) {
        if (!m_hideIfEmpty)
            renderFrame(rect, page);
        return;
    }
    if (m_highlighted && m_highlightBackground.isValid()) {
        const QColor saved = background();
        setBackground(m_highlightBackground);
        renderFrame(rect, page);
        setBackground(saved);
    } else {
        renderFrame(rect, page);
    }

    const QRectF inner = rect.adjusted(m_padding, m_padding, -m_padding, -m_padding);
    QFont f = fittingFont(inner);
    if (m_highlighted && m_highlightBold)
        f.setBold(true);
    const QColor c = m_highlighted && m_highlightColor.isValid() ? m_highlightColor : color();
    const int hflag = horizontalFlag(m_alignment);
    const int vflag = verticalFlag(m_layout);

    if (m_allowHtml) {
        BrPageItem item = BrPageItem::makeText(inner, m_currentText, f, c, hflag | Qt::AlignTop);
        item.kind = BrPageItem::RichText;
        if (m_alignment == Br::TextAlign::Justify)
            item.alignment = Qt::AlignJustify | Qt::AlignTop;
        page.items << item;
        return;
    }

    if (!qFuzzyIsNull(m_angle)) {
        page.items << BrPageItem::makeText(inner, m_currentText, f, c, hflag | vflag, m_angle);
        return;
    }

    QList<bool> paragraphEnds;
    QStringList lines = BrTextMetrics::wrap(f, m_currentText, inner.width(), m_wordWrap,
                                            &paragraphEnds);
    const bool justify = m_alignment == Br::TextAlign::Justify;
    if (lines.size() == 1 && !justify) {
        page.items << BrPageItem::makeText(inner, lines.first(), f, c, hflag | vflag);
        return;
    }

    const double lh = BrTextMetrics::lineHeight(f);
    // Lines that do not fit are clipped (keep at least one).
    const int fit = qMax(1, int((inner.height() + 0.01) / lh));
    if (lines.size() > fit) {
        lines = lines.mid(0, fit);
        paragraphEnds = paragraphEnds.mid(0, fit);
    }
    const double blockHeight = lines.size() * lh;
    double y = inner.top();
    if (m_layout == Br::TextLayout::Center)
        y = inner.center().y() - blockHeight / 2;
    else if (m_layout == Br::TextLayout::Bottom)
        y = inner.bottom() - blockHeight;

    for (int i = 0; i < lines.size(); ++i, y += lh) {
        const QRectF lineRect(inner.left(), y, inner.width(), lh);
        const QString &line = lines.at(i);
        if (justify && !paragraphEnds.value(i, true)) {
            const QStringList words = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (words.size() > 1) {
                double total = 0.0;
                QList<double> widths;
                for (const QString &w : words) {
                    widths << BrTextMetrics::width(f, w);
                    total += widths.last();
                }
                const double gap = (inner.width() - total) / (words.size() - 1);
                double x = inner.left();
                for (int w = 0; w < words.size(); ++w) {
                    page.items << BrPageItem::makeText(QRectF(x, y, widths.at(w) + 0.5, lh),
                                                       words.at(w), f, c,
                                                       Qt::AlignLeft | Qt::AlignVCenter);
                    x += widths.at(w) + gap;
                }
                continue;
            }
        }
        page.items << BrPageItem::makeText(lineRect, line, f, c, hflag | Qt::AlignVCenter);
    }
}

// ---------------------------------------------------------------------------
// BrLabel / BrMemo
// ---------------------------------------------------------------------------

BrLabel::BrLabel(BrContainer *parent) : BrCustomText(parent) {}

BrLabel::BrLabel(const QString &text, BrContainer *parent) : BrCustomText(parent), m_text(text) {}

QString BrLabel::computeText(BrRenderContext &)
{
    return m_text;
}

BrMemo::BrMemo(BrContainer *parent) : BrLabel(parent)
{
    setWordWrap(true);
    setAutoStretch(true);
    setLayout(Br::TextLayout::Top);
}

// ---------------------------------------------------------------------------
// BrDBText / BrDBMemo
// ---------------------------------------------------------------------------

BrDBText::BrDBText(BrContainer *parent) : BrCustomText(parent) {}

BrDBText::BrDBText(const QString &dataField, BrContainer *parent)
    : BrCustomText(parent), m_dataField(dataField)
{
}

void BrDBText::setDataField(const QString &field)
{
    m_dataField = field;
    if (!field.isEmpty())
        m_formula.setSource(QString());
}

void BrDBText::setDataFormula(const QString &expression)
{
    m_formula.setSource(expression);
    if (!m_formula.isValid() && !m_formula.isEmpty())
        qWarning("BrDBText: invalid formula '%s': %s", qPrintable(expression),
                 qPrintable(m_formula.errorString()));
    if (!expression.isEmpty())
        m_dataField.clear();
}

QVariant BrDBText::fetchValue(BrRenderContext &ctx) const
{
    if (m_formula.isValid())
        return ctx.evaluate(m_formula);
    if (!m_dataField.isEmpty())
        return ctx.fieldValue(m_dataField);
    return {};
}

QString BrDBText::formatValue(const QVariant &value, BrRenderContext &ctx) const
{
    const QString text = BrFormat::format(value, m_displayFormat, ctx.locale());
    if (text.isEmpty())
        return text;
    return m_prefix + text + m_suffix;
}

QString BrDBText::computeText(BrRenderContext &ctx)
{
    m_value = fetchValue(ctx);
    return formatValue(m_value, ctx);
}

QString BrDBText::designText(BrRenderContext &)
{
    if (m_formula.isValid() || !m_formula.isEmpty())
        return m_prefix + QLatin1Char('=') + m_formula.source() + m_suffix;
    if (!m_dataField.isEmpty())
        return m_prefix + QLatin1Char('[') + m_dataField + QLatin1Char(']') + m_suffix;
    return QStringLiteral("[ ]");
}

BrDBMemo::BrDBMemo(BrContainer *parent) : BrDBText(parent)
{
    setWordWrap(true);
    setAutoStretch(true);
    setLayout(Br::TextLayout::Top);
}

BrDBMemo::BrDBMemo(const QString &dataField, BrContainer *parent) : BrDBMemo(parent)
{
    setDataField(dataField);
}

// ---------------------------------------------------------------------------
// BrDBResult
// ---------------------------------------------------------------------------

BrDBResult::BrDBResult(BrContainer *parent) : BrDBText(parent) {}

BrDBResult::BrDBResult(const QString &dataField, Br::ResultType type, BrContainer *parent)
    : BrDBText(dataField, parent), m_type(type)
{
}

BrPager *BrDBResult::ownerPager() const
{
    BrBand *b = band();
    return b ? b->pager() : nullptr;
}

void BrDBResult::reset()
{
    m_sum = 0.0;
    m_count = 0;
    m_first = m_last = m_min = m_max = QVariant();
}

void BrDBResult::accumulate(BrRenderContext &ctx)
{
    const QVariant v = fetchValue(ctx);
    if (!v.isValid() || v.isNull())
        return;
    ++m_count;
    if (BrFormat::isNumeric(v))
        m_sum += v.toDouble();
    if (m_count == 1) {
        m_first = m_min = m_max = v;
    } else {
        if (compareValues(v, m_min) < 0)
            m_min = v;
        if (compareValues(v, m_max) > 0)
            m_max = v;
    }
    m_last = v;
}

QVariant BrDBResult::result() const
{
    switch (m_type) {
    case Br::ResultType::Sum:
        return m_sum;
    case Br::ResultType::Count:
        return m_count;
    case Br::ResultType::Average:
        return m_count > 0 ? QVariant(m_sum / double(m_count)) : QVariant(0.0);
    case Br::ResultType::Min:
        return m_min;
    case Br::ResultType::Max:
        return m_max;
    case Br::ResultType::First:
        return m_first;
    case Br::ResultType::Last:
        return m_last;
    }
    return {};
}

QString BrDBResult::designText(BrRenderContext &)
{
    static const char *names[] = {"Sum", "Count", "Average", "Min", "Max", "First", "Last"};
    const QString what = !dataFormula().isEmpty() ? dataFormula() : dataField();
    return prefix() + QString::fromLatin1(names[int(m_type)]) + QLatin1Char('(') + what
           + QLatin1Char(')') + suffix();
}

QString BrDBResult::computeText(BrRenderContext &ctx)
{
    return formatValue(result(), ctx);
}

void BrDBResult::render(BrRenderContext &ctx, const QRectF &rect, BrPage &page)
{
    // The value may have changed between measuring and painting (see BrReport docs).
    if (!ctx.isDesignMode())
        setCurrentText(computeText(ctx));
    BrDBText::render(ctx, rect, page);
    if (m_resetAfterPrint)
        reset();
}

// ---------------------------------------------------------------------------
// BrSystemInfo
// ---------------------------------------------------------------------------

BrSystemInfo::BrSystemInfo(BrContainer *parent) : BrCustomText(parent) {}

BrSystemInfo::BrSystemInfo(Br::InfoType info, BrContainer *parent)
    : BrCustomText(parent), m_info(info)
{
}

QString BrSystemInfo::computeText(BrRenderContext &ctx)
{
    QString fmt = m_format;
    if (fmt.isEmpty())
        fmt = m_info == Br::InfoType::PagePreview ? QStringLiteral("%1/%2") : QStringLiteral("%1");
    if (!fmt.contains(QLatin1String("%1")))
        fmt += QLatin1String("%1");

    const QLocale loc = ctx.locale();
    const QDateTime dt = ctx.reportDateTime();
    QString value;
    switch (m_info) {
    case Br::InfoType::Date:
        value = BrFormat::format(dt.date(), m_displayFormat, loc);
        break;
    case Br::InfoType::Time:
        value = BrFormat::format(dt.time(), m_displayFormat, loc);
        break;
    case Br::InfoType::DateTime:
        value = BrFormat::format(dt, m_displayFormat, loc);
        break;
    case Br::InfoType::PageNumber:
        value = ctx.pageNumberText();
        break;
    case Br::InfoType::LastPageNumber:
        value = ctx.pageCountText();
        break;
    case Br::InfoType::PagePreview:
        if (fmt.contains(QLatin1String("%2")))
            return fmt.arg(ctx.pageNumberText(), ctx.pageCountText());
        value = ctx.pageNumberText();
        break;
    case Br::InfoType::Title:
        value = ctx.report() ? ctx.report()->title() : QString();
        break;
    case Br::InfoType::RecNo:
        value = QString::number(ctx.recNo());
        break;
    case Br::InfoType::DetailCount:
        value = QString::number(ctx.detailCount());
        break;
    case Br::InfoType::CarbonCopy:
        value = QString::number(ctx.carbonCopy());
        break;
    case Br::InfoType::DocumentPageNumber:
        value = QString::number(ctx.documentPageNumber());
        break;
    case Br::InfoType::DocumentPageCount:
        value = ctx.documentPageCountText();
        break;
    }
    return fmt.arg(value);
}
