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

#ifndef BRBARCODE_H
#define BRBARCODE_H

#include "brcontrol.h"

#include <QVector>

struct BrMatrixCode;

/*!
 * Encodes barcode symbols as a sequence of module widths, starting with a bar
 * and alternating bar/space.
 */
class BRREPORT_EXPORT BrBarcodeEncoder
{
public:
    struct BRREPORT_EXPORT Result
    {
        QVector<int> widths; //!< Module widths (bar, space, bar, ...). Empty on error.
        QString humanText;   //!< Text to print below the bars (with check digits).
        QString error;
        bool isValid() const { return !widths.isEmpty(); }
        int totalModules() const;
    };

    static Result encode(Br::BarcodeType type, const QString &data);

    static Result code128(const QString &data);
    static Result ean13(const QString &data);
    /*! Odd-length input is padded with a leading zero. Wide bars are \a ratio modules. */
    static Result interleaved2of5(const QString &data, int ratio = 3);
    /*! Without a check digit. Wide bars are \a ratio modules. */
    static Result code39(const QString &data, int ratio = 3);

    /*! EAN-13 check digit for 12 digits (returns -1 on invalid input). */
    static int ean13CheckDigit(const QString &twelveDigits);
};

/*!
 * Barcode control. The bars fill the control width; the human
 * readable text is printed below them when showText() is true.
 *
 * Two-dimensional types (QR Code, Data Matrix, PDF417) are drawn as large as they
 * fit the control, centered; showText() and moduleWidth() then do not apply.
 */
class BRREPORT_EXPORT BrBarcode : public BrControl
{
    Q_OBJECT
    Q_PROPERTY(Br::BarcodeType barcodeType READ barcodeType WRITE setBarcodeType)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool showText READ showText WRITE setShowText)
    Q_PROPERTY(double moduleWidth READ moduleWidth WRITE setModuleWidth)
    Q_PROPERTY(int ratio READ ratio WRITE setRatio)
    Q_PROPERTY(QColor barColor READ barColor WRITE setBarColor)
    Q_PROPERTY(Br::QrErrorCorrection qrErrorCorrection READ qrErrorCorrection WRITE setQrErrorCorrection)
    Q_PROPERTY(int quietZone READ quietZone WRITE setQuietZone)
public:
    explicit BrBarcode(BrContainer *parent = nullptr);
    BrBarcode(Br::BarcodeType type, const QString &text, BrContainer *parent);

    Br::BarcodeType barcodeType() const { return m_type; }
    void setBarcodeType(Br::BarcodeType type) { m_type = type; }

    QString text() const { return m_text; }
    void setText(const QString &text) { m_text = text; }

    bool showText() const { return m_showText; }
    void setShowText(bool show) { m_showText = show; }

    /*!
     * Width of the narrowest bar in mm. 0 (default) stretches the code to the
     * control width; otherwise the code is centered with this module width.
     */
    double moduleWidth() const { return m_moduleWidth; }
    void setModuleWidth(double mm) { m_moduleWidth = mm; }

    /*! Wide/narrow ratio for Interleaved 2 of 5 and Code 39 (2 or 3; default 3). */
    int ratio() const { return m_ratio; }
    void setRatio(int ratio) { m_ratio = ratio; }

    QColor barColor() const { return m_barColor; }
    void setBarColor(const QColor &color) { m_barColor = color; }

    /*! QR Code error correction level (default Medium). */
    Br::QrErrorCorrection qrErrorCorrection() const { return m_qrEcc; }
    void setQrErrorCorrection(Br::QrErrorCorrection level) { m_qrEcc = level; }

    /*!
     * Blank margin around a two-dimensional code, in modules (default 2). The QR Code
     * standard asks for 4; less is fine when the area around the control is already blank.
     */
    int quietZone() const { return m_quietZone; }
    void setQuietZone(int modules) { m_quietZone = qMax(0, modules); }

    void prepare(BrRenderContext &ctx) override;
    void render(BrRenderContext &ctx, const QRectF &rect, BrPage &page) override;

protected:
    /*! Data to encode for the current print. */
    virtual QString currentData(BrRenderContext &ctx);

private:
    Br::BarcodeType m_type = Br::BarcodeType::Code128;
    QString m_text;
    QString m_current;
    bool m_showText = true;
    double m_moduleWidth = 0.0;
    int m_ratio = 3;
    QColor m_barColor = Qt::black;
    Br::QrErrorCorrection m_qrEcc = Br::QrErrorCorrection::Medium;
    int m_quietZone = 2;

    void renderMatrix(const BrMatrixCode &code, const QRectF &rect, BrPage &page);
};

/*! Barcode whose data comes from a field or expression. */
class BRREPORT_EXPORT BrDBBarcode : public BrBarcode
{
    Q_OBJECT
    Q_PROPERTY(QString dataField READ dataField WRITE setDataField)
public:
    explicit BrDBBarcode(BrContainer *parent = nullptr);
    BrDBBarcode(Br::BarcodeType type, const QString &dataField, BrContainer *parent);

    QString dataField() const { return m_dataField; }
    void setDataField(const QString &field) { m_dataField = field; }

protected:
    QString currentData(BrRenderContext &ctx) override;

private:
    QString m_dataField;
};

#endif // BRBARCODE_H
