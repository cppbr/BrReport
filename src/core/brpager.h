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

#ifndef BRPAGER_H
#define BRPAGER_H

#include "brglobal.h"

#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariant>

#include <functional>

class BrBand;
class BrDataSource;
class BrGroup;
class BrRenderContext;
class BrReport;

/*!
 * Base class of objects that own bands and control the print flow
 * (BrReport, BrGroup and BrSubDetail).
 *
 * The order of creation matters: detail bands and sub-details print in the
 * order they were created.
 */
class BRREPORT_EXPORT BrPager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Br::PageBreak pageBreak READ pageBreak WRITE setPageBreak)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(bool keepHeaderWithDetail READ keepHeaderWithDetail WRITE setKeepHeaderWithDetail)
    Q_PROPERTY(bool resetPageNumber READ resetPageNumber WRITE setResetPageNumber)
public:
    explicit BrPager(BrPager *parent = nullptr);
    ~BrPager() override;

    /*! Creates a band owned by this pager. */
    BrBand *addBand(Br::BandType type, double height = 8.0);

    /*! All bands, in creation order. */
    QList<BrBand *> bands() const;
    QList<BrBand *> bands(Br::BandType type) const;

    /*! The first child group (records are delegated to it), or nullptr. */
    BrGroup *group() const;

    BrPager *parentPager() const;
    virtual BrReport *report() const;

    Br::PageBreak pageBreak() const { return m_pageBreak; }
    void setPageBreak(Br::PageBreak pageBreak) { m_pageBreak = pageBreak; }

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    /*!
     * Groups and sub-details: never leave the Title/Header/ColumnHeader bands alone
     * at the bottom of a page. If they do not fit together with the first detail
     * band, a new page is started first (default false).
     */
    bool keepHeaderWithDetail() const { return m_keepHeaderWithDetail; }
    void setKeepHeaderWithDetail(bool on) { m_keepHeaderWithDetail = on; }

    /*!
     * Groups and sub-details: page numbers restart at 1 when the pager starts, and
     * the page count ("page 1 of 3") covers only its pages. Use it with
     * setPageBreak(Br::PageBreak::Before) (or a page break in the band that
     * precedes it) so that every section starts on a new page. Typical use: one
     * invoice per group. BrSystemInfo's DocumentPageNumber/DocumentPageCount keep
     * numbering the whole document.
     */
    bool resetPageNumber() const { return m_resetPageNumber; }
    void setResetPageNumber(bool on) { m_resetPageNumber = on; }

private:
    Br::PageBreak m_pageBreak = Br::PageBreak::None;
    bool m_resetPageNumber = false;
    bool m_enabled = true;
    bool m_keepHeaderWithDetail = false;
};

/*!
 * Groups consecutive records with the same key.
 *
 * The data must be sorted by the key. The group prints its Title and Header
 * bands when it starts, its Detail bands for every record and its
 * ColumnFooter/Summary/Footer bands when the key changes. Groups can be nested:
 * create a BrGroup inside another BrGroup.
 *
 * Header and ColumnHeader bands are repeated after page breaks; use the Title
 * band for content that must print only once per group.
 */
class BRREPORT_EXPORT BrGroup : public BrPager
{
    Q_OBJECT
    Q_PROPERTY(QString dataFields READ dataFields WRITE setDataFields)
    Q_PROPERTY(QString dataFormula READ dataFormula WRITE setDataFormula)
public:
    using KeyFunction = std::function<QVariant(BrRenderContext &ctx)>;

    explicit BrGroup(BrPager *parent);

    /*! Fields that form the key, separated by ';'. */
    QString dataFields() const { return m_dataFields; }
    void setDataFields(const QString &fields);

    /*! Expression that forms the key. */
    QString dataFormula() const { return m_dataFormula; }
    void setDataFormula(const QString &expression);

    /*! Custom key function (overrides fields and formula). */
    void setKeyFunction(const KeyFunction &function) { m_keyFunction = function; }

    /*! Key of the current record. */
    QString key(BrRenderContext &ctx) const;

private:
    QString m_dataFields;
    QString m_dataFormula;
    KeyFunction m_keyFunction;
};

/*!
 * Prints the records of a second data source inside a pager,
 * typically for master/detail relations: the report iterates the masters and
 * the sub-detail iterates the children of each master.
 *
 * Use setBeforeOpen() to filter or re-query the detail data for the current
 * master record before it is iterated.
 */
class BRREPORT_EXPORT BrSubDetail : public BrPager
{
    Q_OBJECT
    Q_PROPERTY(QString dataSourceName READ dataSourceName WRITE setDataSourceName)
    Q_PROPERTY(Br::BandType positioning READ positioning WRITE setPositioning)
    Q_PROPERTY(QString masterFields READ masterFields WRITE setMasterFields)
    Q_PROPERTY(QString detailFields READ detailFields WRITE setDetailFields)
public:
    using BeforeOpenHandler = std::function<void(BrSubDetail &subDetail, BrRenderContext &ctx)>;

    explicit BrSubDetail(BrPager *parent, BrDataSource *dataSource = nullptr);

    /*! The data source: the one set with setDataSource(), or the report's data source named dataSourceName(). */
    BrDataSource *dataSource() const;
    void setDataSource(BrDataSource *dataSource) { m_dataSource = dataSource; }

    /*! Name of a data source registered with BrReport::addDataSource() (used by report files). */
    QString dataSourceName() const { return m_dataSourceName; }
    void setDataSourceName(const QString &name) { m_dataSourceName = name; }

    /*!
     * Declarative master/detail link: only the records whose detailFields() values
     * equal the masterFields() values of the current master record are printed.
     * Fields are separated by ';' and matched by position. Example:
     * setMasterFields("id"); setDetailFields("order_id");
     */
    QString masterFields() const { return m_masterFields; }
    void setMasterFields(const QString &fields) { m_masterFields = fields; }
    QString detailFields() const { return m_detailFields; }
    void setDetailFields(const QString &fields) { m_detailFields = fields; }

    /*! Which band slot of the parent the sub-detail occupies (default: Detail). */
    Br::BandType positioning() const { return m_positioning; }
    void setPositioning(Br::BandType type) { m_positioning = type; }

    void setBeforeOpen(const BeforeOpenHandler &handler) { m_beforeOpen = handler; }
    void callBeforeOpen(BrRenderContext &ctx);

private:
    BrDataSource *m_dataSource = nullptr;
    QString m_dataSourceName;
    QString m_masterFields;
    QString m_detailFields;
    Br::BandType m_positioning = Br::BandType::Detail;
    BeforeOpenHandler m_beforeOpen;
};

#endif // BRPAGER_H
