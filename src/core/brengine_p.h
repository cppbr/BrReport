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

#ifndef BRENGINE_P_H
#define BRENGINE_P_H

//
//  W A R N I N G
//  -------------
// This file is not part of the BrReport API. It may change without notice.
//

#include "brdocument.h"
#include "brrendercontext.h"

#include <QHash>
#include <QVector>

#include <memory>
#include <vector>

class BrBand;
class BrDBResult;
class BrGroup;
class BrPager;
class BrSubDetail;

/*!
 * Layout engine: walks the pagers and bands of a report, breaks pages and emits
 * the prepared document. It is also the BrRenderContext seen by controls.
 */
class BrEngine : public BrRenderContext
{
public:
    explicit BrEngine(BrReport *report);
    ~BrEngine() override;

    bool run(BrDocument *document, QString *error);

    // BrRenderContext
    BrReport *report() const override { return m_report; }
    int pageNumber() const override;
    int documentPageNumber() const override { return m_pageNumber; }
    QString pageNumberText() const override;
    QString pageCountText() const override;
    QString documentPageCountText() const override;
    int recNo() const override;
    int detailCount() const override { return m_detailCount; }
    int carbonCopy() const override { return m_carbon; }
    void keepTogether(const QRectF &rect) override
    {
        if (m_keepAreas)
            *m_keepAreas << rect;
    }
    QVariant fieldValue(const QString &name, bool *found = nullptr) const override;
    QDateTime reportDateTime() const override { return m_dateTime; }

    // BrExpressionContext
    QVariant variable(const QString &name, bool *found) const override;
    QVariant function(const QString &name, const QVariantList &args, bool *found) const override;
    QLocale locale() const override;

    static QString pageNumberToken();
    static QString pageCountToken();
    static QString documentPageCountToken();

private:
    struct Frame
    {
        BrPager *pager = nullptr;
        BrDataSource *dataSource = nullptr;
        bool ownsDataSource = false; //!< report / sub-detail (iterates its own records)
        bool isReport = false;
        bool headersPrinted = false;
        bool ending = false;
        bool columnFooterPrinted = false;
        bool footerPrinted = false;
        QString key;
        int recNo = 0;
        QStringList linkFields; //!< detail fields of a master/detail link
        QStringList linkValues; //!< master values they must match
    };

    enum class Mode {
        Flow, //!< Normal flow: may break pages.
        Fixed //!< Page decoration (headers/footers): never breaks pages.
    };

    Frame &top() { return *m_stack.back(); }
    Frame *push(BrPager *pager, BrDataSource *ds, bool ownsDs);
    void pop();
    bool aborted() const;

    void iterateRecords(Frame &frame);
    void runGroup(BrGroup *group);
    void runSubDetail(BrSubDetail *subDetail);
    void finishPager(Frame &frame);
    void printRecord();
    void advance();
    static bool linkMatches(BrDataSource *ds, const QStringList &fields, const QStringList &values);
    static void skipUnlinked(BrDataSource *ds, const QStringList &fields, const QStringList &values);
    bool groupBroken();

    void printSlot(BrPager *pager, int bandType, Mode mode);
    void printBand(BrBand *band, Mode mode, double *fixedY = nullptr);
    void printSplit(BrBand *band, double x, double width, double height);

    void newPage();
    void closePage();
    double bandsHeight(BrPager *pager, int bandType, bool onlyThisPage = false) const;
    double bottomLimit() const;
    bool pageHasBody() const;

    void resetPager(BrPager *pager);
    void resetResults(BrPager *pager);
    bool bandPrintsOnThisPage(BrBand *band) const;
    void keepHeaderWithDetail(BrPager *pager);
    void drawWatermark();
    void startNumberingSection();
    void resolvePageNumbers();
    void accumulate();
    BrDataSource *currentDataSource() const;
    static BrPager *dataOwnerOf(BrPager *pager);

    BrReport *m_report;
    BrDocument *m_doc = nullptr;
    std::vector<std::unique_ptr<Frame>> m_stack;
    QList<BrDBResult *> m_results;
    QDateTime m_dateTime;

    int m_pageNumber = 0;
    double m_y = 0.0;
    double m_bodyTop = 0.0;
    double m_left = 0.0;
    double m_width = 0.0;
    double m_pageBottom = 0.0;
    bool m_hasPage = false;
    bool m_pendingBreak = false;
    bool m_pendingAccumulate = false;
    bool m_abort = false;
    bool m_lastPage = false;
    QVector<int> m_pageSection; // numbering section of every page
    int m_section = 0;
    int m_sectionCount = 1;
    QHash<BrBand *, int> m_zebra;
    // Multi-column detail state
    BrBand *m_columnBand = nullptr;
    int m_column = 0;
    double m_rowTop = 0.0;
    double m_rowHeight = 0.0;
    int m_detailCount = 0;
    int m_carbon = 1;
    QList<QRectF> *m_keepAreas = nullptr; // areas that printSplit() must not cut
    QString m_error;
};

#endif // BRENGINE_P_H
