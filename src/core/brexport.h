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

#ifndef BREXPORT_H
#define BREXPORT_H

#include "brdocument.h"

#include <QStringList>

/*!
 * Export filters for prepared documents.
 *
 * All functions work without QtWidgets, so they can run in console/server
 * applications (a QGuiApplication is still required for fonts; use
 * "-platform offscreen" or QT_QPA_PLATFORM=offscreen on headless machines).
 */
class BRREPORT_EXPORT BrExport
{
public:
    /*! Writes a vector PDF. Text remains selectable/searchable. */
    static bool toPdf(const BrDocument &doc, const QString &fileName, int resolution = 1200);

    /*!
     * Writes one image per page. \a filePattern must contain "%1", replaced by the
     * page number (1-based), e.g. "page-%1.png". The format comes from the extension.
     * Returns the written files (empty on failure).
     */
    static QStringList toImages(const BrDocument &doc, const QString &filePattern,
                                double dpi = 150.0);

    /*! Returns a standalone SVG for one page (units in mm). */
    static QString toSvg(const BrPage &page);

    /*! Writes an HTML file with every page embedded as inline SVG. */
    static bool toHtml(const BrDocument &doc, const QString &fileName);

    /*!
     * Text of the document on a grid of characters, for dot-matrix and thermal printers.
     * \a charWidthMm and \a lineHeightMm set the grid: the default is 10 characters per
     * inch and 6 lines per inch. Pages are separated by a form feed. Only text is kept.
     */
    static QString toText(const BrDocument &doc, double charWidthMm = 2.54, double lineHeightMm = 4.233);
    /*! Writes toText() to a file in UTF-8. */
    static bool toTextFile(const BrDocument &doc, const QString &fileName, double charWidthMm = 2.54,
                           double lineHeightMm = 4.233);

    /*!
     * Writes a spreadsheet with the text of the document: objects at the same height
     * become a row and objects with the same left edge a column, in a single sheet.
     * Text that reads as a number becomes a number. Images and shapes are left out.
     */
    static bool toXlsx(const BrDocument &doc, const QString &fileName);

    /*!
     * Writes a word processor document that keeps the layout of the pages: text in
     * positioned frames, shapes and images. Rich text keeps its lines, not its styles.
     */
    static bool toRtf(const BrDocument &doc, const QString &fileName);
};

#endif // BREXPORT_H
