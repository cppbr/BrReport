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

#ifndef BRRENDERER_H
#define BRRENDERER_H

#include "brdocument.h"

#include <QPointF>

class QPainter;

/*!
 * Paints prepared pages (BrPage) on any QPainter: screen, printer, PDF or image.
 */
class BRREPORT_EXPORT BrRenderer
{
public:
    /*!
     * Paints \a page with its top-left corner at \a offset (device pixels of the
     * painter), using \a pixelsPerMm as scale. The painter state is preserved.
     */
    static void renderPage(QPainter *painter, const BrPage &page, double pixelsPerMm,
                           const QPointF &offset = QPointF());

    /*! Paints a single item. The painter must already be in BrReport units (see renderPage()). */
    static void renderItem(QPainter *painter, const BrPageItem &item, double deviceDpi);

    /*! Renders \a page into an image with the given resolution. */
    static QImage toImage(const BrPage &page, double dpi = 150.0,
                          const QColor &background = Qt::white);

    /*! Painter units per millimeter used internally (1 unit = 0.01 mm). */
    static constexpr double UnitsPerMm = 100.0;
};

#endif // BRRENDERER_H
