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

#ifndef BRCHART_H
#define BRCHART_H

#include "brcontrol.h"

#include <QStringList>
#include <QVariantList>

class BrDataSource;

/*!
 * Chart control: pie, vertical bars, horizontal bars and lines, drawn as vector
 * graphics (sharp in PDF and print).
 *
 * Data can be given directly (setLabels() + addSeries()) or read from a data
 * source when the chart prints (setDataSource()). Use a data source that is not
 * the one being iterated by the report/sub-detail where the chart is placed.
 *
 * \code
 * auto *chart = new BrChart(Br::ChartType::Bar, band);
 * chart->setGeometry(0, 0, 190, 80);
 * chart->setTitle("Sales by month");
 * chart->setLabels({"Jan", "Feb", "Mar"});
 * chart->addSeries("2025", {120, 98, 143});
 * chart->addSeries("2026", {131, 120, 160});
 * \endcode
 */
class BRREPORT_EXPORT BrChart : public BrControl
{
    Q_OBJECT
    Q_PROPERTY(Br::ChartType chartType READ chartType WRITE setChartType)
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QStringList labels READ labels WRITE setLabels)
    Q_PROPERTY(QVariantList seriesData READ seriesData WRITE setSeriesData)
    Q_PROPERTY(QString dataSourceName READ dataSourceName WRITE setDataSourceName)
    Q_PROPERTY(QString labelField READ labelField WRITE setLabelField)
    Q_PROPERTY(QStringList valueFields READ valueFields WRITE setValueFields)
    Q_PROPERTY(bool showLegend READ showLegend WRITE setShowLegend)
    Q_PROPERTY(bool showValues READ showValues WRITE setShowValues)
    Q_PROPERTY(QString valueFormat READ valueFormat WRITE setValueFormat)
public:
    struct Series
    {
        QString name;
        QList<double> values;
        QColor color; //!< Invalid = taken from the palette.
    };

    explicit BrChart(BrContainer *parent = nullptr);
    BrChart(Br::ChartType type, BrContainer *parent);

    Br::ChartType chartType() const { return m_type; }
    void setChartType(Br::ChartType type) { m_type = type; }

    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; }

    // -- Static data --
    QStringList labels() const { return m_labels; }
    void setLabels(const QStringList &labels) { m_labels = labels; }
    void addSeries(const QString &name, const QList<double> &values, const QColor &color = QColor());
    QList<Series> series() const { return m_series; }
    void clearSeries() { m_series.clear(); }

    // -- Data from a data source (read every time the chart prints) --
    /*!
     * Each record becomes a label (\a labelField) with one value per series
     * (\a valueFields; the field names are the series names).
     */
    void setDataSource(BrDataSource *dataSource, const QString &labelField,
                       const QStringList &valueFields);

    /*! Data source registered in the report under this name (used by report files). */
    QString dataSourceName() const { return m_dataSourceName; }
    void setDataSourceName(const QString &name) { m_dataSourceName = name; }
    QString labelField() const { return m_labelField; }
    void setLabelField(const QString &field) { m_labelField = field; }
    QStringList valueFields() const { return m_valueFields; }
    void setValueFields(const QStringList &fields) { m_valueFields = fields; }

    /*! Static series as a list of maps {name, values, color} (used by report files). */
    QVariantList seriesData() const;
    void setSeriesData(const QVariantList &data);

    // -- Appearance --
    bool showLegend() const { return m_showLegend; }
    void setShowLegend(bool show) { m_showLegend = show; }

    /*! Prints the values on bars/points (percentages on pie slices). */
    bool showValues() const { return m_showValues; }
    void setShowValues(bool show) { m_showValues = show; }

    /*! Number mask for values and axis labels (see BrFormat), e.g. "#,##0". */
    QString valueFormat() const { return m_valueFormat; }
    void setValueFormat(const QString &mask) { m_valueFormat = mask; }

    /*! Colors used for series (pie: slices) without an explicit color. */
    QList<QColor> palette() const { return m_palette; }
    void setPalette(const QList<QColor> &colors) { m_palette = colors; }

    void prepare(BrRenderContext &ctx) override;
    void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page) override;

private:
    QColor colorAt(int index) const;
    void renderPie(BrRenderContext &ctx, const QRectF &area, BrPage &page);
    void renderBars(BrRenderContext &ctx, const QRectF &area, BrPage &page, bool horizontal);
    void renderLines(BrRenderContext &ctx, const QRectF &area, BrPage &page);
    QRectF renderLegend(const QRectF &area, const QStringList &names, BrPage &page);

    Br::ChartType m_type = Br::ChartType::Bar;
    QString m_title;
    QStringList m_labels;
    QList<Series> m_series;
    BrDataSource *m_dataSource = nullptr;
    QString m_dataSourceName;
    QString m_labelField;
    QStringList m_valueFields;
    bool m_showLegend = true;
    bool m_showValues = false;
    QString m_valueFormat;
    QList<QColor> m_palette;

    // Data used for the current print
    QStringList m_curLabels;
    QList<Series> m_curSeries;
};

#endif // BRCHART_H
