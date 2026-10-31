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

#ifndef BRQRCODE_H
#define BRQRCODE_H

#include "brglobal.h"

#include <QByteArray>
#include <QString>
#include <QVector>

/*!
 * QR Code (ISO/IEC 18004) encoder, versions 1 to 40.
 *
 * The input is encoded in the most compact single mode: numeric, alphanumeric
 * or byte (UTF-8). The smallest version that fits is chosen; the mask with the
 * lowest penalty is selected unless one is forced.
 *
 * \code
 * const BrQrEncoder::Result qr = BrQrEncoder::encode("https://example.com");
 * for (int y = 0; y < qr.size; ++y)
 *     for (int x = 0; x < qr.size; ++x)
 *         if (qr.module(x, y)) ... // dark module
 * \endcode
 */
class BRREPORT_EXPORT BrQrEncoder
{
public:
    struct Result
    {
        int version = 0; //!< 1..40, 0 on error.
        int size = 0;    //!< Modules per side (version * 4 + 17).
        int mask = -1;   //!< Mask pattern used (0..7).
        Br::QrErrorCorrection errorCorrection = Br::QrErrorCorrection::Medium;
        QVector<bool> modules; //!< size * size, row-major; true = dark.
        QString error;

        bool isValid() const { return size > 0; }
        bool module(int x, int y) const { return modules.at(y * size + x); }
    };

    /*! Encodes text (numeric / alphanumeric / UTF-8 byte mode). */
    static Result encode(const QString &text,
                         Br::QrErrorCorrection ecc = Br::QrErrorCorrection::Medium,
                         int minVersion = 1, int mask = -1);

    /*! Encodes raw bytes (byte mode). */
    static Result encodeBytes(const QByteArray &data,
                              Br::QrErrorCorrection ecc = Br::QrErrorCorrection::Medium,
                              int minVersion = 1, int mask = -1);
};

#endif // BRQRCODE_H
