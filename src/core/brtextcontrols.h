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

#ifndef BRTEXTCONTROLS_H
#define BRTEXTCONTROLS_H

#include "brcontrol.h"
#include "brexpression.h"

#include <QStringList>
#include <QVariant>

class BrPager;

/*!
 * Base class for controls that print text.
 */
class BRREPORT_EXPORT BrCustomText : public BrControl
{
    Q_OBJECT
public:
    explicit BrCustomText(BrContainer *parent = nullptr);

    Br::TextAlign alignment() const { return m_alignment; }
    void setAlignment(Br::TextAlign alignment) { m_alignment = alignment; }

    Br::TextLayout layout() const { return m_layout; }
    void setLayout(Br::TextLayout layout) { m_layout = layout; }

    /*! Breaks lines at word boundaries to fit the width. */
    bool wordWrap() const { return m_wordWrap; }
    void setWordWrap(bool on) { m_wordWrap = on; }

    /*! Grows the height to fit all lines (and expands the band when allowed). */
    bool autoStretch() const { return m_autoStretch; }
    void setAutoStretch(bool on) { m_autoStretch = on; }

    /*! Counter-clockwise rotation in degrees. Disables wrapping. */
    double angle() const { return m_angle; }
    void setAngle(double degrees) { m_angle = degrees; }

    /*! Inner spacing between the frame and the text, in mm. */
    double padding() const { return m_padding; }
    void setPadding(double mm) { m_padding = mm; }

    /*! Does not print anything (not even background/borders) when the text is empty. */
    bool hideIfEmpty() const { return m_hideIfEmpty; }
    void setHideIfEmpty(bool on) { m_hideIfEmpty = on; }

    /*! Reduces the font size (down to 4 pt) until the text fits the control. */
    bool shrinkToFit() const { return m_shrinkToFit; }
    void setShrinkToFit(bool on) { m_shrinkToFit = on; }

    /*!
     * Interprets the text as HTML: <b>, <i>, <u>, <font color=...>, <br>,
     * <span style="...">, lists and simple tables. The control font and color are
     * the defaults. Rich text always wraps.
     */
    bool allowHtml() const { return m_allowHtml; }
    void setAllowHtml(bool on) { m_allowHtml = on; }

    /*! Text computed for the current print (after prepare()). */
    QString currentText() const { return m_currentText; }

    void prepare(BrRenderContext &ctx) override;
    double measureHeight(BrRenderContext &ctx, double width) override;
    void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page) override;

protected:
    /*! Returns the text to print for the current record/page. */
    virtual QString computeText(BrRenderContext &ctx) = 0;
    void setCurrentText(const QString &text) { m_currentText = text; }

private:
    Br::TextAlign m_alignment = Br::TextAlign::Left;
    Br::TextLayout m_layout = Br::TextLayout::Center;
    bool m_wordWrap = false;
    bool m_autoStretch = false;
    double m_angle = 0.0;
    double m_padding = 0.0;
    bool m_hideIfEmpty = false;
    bool m_shrinkToFit = false;
    bool m_allowHtml = false;
    QString m_currentText;

    QFont fittingFont(const QRectF &inner) const;
};

/*! Static text. */
class BRREPORT_EXPORT BrLabel : public BrCustomText
{
    Q_OBJECT
public:
    explicit BrLabel(BrContainer *parent = nullptr);
    BrLabel(const QString &text, BrContainer *parent);

    QString text() const { return m_text; }
    void setText(const QString &text) { m_text = text; }

protected:
    QString computeText(BrRenderContext &ctx) override;

private:
    QString m_text;
};

/*! Multi-line static text, word wrapped and auto-stretched. */
class BRREPORT_EXPORT BrMemo : public BrLabel
{
    Q_OBJECT
public:
    explicit BrMemo(BrContainer *parent = nullptr);

    void setLines(const QStringList &lines) { setText(lines.join(QLatin1Char('\n'))); }
    QStringList lines() const { return text().split(QLatin1Char('\n')); }
};

/*!
 * Text from a data field or an expression.
 *
 * Use setDataField() for a plain field or setDataFormula() for an expression
 * (see BrExpression), e.g. "Quantity * Price" or "upper(Name)".
 */
class BRREPORT_EXPORT BrDBText : public BrCustomText
{
    Q_OBJECT
public:
    explicit BrDBText(BrContainer *parent = nullptr);
    BrDBText(const QString &dataField, BrContainer *parent);

    QString dataField() const { return m_dataField; }
    void setDataField(const QString &field);

    QString dataFormula() const { return m_formula.source(); }
    void setDataFormula(const QString &expression);

    /*! See BrFormat::format(), e.g. "#,##0.00" or "dd/MM/yyyy". */
    QString displayFormat() const { return m_displayFormat; }
    void setDisplayFormat(const QString &format) { m_displayFormat = format; }

    /*! Text printed before/after the value (only when the value is not empty). */
    void setPrefix(const QString &text) { m_prefix = text; }
    QString prefix() const { return m_prefix; }
    void setSuffix(const QString &text) { m_suffix = text; }
    QString suffix() const { return m_suffix; }

    /*! Value of the field/formula for the current print. */
    QVariant currentValue() const { return m_value; }

protected:
    QString computeText(BrRenderContext &ctx) override;
    QString formatValue(const QVariant &value, BrRenderContext &ctx) const;
    /*! Evaluates the field or formula. */
    QVariant fetchValue(BrRenderContext &ctx) const;

private:
    QString m_dataField;
    BrExpression m_formula;
    QString m_displayFormat;
    QString m_prefix;
    QString m_suffix;
    QVariant m_value;
};

/*! Multi-line data text, word wrapped and auto-stretched. */
class BRREPORT_EXPORT BrDBMemo : public BrDBText
{
    Q_OBJECT
public:
    explicit BrDBMemo(BrContainer *parent = nullptr);
    BrDBMemo(const QString &dataField, BrContainer *parent);
};

/*!
 * Aggregated value of a field/formula: sum, count, average, ...
 *
 * Values are accumulated for every record processed by the pager (report,
 * group or sub-detail) that owns the band, and reset when that pager starts.
 * With resetAfterPrint() the value is also reset after each print, which gives
 * per-page totals when the control is placed in the report footer.
 */
class BRREPORT_EXPORT BrDBResult : public BrDBText
{
    Q_OBJECT
public:
    explicit BrDBResult(BrContainer *parent = nullptr);
    BrDBResult(const QString &dataField, Br::ResultType type, BrContainer *parent);

    Br::ResultType resultType() const { return m_type; }
    void setResultType(Br::ResultType type) { m_type = type; }

    bool resetAfterPrint() const { return m_resetAfterPrint; }
    void setResetAfterPrint(bool on) { m_resetAfterPrint = on; }

    /*! Current aggregated value. */
    QVariant result() const;

    /*! The pager whose records are accumulated (the band's pager). */
    BrPager *ownerPager() const;

    // Engine interface
    void reset();
    void accumulate(BrRenderContext &ctx);
    void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page) override;

protected:
    QString computeText(BrRenderContext &ctx) override;

private:
    Br::ResultType m_type = Br::ResultType::Sum;
    bool m_resetAfterPrint = false;
    double m_sum = 0.0;
    qint64 m_count = 0;
    QVariant m_first, m_last, m_min, m_max;
};

/*!
 * System information: page numbers, date/time, title...
 *
 * format() is a template where %1 is the information and, for PagePreview,
 * %2 is the total number of pages. Default: "%1" ("%1/%2" for PagePreview).
 * Example: setInfo(Br::InfoType::PagePreview); setFormat("Page %1 of %2");
 */
class BRREPORT_EXPORT BrSystemInfo : public BrCustomText
{
    Q_OBJECT
public:
    explicit BrSystemInfo(BrContainer *parent = nullptr);
    BrSystemInfo(Br::InfoType info, BrContainer *parent);

    Br::InfoType info() const { return m_info; }
    void setInfo(Br::InfoType info) { m_info = info; }

    QString format() const { return m_format; }
    void setFormat(const QString &format) { m_format = format; }

    /*! Date/time format for Date, Time and DateTime (Qt patterns). Empty = locale default. */
    QString displayFormat() const { return m_displayFormat; }
    void setDisplayFormat(const QString &format) { m_displayFormat = format; }

protected:
    QString computeText(BrRenderContext &ctx) override;

private:
    Br::InfoType m_info = Br::InfoType::PageNumber;
    QString m_format;
    QString m_displayFormat;
};

#endif // BRTEXTCONTROLS_H
