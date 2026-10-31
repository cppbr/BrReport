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

#ifndef BRDOCUMENT_H
#define BRDOCUMENT_H

#include "brglobal.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QList>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>

class QIODevice;

/*!
 * A single drawing primitive of a prepared page.
 *
 * Coordinates are absolute page coordinates in millimeters. Once a report is prepared, it is
 * just a list of pages made of these items, which can be previewed, printed,
 * exported or saved to disk without the original data.
 */
struct BRREPORT_EXPORT BrPageItem
{
    enum Kind : quint8 {
        Text,    //!< One line of text drawn inside rect.
        Rect,    //!< Rectangle (optionally rounded with radius).
        Line,    //!< Line from points[0] to points[1].
        Ellipse, //!< Ellipse inscribed in rect.
        Polygon, //!< Closed polygon (points).
        Image,   //!< Image stretched into rect.
        RichText //!< HTML text (text) laid out in rect, using font and textColor as defaults.
    };

    Kind kind = Rect;
    QRectF rect;

    // Shapes
    QColor penColor = Qt::black;
    double penWidth = 0.0; //!< mm; 0 = hairline.
    Qt::PenStyle penStyle = Qt::SolidLine;
    QColor brushColor;     //!< Invalid color = no fill.
    double radius = 0.0;   //!< mm, used by rounded rectangles.
    QPolygonF points;

    // Text
    QString text;
    QFont font;
    QColor textColor = Qt::black;
    int alignment = Qt::AlignLeft | Qt::AlignVCenter; //!< Qt::Alignment flags.
    double angle = 0.0; //!< Counter-clockwise rotation in degrees, around the rect center.

    // Image
    QImage image;

    static BrPageItem makeText(const QRectF &rect, const QString &text, const QFont &font,
                               const QColor &color, int alignment, double angle = 0.0);
    static BrPageItem makeRect(const QRectF &rect, const QColor &pen, double penWidth,
                               const QColor &brush, Qt::PenStyle style = Qt::SolidLine);
    static BrPageItem makeLine(const QPointF &p1, const QPointF &p2, const QColor &pen,
                               double penWidth, Qt::PenStyle style = Qt::SolidLine);
    static BrPageItem makeImage(const QRectF &rect, const QImage &image);
};

/*! A prepared page: its size (mm) and the items drawn on it, in painting order. */
struct BRREPORT_EXPORT BrPage
{
    QSizeF size; //!< Page size in mm (already oriented).
    QList<BrPageItem> items;
};

/*!
 * A prepared report: the result of BrReport::prepare().
 *
 * It can be saved with save() and loaded later with load() (binary ".brp" format),
 * previewed (BrPreviewWidget), printed (BrPrinting) or exported (BrExport).
 */
class BRREPORT_EXPORT BrDocument
{
public:
    QString title;
    QList<BrPage> pages;

    int pageCount() const { return int(pages.size()); }
    bool isEmpty() const { return pages.isEmpty(); }
    void clear();

    /*! Replaces every occurrence of \a token in all text items. */
    void replaceText(const QString &token, const QString &value);

    /*! Concatenated text of a page (one item per line). Handy for tests and searching. */
    QString pageText(int page) const;

    bool save(QIODevice *device) const;
    bool save(const QString &fileName) const;
    bool load(QIODevice *device);
    bool load(const QString &fileName);
};

#endif // BRDOCUMENT_H
