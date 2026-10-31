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

namespace {
const double kEpsilon = 0.001;
}

BrEngine::BrEngine(BrReport *report) : m_report(report) {}

BrEngine::~BrEngine() = default;

QString BrEngine::pageCountToken()
{
    return QStringLiteral("{{BR_PAGE_COUNT}}");
}

QString BrEngine::pageCountText() const
{
    return pageCountToken();
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
    const QVariantMap vars = m_report->variables();
    v = brMapValue(vars, name, &ok);
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
        return m_pageNumber;
    if (name == QLatin1String("pages") || name == QLatin1String("pagecount"))
        return pageCountToken();
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
    m_pageBottom = content.bottom();
    if (m_width <= 0 || content.height() <= 0) {
        if (error)
            *error = QStringLiteral("The page margins leave no printable area.");
        return false;
    }

    m_results = m_report->findChildren<BrDBResult *>();

    Frame *rf = push(m_report, m_report->dataSource(), true);
    rf->headersPrinted = true;
    resetResults(m_report);
    newPage();
    iterateRecords(*rf);
    finishPager(*rf);
    closePage();
    pop();

    m_doc->replaceText(pageCountToken(), QString::number(m_doc->pageCount()));

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
                ++(*it)->recNo;
            }
            return;
        }
    }
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
    resetResults(group);
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
    if (!ds || !ds->first())
        return;

    if (sub->pageBreak() == Br::PageBreak::Before && pageHasBody())
        newPage();

    Frame *f = push(sub, ds, true);
    f->recNo = 1;
    resetResults(sub);
    printSlot(sub, int(Br::BandType::Title), Mode::Flow);
    printSlot(sub, int(Br::BandType::Header), Mode::Flow);
    printSlot(sub, int(Br::BandType::ColumnHeader), Mode::Flow);
    f->headersPrinted = true;

    while (!ds->eof() && !aborted()) {
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

    for (int copy = 1; copy <= band->carbonCopies(); ++copy) {
        m_carbon = copy;
        if (!band->checkPrint(*this))
            continue;
        band->prepare(*this);
        double h = band->measureHeight(*this, m_width);

        if (mode == Mode::Flow) {
            bool broke = false;
            if (band->pageBreak() == Br::PageBreak::Before && pageHasBody()) {
                newPage();
                broke = true;
            }
            if (!broke && m_y + h > bottomLimit() + kEpsilon && pageHasBody()) {
                newPage();
                broke = true;
            }
            if (broke) { // page-dependent content (page numbers) must be recomputed
                band->prepare(*this);
                h = band->measureHeight(*this, m_width);
            }
            if (m_pendingAccumulate && band->bandType() == Br::BandType::Detail)
                accumulate();
        }

        const double y = fixedY ? *fixedY : m_y;
        band->render(*this, QRectF(m_left, y, m_width, h), m_doc->pages.last());
        if (fixedY)
            *fixedY += h;
        else
            m_y += h;
        band->callAfterPrint();
        if (mode == Mode::Flow && band->pageBreak() == Br::PageBreak::After)
            m_pendingBreak = true;
    }
    m_carbon = 1;
}

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

bool BrEngine::pageHasBody() const
{
    return m_y > m_bodyTop + kEpsilon;
}

double BrEngine::bandsHeight(BrPager *pager, int bandType) const
{
    double h = 0.0;
    for (BrBand *b : pager->bands(Br::BandType(bandType)))
        if (b->isVisible())
            h += b->height() * b->carbonCopies();
    return h;
}

double BrEngine::bottomLimit() const
{
    double limit = m_pageBottom - bandsHeight(m_report, int(Br::BandType::Footer));
    for (const auto &frame : m_stack) {
        if (frame->ending)
            continue;
        limit -= bandsHeight(frame->pager, int(Br::BandType::ColumnFooter));
        if (!frame->isReport)
            limit -= bandsHeight(frame->pager, int(Br::BandType::Footer));
    }
    return limit;
}

void BrEngine::newPage()
{
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
    m_pageNumber = m_doc->pageCount();
    m_hasPage = true;
    m_pendingBreak = false;
    m_y = m_report->contentRect().top();

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
            if (!frame->ending)
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
    // Column footers / group footers right after the content, innermost first.
    for (auto it = m_stack.rbegin(); it != m_stack.rend(); ++it) {
        Frame &f = **it;
        if (f.ending && !f.isReport)
            continue;
        if (!f.columnFooterPrinted) {
            printSlot(f.pager, int(Br::BandType::ColumnFooter), Mode::Fixed);
            f.columnFooterPrinted = true;
        }
        if (!f.isReport && !f.footerPrinted) {
            printSlot(f.pager, int(Br::BandType::Footer), Mode::Fixed);
            f.footerPrinted = true;
        }
    }
    // Report footer: anchored to the bottom of the printable area.
    double y = m_pageBottom - bandsHeight(m_report, int(Br::BandType::Footer));
    for (BrBand *b : m_report->bands(Br::BandType::Footer))
        printBand(b, Mode::Fixed, &y);
    m_hasPage = true;
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

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
