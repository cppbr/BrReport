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

#include "brengine_p.h"

#include "brband.h"
#include "brdatasource.h"
#include "brpager.h"
#include "brreport.h"
#include "brtextcontrols.h"
#include "brtextmetrics.h"

#include <QHash>

#include <algorithm>
#include <functional>
#include <limits>

namespace {
const double kEpsilon = 0.001;
}

BrEngine::BrEngine(BrReport *report) : m_report(report) {}

BrEngine::~BrEngine() = default;

// Short placeholders (control characters) so that they hardly affect text measuring.
QString BrEngine::pageNumberToken()
{
    return QStringLiteral("\u0001pn\u0001");
}

QString BrEngine::pageCountToken()
{
    return QStringLiteral("\u0001pc\u0001");
}

QString BrEngine::documentPageCountToken()
{
    return QStringLiteral("\u0001dc\u0001");
}

QString BrEngine::pageNumberText() const
{
    return pageNumberToken();
}

QString BrEngine::pageCountText() const
{
    return pageCountToken();
}

QString BrEngine::documentPageCountText() const
{
    return documentPageCountToken();
}

int BrEngine::pageNumber() const
{
    if (m_pageSection.isEmpty())
        return m_pageNumber;
    const int last = int(m_pageSection.size()) - 1;
    const int section = m_pageSection.at(last);
    int first = last;
    while (first > 0 && m_pageSection.at(first - 1) == section)
        --first;
    return last - first + 1;
}

void BrEngine::startNumberingSection()
{
    m_section = m_sectionCount++;
    if (!m_pageSection.isEmpty())
        m_pageSection.last() = m_section; // the current page is the first of the section
}

void BrEngine::resolvePageNumbers()
{
    QHash<int, int> sectionPages;
    for (int s : m_pageSection)
        ++sectionPages[s];
    const QString total = QString::number(m_doc->pageCount());
    int number = 0;
    for (int i = 0; i < m_doc->pages.size(); ++i) {
        const int section = m_pageSection.value(i);
        number = (i > 0 && m_pageSection.value(i - 1) == section) ? number + 1 : 1;
        const QString pn = QString::number(number);
        const QString pc = QString::number(sectionPages.value(section));
        for (BrPageItem &item : m_doc->pages[i].items) {
            if ((item.kind != BrPageItem::Text && item.kind != BrPageItem::RichText)
                || !item.text.contains(QChar(0x0001)))
                continue;
            item.text.replace(pageNumberToken(), pn);
            item.text.replace(pageCountToken(), pc);
            item.text.replace(documentPageCountToken(), total);
        }
    }
}

// ---------------------------------------------------------------------------
// Context
// ---------------------------------------------------------------------------

int BrEngine::recNo() const
{
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it)
        if ((*it)->ownsDataSource)
            return (*it)->recNo;
    return 0;
}

QVariant BrEngine::fieldValue(const QString &name, bool *found) const
{
    // Qualified name: "<data source objectName>.<field>"
    const int dot = name.indexOf(QLatin1Char('.'));
    if (dot > 0) {
        const QString dsName = name.left(dot);
        const QString field = name.mid(dot + 1);
        for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
            BrDataSource *ds = (*it)->dataSource;
            if (ds && ds->objectName().compare(dsName, Qt::CaseInsensitive) == 0
                && ds->hasField(field)) {
                if (found)
                    *found = true;
                return ds->value(field);
            }
        }
    }
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
        BrDataSource *ds = (*it)->dataSource;
        if (ds && !ds->eof() && ds->hasField(name)) {
            if (found)
                *found = true;
            return ds->value(name);
        }
    }
    if (found)
        *found = false;
    return {};
}

QVariant BrEngine::variable(const QString &name, bool *found) const
{
    bool ok = false;
    QVariant v = fieldValue(name, &ok);
    if (ok) {
        *found = true;
        return v;
    }
    v = m_report->variable(name, &ok);
    if (ok) {
        *found = true;
        return v;
    }
    // A field that exists but has no current record (e.g. after the last one)
    // evaluates to null instead of raising an error.
    for (const auto &frame : m_stack) {
        if (frame->dataSource && frame->dataSource->hasField(name)) {
            *found = true;
            return {};
        }
    }
    *found = false;
    return {};
}

QVariant BrEngine::function(const QString &name, const QVariantList &args, bool *found) const
{
    *found = true;
    if (const BrReport::CustomFunction f = m_report->function(name))
        return f(args);
    if (name == QLatin1String("page"))
        return pageNumber();
    if (name == QLatin1String("pages") || name == QLatin1String("pagecount"))
        return pageCountToken();
    if (name == QLatin1String("documentpage"))
        return m_pageNumber;
    if (name == QLatin1String("documentpages"))
        return documentPageCountToken();
    if (name == QLatin1String("recno"))
        return recNo();
    if (name == QLatin1String("detailcount"))
        return m_detailCount;
    if (name == QLatin1String("title"))
        return m_report->title();
    if (name == QLatin1String("reportdate"))
        return m_dateTime;
    if (name == QLatin1String("var") || name == QLatin1String("variable"))
        return m_report->variable(args.value(0).toString());
    if (name == QLatin1String("field")) {
        return fieldValue(args.value(0).toString());
    }
    *found = false;
    return {};
}

QLocale BrEngine::locale() const
{
    return m_report->locale();
}

// ---------------------------------------------------------------------------
// Stack helpers
// ---------------------------------------------------------------------------

BrEngine::Frame *BrEngine::push(BrPager *pager, BrDataSource *ds, bool ownsDs)
{
    auto frame = std::make_unique<Frame>();
    frame->pager = pager;
    frame->dataSource = ds;
    frame->ownsDataSource = ownsDs;
    frame->isReport = pager == m_report;
    m_stack.push_back(std::move(frame));
    return m_stack.back().get();
}

void BrEngine::pop()
{
    m_stack.pop_back();
}

bool BrEngine::aborted() const
{
    return m_abort || m_report->isCanceled();
}

BrDataSource *BrEngine::currentDataSource() const
{
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it)
        if ((*it)->ownsDataSource)
            return (*it)->dataSource;
    return nullptr;
}

BrPager *BrEngine::dataOwnerOf(BrPager *pager)
{
    for (BrPager *p = pager; p; p = p->parentPager())
        if (qobject_cast<BrSubDetail *>(p) || qobject_cast<BrReport *>(p))
            return p;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------

bool BrEngine::run(BrDocument *document, QString *error)
{
    m_doc = document;
    m_doc->clear();
    m_doc->title = m_report->title();
    m_dateTime = m_report->reportDateTime().isValid() ? m_report->reportDateTime()
                                                       : QDateTime::currentDateTime();
    const QRectF content = m_report->contentRect();
    m_left = content.left();
    m_width = content.width();
    m_pageBottom = m_report->isContinuous() ? content.top() + 1.0e7 : content.bottom();
    if (m_width <= 0 || content.height() <= 0) {
        if (error)
            *error = QStringLiteral("The page margins leave no printable area.");
        return false;
    }

    m_results = m_report->findChildren<BrDBResult *>();

    Frame *rf = push(m_report, m_report->dataSource(), true);
    rf->headersPrinted = true;
    resetPager(m_report);
    QList<BrSubDetail *> frontPages;
    for (QObject *child : m_report->children())
        if (auto *sub = qobject_cast<BrSubDetail *>(child))
            if (sub->positioning() == Br::BandType::Title && sub->isEnabled())
                frontPages << sub;
    m_frontPages = !frontPages.isEmpty();
    newPage();
    if (m_frontPages) {
        for (BrSubDetail *sub : std::as_const(frontPages))
            runSubDetail(sub);
        m_frontPages = false;
        if (!m_pendingBreak) // otherwise the next page prints them
            printSlot(m_report, int(Br::BandType::ColumnHeader), Mode::Fixed);
    }
    iterateRecords(*rf);
    finishPager(*rf);
    m_lastPage = true;
    closePage();
    pop();

    resolvePageNumbers();

    if (error)
        *error = m_error;
    if (m_abort)
        return false;
    if (m_report->isCanceled()) {
        if (error)
            *error = QStringLiteral("Canceled.");
        return false;
    }
    return true;
}

void BrEngine::iterateRecords(Frame &frame)
{
    BrDataSource *ds = frame.dataSource;
    if (!ds) {
        // No data: print the detail bands once (static reports, forms).
        frame.recNo = 1;
        printRecord();
        return;
    }
    if (!ds->first())
        return;
    frame.recNo = 1;
    while (!ds->eof() && !aborted()) {
        if (BrGroup *g = frame.pager->group())
            runGroup(g);
        else {
            printRecord();
            advance();
        }
    }
}

void BrEngine::advance()
{
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
        if ((*it)->ownsDataSource) {
            if ((*it)->dataSource) {
                (*it)->dataSource->next();
                skipUnlinked((*it)->dataSource, (*it)->linkFields, (*it)->linkValues);
                ++(*it)->recNo;
            }
            return;
        }
    }
}

bool BrEngine::linkMatches(BrDataSource *ds, const QStringList &fields, const QStringList &values)
{
    for (int i = 0; i < fields.size(); ++i)
        if (ds->value(fields.at(i).trimmed()).toString() != values.value(i))
            return false;
    return true;
}

void BrEngine::skipUnlinked(BrDataSource *ds, const QStringList &fields, const QStringList &values)
{
    if (fields.isEmpty())
        return;
    while (!ds->eof() && !linkMatches(ds, fields, values))
        ds->next();
}

bool BrEngine::groupBroken()
{
    for (const auto &frame : m_stack) {
        if (auto *g = qobject_cast<BrGroup *>(frame->pager))
            if (g->key(*this) != frame->key)
                return true;
    }
    return false;
}

void BrEngine::runGroup(BrGroup *group)
{
    BrDataSource *ds = currentDataSource();
    if (!ds || ds->eof())
        return;

    if (group->pageBreak() == Br::PageBreak::Before && pageHasBody())
        newPage();

    Frame *f = push(group, ds, false);
    f->key = group->key(*this);
    resetPager(group);
    keepHeaderWithDetail(group);
    if (group->resetPageNumber())
        startNumberingSection();
    printSlot(group, int(Br::BandType::Title), Mode::Flow);
    printSlot(group, int(Br::BandType::Header), Mode::Flow);
    printSlot(group, int(Br::BandType::ColumnHeader), Mode::Flow);
    f->headersPrinted = true;

    bool first = true;
    while (!ds->eof() && !aborted()) {
        if (!first && groupBroken())
            break;
        first = false;
        if (BrGroup *child = group->group())
            runGroup(child);
        else {
            printRecord();
            advance();
        }
    }

    finishPager(*f);
    pop();
    if (group->pageBreak() == Br::PageBreak::After)
        m_pendingBreak = true;
}

void BrEngine::runSubDetail(BrSubDetail *sub)
{
    if (!sub->isEnabled())
        return;
    sub->callBeforeOpen(*this);
    BrDataSource *ds = sub->dataSource();
    // Without data, a sub-report placed before or after the records is a page of the
    // template (cover, terms): it prints once.
    const bool templatePage = !ds && sub->dataSourceName().isEmpty()
                              && sub->positioning() != Br::BandType::Detail;
    if (!templatePage && (!ds || !ds->first()))
        return;

    // Declarative master/detail link (evaluated in the master context)
    QStringList linkFields, linkValues;
    if (ds) {
        linkFields = sub->detailFields().split(QLatin1Char(';'), Qt::SkipEmptyParts);
        const QStringList masterFields = sub->masterFields().split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (int i = 0; i < linkFields.size(); ++i)
            linkValues << fieldValue(masterFields.value(i, linkFields.at(i)).trimmed()).toString();
        skipUnlinked(ds, linkFields, linkValues);
        if (ds->eof())
            return;
    }

    if (sub->pageBreak() == Br::PageBreak::Before && pageHasBody())
        newPage();

    Frame *f = push(sub, ds, ds != nullptr);
    f->recNo = 1;
    f->linkFields = linkFields;
    f->linkValues = linkValues;
    resetPager(sub);
    keepHeaderWithDetail(sub);
    if (sub->resetPageNumber())
        startNumberingSection();
    printSlot(sub, int(Br::BandType::Title), Mode::Flow);
    printSlot(sub, int(Br::BandType::Header), Mode::Flow);
    printSlot(sub, int(Br::BandType::ColumnHeader), Mode::Flow);
    f->headersPrinted = true;

    if (templatePage)
        printRecord();
    while (ds && !ds->eof() && !aborted()) {
        if (BrGroup *g = sub->group())
            runGroup(g);
        else {
            printRecord();
            advance();
        }
    }

    finishPager(*f);
    pop();
    if (sub->pageBreak() == Br::PageBreak::After)
        m_pendingBreak = true;
}

void BrEngine::finishPager(Frame &frame)
{
    frame.ending = true;
    if (!frame.columnFooterPrinted) {
        printSlot(frame.pager, int(Br::BandType::ColumnFooter), Mode::Flow);
        frame.columnFooterPrinted = true;
    }
    printSlot(frame.pager, int(Br::BandType::Summary), Mode::Flow);
    if (!frame.isReport && !frame.footerPrinted) {
        printSlot(frame.pager, int(Br::BandType::Footer), Mode::Flow);
        frame.footerPrinted = true;
    }
}

void BrEngine::printRecord()
{
    ++m_detailCount;
    m_pendingAccumulate = true;
    BrPager *pager = top().pager;
    for (QObject *child : pager->children()) {
        if (aborted())
            break;
        if (auto *band = qobject_cast<BrBand *>(child)) {
            if (band->bandType() == Br::BandType::Detail)
                printBand(band, Mode::Flow);
        } else if (auto *sub = qobject_cast<BrSubDetail *>(child)) {
            if (sub->positioning() == Br::BandType::Detail) {
                if (m_pendingAccumulate)
                    accumulate();
                runSubDetail(sub);
            }
        }
    }
    if (m_pendingAccumulate)
        accumulate();
}

// ---------------------------------------------------------------------------
// Bands
// ---------------------------------------------------------------------------

void BrEngine::printSlot(BrPager *pager, int bandType, Mode mode)
{
    const auto type = Br::BandType(bandType);
    // Copy: handlers may create/destroy objects (unusual, but keep it safe).
    const QObjectList children = pager->children();
    for (QObject *child : children) {
        if (auto *band = qobject_cast<BrBand *>(child)) {
            if (band->bandType() == type)
                printBand(band, mode);
        } else if (auto *sub = qobject_cast<BrSubDetail *>(child)) {
            if (mode == Mode::Flow && type != Br::BandType::Detail && sub->positioning() == type)
                runSubDetail(sub);
        }
    }
}

void BrEngine::printBand(BrBand *band, Mode mode, double *fixedY)
{
    if (mode == Mode::Flow && m_pendingBreak)
        newPage();
    if (!bandPrintsOnThisPage(band))
        return;

    const bool multiColumn = mode == Mode::Flow && !fixedY && band->columns() > 1
                             && band->bandType() == Br::BandType::Detail;
    if (mode == Mode::Flow && m_columnBand != band)
        endColumns(); // any other band ends a row of columns
    const bool down = multiColumn && band->columnDirection() == Br::ColumnDirection::Down;
    const double width = multiColumn
                             ? (m_width - band->columnSpacing() * (band->columns() - 1)) / band->columns()
                             : m_width;

    for (int copy = 1; copy <= band->carbonCopies(); ++copy) {
        m_carbon = copy;
        if (!band->checkPrint(*this))
            continue;
        band->prepare(*this);
        double h = band->measureHeight(*this, width);
        const bool newRow = !multiColumn || m_columnBand != band || (!down && m_column == 0);
        const bool canSplit = mode == Mode::Flow && !multiColumn && !m_report->isContinuous();
        const bool splittable = canSplit && band->allowSplit();

        if (mode == Mode::Flow) {
            bool broke = false;
            if (newRow) {
                if (band->pageBreak() == Br::PageBreak::Before && pageHasBody()) {
                    newPage();
                    broke = true;
                }
                if (!broke && m_y + h > bottomLimit() + kEpsilon && pageHasBody() && !splittable) {
                    newPage();
                    broke = true;
                }
            }
            if (broke) { // page-dependent content (page numbers) must be recomputed
                band->prepare(*this);
                h = band->measureHeight(*this, width);
            }
            if (m_pendingAccumulate && band->bandType() == Br::BandType::Detail)
                accumulate();
        }

        // Zebra stripes
        const QColor savedBackground = band->background();
        if (band->alternateBackground().isValid() && band->bandType() == Br::BandType::Detail) {
            if (m_zebra[band]++ % 2 == 1)
                band->setBackground(band->alternateBackground());
        }

        double x = m_left;
        double y = fixedY ? *fixedY : m_y;
        if (down) {
            bool startColumns = newRow;
            if (!newRow && m_y + h > bottomLimit() + kEpsilon) {
                if (++m_column < band->columns()) {
                    m_y = m_rowTop;
                } else {
                    newPage();
                    startColumns = true;
                    band->prepare(*this); // page-dependent content
                    h = band->measureHeight(*this, width);
                }
            }
            if (startColumns) {
                m_columnBand = band;
                m_columnsDown = true;
                m_column = 0;
                m_rowTop = m_y;
                m_columnBottom = m_y;
            }
            x = m_left + m_column * (width + band->columnSpacing());
            y = m_y;
        } else if (multiColumn) {
            if (newRow) {
                m_columnBand = band;
                m_columnsDown = false;
                m_column = 0;
                m_rowTop = m_y;
                m_rowHeight = 0.0;
            }
            x = m_left + m_column * (width + band->columnSpacing());
            y = m_rowTop;
        }
        // A band that is alone on the page and still does not fit can only be split.
        const bool split = canSplit && y + h > bottomLimit() + kEpsilon && (splittable || !pageHasBody());
        if (split)
            printSplit(band, x, width, h);
        else
            band->render(*this, QRectF(x, y, width, h), m_doc->pages.last());
        band->setBackground(savedBackground);

        if (down) {
            m_y += h;
            m_columnBottom = std::max(m_columnBottom, m_y);
        } else if (multiColumn) {
            if (h > m_rowHeight) {
                m_rowHeight = h;
                m_y = m_rowTop + h;
            }
            m_column = (m_column + 1) % band->columns();
        } else if (fixedY) {
            *fixedY += h;
        } else if (!split) {
            m_y += h;
        }
        band->callAfterPrint();
        if (mode == Mode::Flow && band->pageBreak() == Br::PageBreak::After)
            m_pendingBreak = true;
    }
    m_carbon = 1;
}

void BrEngine::printSplit(BrBand *band, double x, double width, double height)
{
    BrPage whole;
    QList<QRectF> keep;
    m_keepAreas = &keep;
    band->render(*this, QRectF(x, 0, width, height), whole);
    m_keepAreas = nullptr;

    // An item of the band, in coordinates from the top of the band.
    struct Part
    {
        QRectF bounds;
        QRectF block;                       // what must stay on one page with the item
        bool cuttable = false;              // frames: drawn on every page they pass through
        QList<QPair<double, double>> lines; // rich text: cut between these lines
        double shown = 0.0;                 // rich text: printed down to here
        bool placed = false;
    };
    QList<Part> parts;
    parts.reserve(whole.items.size());
    for (const BrPageItem &item : std::as_const(whole.items)) {
        Part part;
        part.bounds = item.points.isEmpty() ? item.rect : item.points.boundingRect();
        part.block = part.bounds;
        const bool verticalLine = item.kind == BrPageItem::Line && item.points.size() == 2
                                  && qFuzzyCompare(item.points.at(0).x() + 1.0, item.points.at(1).x() + 1.0);
        part.cuttable = part.bounds.height() > kEpsilon && (item.kind == BrPageItem::Rect || verticalLine);
        for (const QRectF &area : std::as_const(keep)) {
            if (area.contains(part.bounds.center()) && part.bounds.height() <= area.height() + kEpsilon) {
                part.block = area;
                part.cuttable = false;
                break;
            }
        }
        if (item.kind == BrPageItem::RichText) {
            part.shown = part.bounds.top();
            const auto lines = BrTextMetrics::richTextLines(item.font, item.text, item.rect.width());
            for (const auto &line : lines)
                part.lines << qMakePair(part.bounds.top() + line.first, part.bounds.top() + line.second);
        }
        parts << part;
    }
    const auto isRich = [&whole](int i) { return whole.items.at(i).kind == BrPageItem::RichText; };

    // Calls \a visit with the top of each piece that goes past \a limit.
    const auto forEachLeftOver = [&](double limit, const std::function<void(int, double, double)> &visit) {
        for (int i = 0; i < parts.size(); ++i) {
            const Part &part = parts.at(i);
            if (isRich(i)) {
                for (const auto &line : part.lines)
                    if (line.second > limit + kEpsilon && line.first >= part.shown - kEpsilon)
                        visit(i, line.first, line.second);
            } else if (!part.cuttable && !part.placed && part.block.bottom() > limit + kEpsilon) {
                visit(i, part.block.top(), part.block.bottom());
            }
        }
    };
    const auto nextTop = [&](double limit) {
        double next = limit;
        forEachLeftOver(limit, [&next](int, double top, double) { next = std::min(next, top); });
        return next;
    };

    // Puts on the page what is complete down to \a limit, \a top being at the current position.
    // What is left over goes to the next page whole, so columns of text with lines of
    // different heights lose at most one line each.
    const auto place = [&](double top, double limit) {
        QHash<int, double> richEnd;
        forEachLeftOver(limit, [&](int i, double lineTop, double) {
            if (isRich(i))
                richEnd[i] = std::min(richEnd.value(i, limit), lineTop);
        });
        const double dy = m_y - top;
        QList<BrPageItem> &items = m_doc->pages.last().items;
        for (int i = 0; i < parts.size(); ++i) {
            Part &part = parts[i];
            BrPageItem item = whole.items.at(i);
            if (isRich(i) || part.cuttable) {
                const double from = isRich(i) ? part.shown : std::max(part.bounds.top(), top);
                const double to = std::min(part.bounds.bottom(), isRich(i) ? richEnd.value(i, limit) : limit);
                if (to - from <= kEpsilon)
                    continue;
                if (isRich(i)) {
                    item.textOffset = from - part.bounds.top();
                    part.shown = to;
                }
                if (item.points.isEmpty()) {
                    item.rect.setTop(from);
                    item.rect.setBottom(to);
                } else {
                    item.points[0].setY(from);
                    item.points[1].setY(to);
                }
            } else {
                if (part.placed || part.block.bottom() > limit + kEpsilon)
                    continue;
                part.placed = true;
            }
            item.rect.translate(0, dy);
            item.points.translate(0, dy);
            items << item;
        }
    };

    double top = 0.0;
    while (!aborted()) {
        const double available = bottomLimit() - m_y;
        if (height - top <= available + kEpsilon) {
            place(top, std::numeric_limits<double>::max());
            m_y += height - top;
            return;
        }
        double limit = top + available;
        double next = nextTop(limit);
        if (next <= top + kEpsilon) {
            if (pageHasBody()) { // nothing fits here, but it may on an empty page
                newPage();
                continue;
            }
            // Taller than a page: it goes over the bottom, as there is nowhere else to put it.
            limit = top + std::max(available, 1.0);
            const double reference = limit;
            forEachLeftOver(reference, [&](int, double pieceTop, double pieceBottom) {
                if (pieceTop <= top + kEpsilon)
                    limit = std::max(limit, pieceBottom);
            });
            next = nextTop(limit);
        }
        place(top, limit);
        top = next;
        newPage();
    }
}

void BrEngine::endColumns()
{
    if (m_columnBand && m_columnsDown)
        m_y = std::max(m_y, m_columnBottom);
    m_columnBand = nullptr;
}

bool BrEngine::bandPrintsOnThisPage(BrBand *band) const
{
    if (band->pager() != m_report)
        return true;
    const Br::BandType t = band->bandType();
    if (t != Br::BandType::Header && t != Br::BandType::Footer)
        return true;
    if (m_pageNumber == 1 && !band->printOnFirstPage())
        return false;
    if (t == Br::BandType::Footer && m_lastPage && !band->printOnLastPage())
        return false;
    return true;
}

void BrEngine::keepHeaderWithDetail(BrPager *pager)
{
    if (!pager->keepHeaderWithDetail() || !pageHasBody() || m_report->isContinuous())
        return;
    double need = 0.0;
    for (BrPager *p = pager; p; p = p->group()) {
        need += bandsHeight(p, int(Br::BandType::Title)) + bandsHeight(p, int(Br::BandType::Header))
                + bandsHeight(p, int(Br::BandType::ColumnHeader));
        if (!p->group()) {
            for (BrBand *b : p->bands(Br::BandType::Detail)) {
                if (b->isVisible()) {
                    need += b->height();
                    break;
                }
            }
        }
    }
    if (m_y + need > bottomLimit() + kEpsilon)
        newPage();
}

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

bool BrEngine::pageHasBody() const
{
    return m_y > m_bodyTop + kEpsilon;
}

double BrEngine::bandsHeight(BrPager *pager, int bandType, bool onlyThisPage) const
{
    double h = 0.0;
    for (BrBand *b : pager->bands(Br::BandType(bandType)))
        if (b->isVisible() && (!onlyThisPage || bandPrintsOnThisPage(b)))
            h += b->height() * b->carbonCopies();
    return h;
}

double BrEngine::bottomLimit() const
{
    double limit = m_pageBottom - bandsHeight(m_report, int(Br::BandType::Footer));
    for (const auto &frame : m_stack) {
        if (frame->ending)
            continue;
        if (!(frame->isReport && m_frontPages))
            limit -= bandsHeight(frame->pager, int(Br::BandType::ColumnFooter));
        if (!frame->isReport)
            limit -= bandsHeight(frame->pager, int(Br::BandType::Footer));
    }
    return limit;
}

void BrEngine::newPage()
{
    if (m_hasPage && m_report->isContinuous()) {
        m_pendingBreak = false; // a continuous report has a single page
        return;
    }
    if (m_hasPage)
        closePage();
    if (m_doc->pageCount() >= m_report->maxPages()) {
        m_abort = true;
        m_error = QStringLiteral("Maximum number of pages (%1) reached.").arg(m_report->maxPages());
        if (m_hasPage)
            return; // keep drawing on the last page; loops stop because aborted()
    }

    BrPage page;
    page.size = m_report->pageSizeMm();
    m_doc->pages.append(page);
    m_pageSection.append(m_section);
    m_pageNumber = m_doc->pageCount();
    m_hasPage = true;
    m_pendingBreak = false;
    m_columnBand = nullptr;
    m_y = m_report->contentRect().top();
    if (!m_report->isContinuous()) // there the height is known only at the end
        drawWatermark();

    for (const auto &frame : m_stack) {
        if (frame->ending && !frame->isReport)
            continue;
        if (!frame->ending) {
            frame->columnFooterPrinted = false;
            frame->footerPrinted = false;
        }
        if (frame->isReport) {
            printSlot(frame->pager, int(Br::BandType::Header), Mode::Fixed);
            if (m_pageNumber == 1)
                printSlot(frame->pager, int(Br::BandType::Title), Mode::Fixed);
            if (!frame->ending && !m_frontPages)
                printSlot(frame->pager, int(Br::BandType::ColumnHeader), Mode::Fixed);
        } else if (frame->headersPrinted) {
            printSlot(frame->pager, int(Br::BandType::Header), Mode::Fixed);
            printSlot(frame->pager, int(Br::BandType::ColumnHeader), Mode::Fixed);
        }
    }
    m_bodyTop = m_y;

    if (m_report->m_pageHandler)
        m_report->m_pageHandler(m_pageNumber);
}

void BrEngine::closePage()
{
    endColumns();
    // Column footers / group footers right after the content, innermost first.
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
        Frame &f = **it;
        if (f.ending && !f.isReport)
            continue;
        if (!f.columnFooterPrinted && !(f.isReport && m_frontPages)) {
            printSlot(f.pager, int(Br::BandType::ColumnFooter), Mode::Fixed);
            f.columnFooterPrinted = true;
        }
        if (!f.isReport && !f.footerPrinted) {
            printSlot(f.pager, int(Br::BandType::Footer), Mode::Fixed);
            f.footerPrinted = true;
        }
    }
    // Report footer: anchored to the bottom of the printable area
    // (right after the content on a continuous page).
    const bool continuous = m_report->isContinuous();
    double y = continuous ? m_y
                          : m_pageBottom - bandsHeight(m_report, int(Br::BandType::Footer), true);
    for (BrBand *b : m_report->bands(Br::BandType::Footer))
        printBand(b, Mode::Fixed, &y);
    if (continuous) {
        m_doc->pages.last().size.setHeight(y + m_report->margins().bottom());
        drawWatermark();
    }
    m_hasPage = true;
}

void BrEngine::drawWatermark()
{
    const BrWatermark wm = m_report->watermark();
    if (!wm.isEnabled() || m_doc->pages.isEmpty())
        return;
    BrPage &page = m_doc->pages.last();
    const QRectF content = m_report->contentRect();
    QList<BrPageItem> items;
    if (!wm.image.isNull()) {
        QImage img = wm.image.convertToFormat(QImage::Format_ARGB32);
        const double opacity = qBound(0.0, wm.imageOpacity, 1.0);
        for (int yy = 0; yy < img.height(); ++yy) {
            auto *line = reinterpret_cast<QRgb *>(img.scanLine(yy));
            for (int xx = 0; xx < img.width(); ++xx)
                line[xx] = qRgba(qRed(line[xx]), qGreen(line[xx]), qBlue(line[xx]),
                                 int(qAlpha(line[xx]) * opacity));
        }
        const QRectF area(content.left(), content.top(), content.width(),
                          page.size.height() - m_report->margins().top() - m_report->margins().bottom());
        QSizeF size = img.size();
        size.scale(area.size(), Qt::KeepAspectRatio);
        QRectF target(QPointF(), size);
        target.moveCenter(area.center());
        items << BrPageItem::makeImage(target, img);
    }
    if (!wm.text.isEmpty()) {
        const QRectF rect(QPointF(0, 0), page.size);
        items << BrPageItem::makeText(rect, wm.text, wm.font, wm.color, Qt::AlignCenter, wm.angle);
    }
    page.items = items + page.items; // behind everything else
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

void BrEngine::resetPager(BrPager *pager)
{
    resetResults(pager);
    for (BrBand *b : pager->bands())
        m_zebra.remove(b);
}

void BrEngine::resetResults(BrPager *pager)
{
    for (BrDBResult *r : m_results)
        if (r->ownerPager() == pager)
            r->reset();
}

void BrEngine::accumulate()
{
    m_pendingAccumulate = false;
    BrPager *owner = nullptr;
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
        if ((*it)->ownsDataSource) {
            owner = (*it)->pager;
            break;
        }
    }
    for (BrDBResult *r : m_results) {
        BrPager *p = r->ownerPager();
        if (!p || dataOwnerOf(p) != owner)
            continue;
        bool active = false;
        for (const auto &frame : m_stack)
            if (frame->pager == p)
                active = true;
        if (active)
            r->accumulate(*this);
    }
}
