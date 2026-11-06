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

#ifndef BRMATRIXCODE_H
#define BRMATRIXCODE_H

#include "brglobal.h"

#include <QString>
#include <QVector>

/*! A two-dimensional symbol: a grid of dark and light modules. */
struct BRREPORT_EXPORT BrMatrixCode
{
    int width = 0;  //!< Modules per row, 0 on error.
    int height = 0; //!< Rows of modules.
    QVector<bool> modules; //!< width * height, row-major; true = dark.
    QString error;

    bool isValid() const { return width > 0; }
    bool module(int x, int y) const { return modules.at(y * width + x); }
};

/*!
 * Data Matrix ECC 200 encoder (ISO/IEC 16022): square symbols from 10x10 to 144x144.
 * The smallest symbol that fits is chosen. Text is encoded as UTF-8 bytes in ASCII
 * mode, with pairs of digits packed; up to 1556 digits or 778 letters.
 */
class BRREPORT_EXPORT BrDataMatrixEncoder
{
public:
    static BrMatrixCode encode(const QString &text);
    /*! The codewords of a symbol: data followed by error correction (for tests). */
    static QVector<int> codewords(const QString &text);
};

/*!
 * PDF417 encoder (ISO/IEC 15438). Digits use numeric compaction and any other text
 * byte compaction (UTF-8). Rows are 3 modules high.
 */
class BRREPORT_EXPORT BrPdf417Encoder
{
public:
    /*!
     * Encodes \a text with the number of columns that gives the symbol closest to
     * \a aspect (width / height). \a securityLevel (0..8) sets the number of error
     * correction codewords, 2^(level + 1); -1 chooses it from the size of the data.
     */
    static BrMatrixCode encode(const QString &text, double aspect = 3.0, int securityLevel = -1);
};

#endif // BRMATRIXCODE_H
