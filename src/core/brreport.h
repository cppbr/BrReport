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
#include <QImage>
#include <QLocale>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QSizeF>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class BrDataSource;

/*! Text and/or image printed behind the content of every page. */
struct BRREPORT_EXPORT BrWatermark
{
    QString text;                                             //!< e.g. "DRAFT", "CANCELED"
    QFont font = QFont(QStringLiteral("Arial"), 80, QFont::Bold);
    QColor color = QColor(0, 0, 0, 38);                       //!< Use alpha for transparency.
    double angle = 45.0;                                      //!< Counter-clockwise degrees.
    QImage image;                                             //!< Centered in the printable area.
    double imageOpacity = 0.15;                               //!< 0..1

    bool isEnabled() const { return !text.isEmpty() || !image.isNull(); }
};

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
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QSizeF paperSize READ paperSize WRITE setPaperSize)
    Q_PROPERTY(bool landscape READ isLandscape WRITE setLandscape)
    Q_PROPERTY(QMarginsF margins READ margins WRITE setMargins)
    Q_PROPERTY(bool continuous READ isContinuous WRITE setContinuous)
    Q_PROPERTY(QFont font READ font WRITE setFont)
    Q_PROPERTY(QString localeName READ localeName WRITE setLocaleName)
    Q_PROPERTY(QString dataSourceName READ dataSourceName WRITE setDataSourceName)
    Q_PROPERTY(QVariantMap variables READ variables WRITE setVariables)
    Q_PROPERTY(QVariantList queries READ queries WRITE setQueries)
    Q_PROPERTY(QVariantList parameters READ parameters WRITE setParameters)
    Q_PROPERTY(int maxPages READ maxPages WRITE setMaxPages)
    Q_PROPERTY(QString watermarkText READ watermarkText WRITE setWatermarkText)
    Q_PROPERTY(QFont watermarkFont READ watermarkFont WRITE setWatermarkFont)
    Q_PROPERTY(QColor watermarkColor READ watermarkColor WRITE setWatermarkColor)
    Q_PROPERTY(double watermarkAngle READ watermarkAngle WRITE setWatermarkAngle)
    Q_PROPERTY(QImage watermarkImage READ watermarkImage WRITE setWatermarkImage)
    Q_PROPERTY(double watermarkImageOpacity READ watermarkImageOpacity WRITE setWatermarkImageOpacity)
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

    /*!
     * Continuous page: everything is printed on a single page whose height grows
     * with the content (receipts, thermal/roll printers). Page breaks are ignored
     * and the Footer follows the content. The page width comes from pageSize().
     */
    bool isContinuous() const { return m_continuous; }
    void setContinuous(bool on) { m_continuous = on; }

    /*! Watermark printed behind the content of every page. */
    BrWatermark watermark() const { return m_watermark; }
    void setWatermark(const BrWatermark &watermark) { m_watermark = watermark; }
    /*! Convenience: text watermark with the default style. */
    void setWatermark(const QString &text)
    {
        m_watermark = BrWatermark();
        m_watermark.text = text;
    }

    /*! Paper size in mm, portrait dimensions (width <= height for standard sizes). */
    QSizeF paperSize() const { return m_pageSize.size(QPageSize::Millimeter); }
    void setPaperSize(const QSizeF &mm);
    bool isLandscape() const { return m_orientation == QPageLayout::Landscape; }
    void setLandscape(bool on) { m_orientation = on ? QPageLayout::Landscape : QPageLayout::Portrait; }

    QString watermarkText() const { return m_watermark.text; }
    void setWatermarkText(const QString &text) { m_watermark.text = text; }
    QFont watermarkFont() const { return m_watermark.font; }
    void setWatermarkFont(const QFont &font) { m_watermark.font = font; }
    QColor watermarkColor() const { return m_watermark.color; }
    void setWatermarkColor(const QColor &color) { m_watermark.color = color; }
    double watermarkAngle() const { return m_watermark.angle; }
    void setWatermarkAngle(double degrees) { m_watermark.angle = degrees; }
    QImage watermarkImage() const { return m_watermark.image; }
    void setWatermarkImage(const QImage &image) { m_watermark.image = image; }
    double watermarkImageOpacity() const { return m_watermark.imageOpacity; }
    void setWatermarkImageOpacity(double opacity) { m_watermark.imageOpacity = opacity; }

    /*! Page size in mm, already oriented. */
    QSizeF pageSizeMm() const;
    /*! Printable area (page minus margins), in mm. */
    QRectF contentRect() const;

    /*! Default font for all controls (default: Arial 10pt). */
    QFont font() const { return m_font; }
    void setFont(const QFont &font) { m_font = font; }

    /*! Locale used to format numbers and dates (default: QLocale()). */
    QLocale locale() const { return m_locale; }
    void setLocale(const QLocale &locale)
    {
        m_locale = locale;
        m_localeName = locale.name();
    }
    /*! Locale as a name ("pt_BR", "en_US"); empty = system locale. */
    QString localeName() const { return m_localeName; }
    void setLocaleName(const QString &name);

    /*! Date/time shown by BrSystemInfo; if invalid, the time of prepare() is used. */
    QDateTime reportDateTime() const { return m_dateTime; }
    void setReportDateTime(const QDateTime &dateTime) { m_dateTime = dateTime; }

    /*!
     * Main data source: the one set with setDataSource(), or the registered data
     * source named dataSourceName(). Without one, the detail bands print exactly once.
     */
    BrDataSource *dataSource() const;
    void setDataSource(BrDataSource *dataSource) { m_dataSource = dataSource; }
    /*! Registers \a dataSource as the main data source under \a name. */
    void setDataSource(const QString &name, BrDataSource *dataSource);

    QString dataSourceName() const { return m_dataSourceName; }
    void setDataSourceName(const QString &name) { m_dataSourceName = name; }

    /*!
     * Registers a data source by name, so that report files (and sub-details,
     * charts and qualified fields "name.field") can refer to it. The data source's
     * objectName() is set to \a name. Not owned by the report.
     */
    void addDataSource(const QString &name, BrDataSource *dataSource);
    BrDataSource *findDataSource(const QString &name) const;
    QStringList dataSourceNames() const;

    /*!
     * SQL queries stored in the report (report files), as a list of maps
     * {"name", "sql", "connection"}. Before printing, each query runs on the Qt SQL
     * connection with that name (empty = default connection) and becomes a data
     * source with the query name. Placeholders ":name" are bound to report
     * variables with the same name. A data source registered by the application
     * with the same name takes precedence. Passwords are never stored in the
     * report: the application opens the connections.
     */
    QVariantList queries() const { return m_queries; }
    void setQueries(const QVariantList &queries) { m_queries = queries; }
    void addQuery(const QString &name, const QString &sql, const QString &connection = QString());

    /*! Runs the queries (prepare() does it). Returns false and sets errorString() on failure. */
    bool openQueries();

    /*!
     * Parameters declared by the report, as a list of maps {"name", "caption",
     * "type", "default", "choices"}. BrPrinting::preview() and print() ask the user
     * for them (BrParametersDialog) and store the answers as report variables, so
     * they work in SQL queries (":name") and formulas (name).
     *
     * - type: "string" (default), "integer", "number", "date" or "boolean".
     * - default: value used until a variable with the same name is set. Dates are
     *   "yyyy-MM-dd" or "today"; a date without a default is today.
     * - choices: fixed list of values, each a plain value or a map {"value", "caption"}.
     * - choicesSql, choicesConnection: query that fills the list instead of "choices".
     *   The first column is the value and the second, optional, the caption.
     *   Placeholders ":name" are bound like those of queries().
     */
    QVariantList parameters() const { return m_parameters; }
    void setParameters(const QVariantList &parameters) { m_parameters = parameters; }
    void addParameter(const QString &name, const QString &caption = QString(),
                      const QString &type = QStringLiteral("string"),
                      const QVariant &defaultValue = QVariant(),
                      const QVariantList &choices = QVariantList());
    /*!
     * The choices of \a parameter, running its query if it has one. On failure
     * returns an empty list and sets \a error.
     */
    QVariantList parameterChoices(const QVariantMap &parameter, QString *error = nullptr) const;
    /*! Converts \a value to the type of \a parameter (an item of parameters()). */
    static QVariant parameterValue(const QVariantMap &parameter, const QVariant &value);

    // -- Report files (layout as JSON, see docs/report-file-format.md) --

    /*! Replaces the layout with the one in \a fileName (.brt). Data sources are kept. */
    bool load(const QString &fileName);
    /*! Saves the layout (not the data) to \a fileName. */
    bool save(const QString &fileName) const;
    bool loadFromJson(const QByteArray &json);
    QByteArray toJson() const;

    /*! Finds an object of the layout by name (e.g. to attach handlers after load()). */
    template<class T = QObject>
    T *object(const QString &name) const
    {
        return findChild<T *>(name);
    }

    /*! Removes every band, group and sub-detail (the layout). */
    void clearLayout();

    /*!
     * Values available to expressions by name (checked after data fields).
     * variable() also returns the value of a declared parameter, converted to its
     * type: the variable with the same name if set, otherwise the default value.
     */
    void setVariable(const QString &name, const QVariant &value);
    QVariant variable(const QString &name, bool *found = nullptr) const;
    QVariantMap variables() const { return m_variables; }
    void setVariables(const QVariantMap &variables) { m_variables = variables; }

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
    bool m_continuous = false;
    BrWatermark m_watermark;
    QFont m_font;
    QLocale m_locale;
    QDateTime m_dateTime;
    BrDataSource *m_dataSource = nullptr;
    QString m_dataSourceName;
    QHash<QString, BrDataSource *> m_dataSources;
    QHash<QString, QString> m_dataSourceLabels; // lower-case key -> name as registered
    QVariantList m_queries;
    QVariantList m_parameters;
    QHash<QString, BrDataSource *> m_queryDataSources; // owned, created from m_queries
    QString m_localeName;
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
