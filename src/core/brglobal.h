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

#ifndef BRGLOBAL_H
#define BRGLOBAL_H

#include <QMetaType>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QtGlobal>

#include "brreport_config.h"

#if defined(BRREPORT_STATIC)
#  define BRREPORT_EXPORT
#elif defined(BRREPORT_CORE_LIBRARY)
#  define BRREPORT_EXPORT Q_DECL_EXPORT
#else
#  define BRREPORT_EXPORT Q_DECL_IMPORT
#endif

/*!
 * Common enumerations used across BrReport.
 *
 * All geometry in BrReport is expressed in millimeters (mm) and font sizes in points (pt).
 */
namespace Br {
Q_NAMESPACE_EXPORT(BRREPORT_EXPORT)

/*! Role of a band inside a pager (report, group or sub-detail). */
enum class BandType {
    Header,       //!< Printed at the top of every page (and when a group/sub-detail starts).
    Title,        //!< Printed once, when the pager starts (first page for the report).
    ColumnHeader, //!< Printed after Header/Title on every page.
    Detail,       //!< Printed once for every data record.
    ColumnFooter, //!< Printed after the last detail of every page.
    Summary,      //!< Printed once, when the pager finishes (after the last record).
    Footer        //!< Report: bottom of every page. Group/sub-detail: at page breaks and at the end.
};
Q_ENUM_NS(BandType)

/*! Automatic placement of a control inside its parent (band or panel). */
enum class Align {
    None,   //!< Uses the control geometry as is.
    Top,    //!< Stacked at the top, full width.
    Bottom, //!< Stacked at the bottom, full width.
    Left,   //!< Stacked at the left, fills the remaining height.
    Right,  //!< Stacked at the right, fills the remaining height.
    Client  //!< Fills the remaining area.
};
Q_ENUM_NS(Align)

/*! Horizontal text alignment. */
enum class TextAlign { Left, Center, Right, Justify };
Q_ENUM_NS(TextAlign)

/*! Vertical text alignment. */
enum class TextLayout { Top, Center, Bottom };
Q_ENUM_NS(TextLayout)

/*! Page breaking behavior for bands and pagers. */
enum class PageBreak {
    None,   //!< No forced page break.
    Before, //!< Starts a new page before printing (unless the page is still empty).
    After   //!< Starts a new page after printing (only if something else is printed).
};
Q_ENUM_NS(PageBreak)

/*! System information displayed by BrSystemInfo. */
enum class InfoType {
    Date,           //!< Report date.
    Time,           //!< Report time.
    DateTime,       //!< Report date and time.
    PageNumber,     //!< Current page number (restarts with BrPager::setResetPageNumber()).
    LastPageNumber, //!< Total number of pages (of the current numbering section).
    PagePreview,    //!< "page / total" (see BrSystemInfo::setFormat()).
    Title,          //!< Report title.
    RecNo,          //!< Current record number of the innermost data pager.
    DetailCount,    //!< Number of records printed so far.
    CarbonCopy,         //!< Current carbon copy index (1-based) of the band.
    DocumentPageNumber, //!< Page number in the whole document (ignores page number resets).
    DocumentPageCount   //!< Number of pages of the whole document.
};
Q_ENUM_NS(InfoType)

/*! Aggregation computed by BrDBResult. */
enum class ResultType { Sum, Count, Average, Min, Max, First, Last };
Q_ENUM_NS(ResultType)

/*! Shape drawn by BrDraw. */
enum class DrawKind {
    Rectangle,
    RoundedRectangle,
    Ellipse,
    HorizontalLine, //!< Line across the vertical center.
    VerticalLine,   //!< Line across the horizontal center.
    DiagonalDown,   //!< Top-left to bottom-right.
    DiagonalUp,     //!< Bottom-left to top-right.
    Triangle        //!< Pointing up.
};
Q_ENUM_NS(DrawKind)

/*! How an image fits into the control rectangle. */
enum class ImageScale {
    None,       //!< Original size (at 96 dpi), anchored at the top-left corner.
    Stretch,    //!< Fills the whole rectangle.
    KeepAspect, //!< Fits the rectangle keeping the aspect ratio, centered.
    Center      //!< Original size (at 96 dpi), centered.
};
Q_ENUM_NS(ImageScale)

/*! Supported barcode symbologies. */
enum class BarcodeType {
    Code128,        //!< Full ASCII, automatic B/C code set switching.
    Ean13,          //!< 12 or 13 digits (check digit computed/validated).
    Interleaved2of5, //!< Numeric (used e.g. by Brazilian bank slips / "boletos").
    QrCode           //!< 2D QR Code (URLs, PIX, NFC-e...). See BrQrEncoder.
};
Q_ENUM_NS(BarcodeType)

/*! QR Code error correction level (share of the symbol that can be recovered). */
enum class QrErrorCorrection {
    Low,      //!< ~7%
    Medium,   //!< ~15%
    Quartile, //!< ~25%
    High      //!< ~30%
};
Q_ENUM_NS(QrErrorCorrection)

/*! Chart drawn by BrChart. */
enum class ChartType {
    Pie,           //!< Slices of the first series.
    Bar,           //!< Vertical bars, one group per label.
    HorizontalBar, //!< Horizontal bars, one group per label.
    Line           //!< One line per series.
};
Q_ENUM_NS(ChartType)

/*! Converts typographic points to millimeters. */
constexpr double ptToMm(double pt) { return pt * 25.4 / 72.0; }
/*! Converts millimeters to typographic points. */
constexpr double mmToPt(double mm) { return mm * 72.0 / 25.4; }

} // namespace Br

/*!
 * What the end user cannot change in a report object, in the report designer.
 *
 * It only restricts the designer: printing ignores it, and it is not a security
 * feature, because a report file is plain text. Stored in the report file.
 */
struct BRREPORT_EXPORT BrProtection
{
    bool move = false;            //!< Position, size and order.
    bool remove = false;          //!< The object cannot be deleted.
    bool properties = false;      //!< All the properties are read-only, including the name.
    bool contents = false;        //!< Nothing can be added to the object or removed from it.
    QStringList hiddenProperties; //!< Names of the properties not shown to the end user.

    bool any() const { return move || remove || properties || contents || !hiddenProperties.isEmpty(); }
    /*! Everything locked, keeping \a hidden properties out of sight. */
    static BrProtection all(const QStringList &hidden = {})
    {
        return BrProtection{true, true, true, true, hidden};
    }
    /*! The protection of a control, band or pager; none for other objects. */
    static BrProtection of(const QObject *object);

    bool operator==(const BrProtection &o) const
    {
        return move == o.move && remove == o.remove && properties == o.properties && contents == o.contents
               && hiddenProperties == o.hiddenProperties;
    }
    bool operator!=(const BrProtection &o) const { return !(*this == o); }
};
Q_DECLARE_METATYPE(BrProtection)

inline BrProtection BrProtection::of(const QObject *object)
{
    return object ? object->property("protection").value<BrProtection>() : BrProtection();
}

#endif // BRGLOBAL_H
