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

#ifndef BRREPORT_H
#define BRREPORT_H

#include "brdocument.h"
#include "brpager.h"

#include <QDateTime>
#include <QFont>
#include <QHash>
#include <QLocale>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QVariantMap>

#include <functional>

class BrDataSource;

/*!
 * The report: page setup, main data source and the bands that make
 * the layout. Calling prepare() runs the print flow and produces a BrDocument
 * that can be previewed, printed or exported.
 *
 * \code
 * BrReport report;
 * report.setTitle("Customers");
 * report.setDataSource(&customers);
 *
 * auto *header = report.addBand(Br::BandType::Header, 12);
 * new BrSystemInfo(Br::InfoType::Title, header);
 *
 * auto *detail = report.addBand(Br::BandType::Detail, 6);
 * auto *name = new BrDBText("name", detail);
 * name->setGeometry(0, 0, 80, 6);
 *
 * report.exportToPdf("customers.pdf");
 * \endcode
 */
class BRREPORT_EXPORT BrReport : public BrPager
{
    Q_OBJECT
public:
    using CustomFunction = std::function<QVariant(const QVariantList &args)>;
    using PageHandler = std::function<void(int pageNumber)>;

    explicit BrReport(QObject *parent = nullptr);
    ~BrReport() override;

    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; }

    // -- Page setup (mm) --
    QPageSize pageSize() const { return m_pageSize; }
    void setPageSize(const QPageSize &size) { m_pageSize = size; }
    void setPageSize(QPageSize::PageSizeId id) { m_pageSize = QPageSize(id); }
    /*! Custom paper size in millimeters (portrait dimensions). */
    void setPageSizeMm(double width, double height);

    QPageLayout::Orientation orientation() const { return m_orientation; }
    void setOrientation(QPageLayout::Orientation orientation) { m_orientation = orientation; }

    QMarginsF margins() const { return m_margins; }
    void setMargins(const QMarginsF &mm) { m_margins = mm; }
    void setMargins(double left, double top, double right, double bottom)
    {
        m_margins = QMarginsF(left, top, right, bottom);
    }

    /*! Page size in mm, already oriented. */
    QSizeF pageSizeMm() const;
    /*! Printable area (page minus margins), in mm. */
    QRectF contentRect() const;

    /*! Default font for all controls (default: Arial 10pt). */
    QFont font() const { return m_font; }
    void setFont(const QFont &font) { m_font = font; }

    /*! Locale used to format numbers and dates (default: QLocale()). */
    QLocale locale() const { return m_locale; }
    void setLocale(const QLocale &locale) { m_locale = locale; }

    /*! Date/time shown by BrSystemInfo; if invalid, the time of prepare() is used. */
    QDateTime reportDateTime() const { return m_dateTime; }
    void setReportDateTime(const QDateTime &dateTime) { m_dateTime = dateTime; }

    /*! Main data source. Without one, the detail bands print exactly once. */
    BrDataSource *dataSource() const { return m_dataSource; }
    void setDataSource(BrDataSource *dataSource) { m_dataSource = dataSource; }

    /*! Values available to expressions by name (checked after data fields). */
    void setVariable(const QString &name, const QVariant &value);
    QVariant variable(const QString &name) const;
    QVariantMap variables() const { return m_variables; }

    /*! Registers a function callable from expressions (overrides built-ins). */
    void registerFunction(const QString &name, const CustomFunction &function);
    CustomFunction function(const QString &name) const;

    /*! Safety limit against runaway layouts (default 10000 pages). */
    int maxPages() const { return m_maxPages; }
    void setMaxPages(int pages) { m_maxPages = pages; }

    /*! Called after each new page is started while preparing. */
    void setPageHandler(const PageHandler &handler) { m_pageHandler = handler; }

    // -- Running --

    /*! Runs the report and builds the document. Returns false on error. */
    bool prepare();
    bool isPrepared() const { return m_prepared; }
    const BrDocument &document() const { return m_document; }
    /*! Discards the prepared document. */
    void clearDocument();

    /*! Requests to stop preparing (e.g. from a page handler). */
    void cancel() { m_canceled = true; }
    bool isCanceled() const { return m_canceled; }

    QString errorString() const { return m_error; }

    // -- Export shortcuts (prepare() is called when needed) --
    bool exportToPdf(const QString &fileName);
    bool exportToHtml(const QString &fileName);
    QStringList exportToImages(const QString &filePattern, double dpi = 150.0);
    bool saveDocument(const QString &fileName);

    BrReport *report() const override { return const_cast<BrReport *>(this); }

private:
    friend class BrEngine;
    bool ensurePrepared();

    QString m_title;
    QPageSize m_pageSize = QPageSize(QPageSize::A4);
    QPageLayout::Orientation m_orientation = QPageLayout::Portrait;
    QMarginsF m_margins = QMarginsF(10, 10, 10, 10);
    QFont m_font;
    QLocale m_locale;
    QDateTime m_dateTime;
    BrDataSource *m_dataSource = nullptr;
    QVariantMap m_variables;
    QHash<QString, CustomFunction> m_functions;
    int m_maxPages = 10000;
    PageHandler m_pageHandler;

    BrDocument m_document;
    bool m_prepared = false;
    bool m_canceled = false;
    QString m_error;
};

#endif // BRREPORT_H
