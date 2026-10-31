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

#ifndef BRTEXTMETRICS_H
#define BRTEXTMETRICS_H

#include "brglobal.h"

#include <QFont>
#include <QStringList>

/*!
 * Device-independent text measurement in millimeters.
 *
 * Measurements use a high-resolution reference device and no font hinting, so
 * the layout computed while preparing a report is the same on screen, printer,
 * PDF and images.
 */
class BRREPORT_EXPORT BrTextMetrics
{
public:
    /*! Width of a single line of text, in mm. */
    static double width(const QFont &font, const QString &text);
    /*! Distance between two baselines, in mm. */
    static double lineHeight(const QFont &font);
    /*!
     * Breaks \a text into lines that fit \a widthMm. Explicit line breaks ("\n")
     * are always honored; when \a wordWrap is false only those are used.
     * If \a paragraphEnds is given, it receives one flag per line telling whether
     * the line ends a paragraph (used for justification).
     */
    static QStringList wrap(const QFont &font, const QString &text, double widthMm, bool wordWrap,
                            QList<bool> *paragraphEnds = nullptr);
};

#endif // BRTEXTMETRICS_H
