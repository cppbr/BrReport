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

#include "brbarcode.h"

#include "brmatrixcode.h"
#include "brqrcode.h"
#include "brrendercontext.h"
#include "brtextmetrics.h"

#include <algorithm>
#include <cstring>

namespace {

// Code 128 symbol patterns (bar/space widths), values 0..105, followed by STOP.
const char *const kCode128[] = {
    "212222", "222122", "222221", "121223", "121322", "131222", "122213", "122312", "132212",
    "221213", "221312", "231212", "112232", "122132", "122231", "113222", "123122", "123221",
    "223211", "221132", "221231", "213212", "223112", "312131", "311222", "321122", "321221",
    "312212", "322112", "322211", "212123", "212321", "232121", "111323", "131123", "131321",
    "112313", "132113", "132311", "211313", "231113", "231311", "112133", "112331", "132131",
    "113123", "113321", "133121", "313121", "211331", "231131", "213113", "213311", "213131",
    "311123", "311321", "331121", "312113", "312311", "332111", "314111", "221411", "431111",
    "111224", "111422", "121124", "121421", "141122", "141221", "112214", "112412", "122114",
    "122411", "142112", "142211", "241211", "221114", "413111", "241112", "134111", "111242",
    "121142", "121241", "114212", "124112", "124211", "411212", "421112", "421211", "212141",
    "214121", "412121", "111143", "111341", "131141", "114113", "114311", "411113", "411311",
    "113141", "114131", "311141", "411131", "211412", "211214", "211232", "2331112"};

const int kStartB = 104;
const int kStartC = 105;
const int kCodeB = 100;
const int kCodeC = 99;
const int kStop = 106;

// EAN-13 left-hand odd parity (L) patterns; R = complement of L, G = reverse of R.
const char *const kEanL[] = {"0001101", "0011001", "0010011", "0111101", "0100011",
                             "0110001", "0101111", "0111011", "0110111", "0001011"};
const char *const kEanParity[] = {"LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG",
                                  "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL"};

// Interleaved 2 of 5: n = narrow, w = wide.
const char *const kItf[] = {"nnwwn", "wnnnw", "nwnnw", "wwnnn", "nnwnw",
                            "wnwnn", "nwwnn", "nnnww", "wnnwn", "nwnwn"};

// Code 39: n = narrow, w = wide, bar first. The index is the position in kCode39Chars.
const char kCode39Chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%*";
const char *const kCode39[] = {
    "nnnwwnwnn", "wnnwnnnnw", "nnwwnnnnw", "wnwwnnnnn", "nnnwwnnnw", "wnnwwnnnn", "nnwwwnnnn",
    "nnnwnnwnw", "wnnwnnwnn", "nnwwnnwnn", "wnnnnwnnw", "nnwnnwnnw", "wnwnnwnnn", "nnnnwwnnw",
    "wnnnwwnnn", "nnwnwwnnn", "nnnnnwwnw", "wnnnnwwnn", "nnwnnwwnn", "nnnnwwwnn", "wnnnnnnww",
    "nnwnnnnww", "wnwnnnnwn", "nnnnwnnww", "wnnnwnnwn", "nnwnwnnwn", "nnnnnnwww", "wnnnnnwwn",
    "nnwnnnwwn", "nnnnwnwwn", "wwnnnnnnw", "nwwnnnnnw", "wwwnnnnnn", "nwnnwnnnw", "wwnnwnnnn",
    "nwwnwnnnn", "nwnnnnwnw", "wwnnnnwnn", "nwwnnnwnn", "nwnwnwnnn", "nwnwnnnwn", "nwnnnwnwn",
    "nnnwnwnwn", "nwnnwnwnn"};

bool allDigits(const QString &s)
{
    for (QChar c : s)
        if (!c.isDigit() || c.unicode() > '9')
            return false;
    return !s.isEmpty();
}

QVector<int> bitsToWidths(const QString &bits)
{
    QVector<int> widths;
    QChar current = QLatin1Char('1');
    int run = 0;
    for (QChar b : bits) {
        if (b == current) {
            ++run;
        } else {
            widths << run;
            current = b;
            run = 1;
        }
    }
    widths << run;
    return widths;
}

BrBarcodeEncoder::Result fail(const QString &error)
{
    BrBarcodeEncoder::Result r;
    r.error = error;
    return r;
}

} // namespace

int BrBarcodeEncoder::Result::totalModules() const
{
    int total = 0;
    for (int w : widths)
        total += w;
    return total;
}

BrBarcodeEncoder::Result BrBarcodeEncoder::encode(Br::BarcodeType type, const QString &data)
{
    switch (type) {
    case Br::BarcodeType::Code128:
        return code128(data);
    case Br::BarcodeType::Ean13:
        return ean13(data);
    case Br::BarcodeType::Interleaved2of5:
        return interleaved2of5(data);
    case Br::BarcodeType::QrCode:
    case Br::BarcodeType::DataMatrix:
    case Br::BarcodeType::Pdf417:
        return fail(QStringLiteral("two-dimensional code: use its encoder"));
    case Br::BarcodeType::Code39:
        return code39(data);
    }
    return fail(QStringLiteral("unknown barcode type"));
}

BrBarcodeEncoder::Result BrBarcodeEncoder::code128(const QString &data)
{
    if (data.isEmpty())
        return fail(QStringLiteral("empty data"));
    const int len = int(data.size());
    auto digitRun = [&](int from) {
        int n = 0;
        while (from + n < len && data.at(from + n).unicode() >= '0'
               && data.at(from + n).unicode() <= '9')
            ++n;
        return n;
    };

    QVector<int> values;
    bool inC = false;
    const int firstRun = digitRun(0);
    if ((firstRun == len && len >= 2 && len % 2 == 0) || firstRun >= 4) {
        values << kStartC;
        inC = true;
    } else {
        values << kStartB;
    }

    int i = 0;
    while (i < len) {
        if (inC) {
            if (digitRun(i) >= 2) {
                values << data.mid(i, 2).toInt();
                i += 2;
            } else {
                values << kCodeB;
                inC = false;
            }
            continue;
        }
        const int run = digitRun(i);
        if (run >= 4 && (i + run == len || run >= 6)) {
            if (run % 2 == 1) {
                values << data.at(i).unicode() - 32;
                ++i;
            }
            values << kCodeC;
            inC = true;
            continue;
        }
        const ushort c = data.at(i).unicode();
        if (c < 32 || c > 126)
            return fail(QStringLiteral("Code 128: unsupported character at position %1").arg(i + 1));
        values << int(c) - 32;
        ++i;
    }

    int checksum = values.first();
    for (int k = 1; k < values.size(); ++k)
        checksum += values.at(k) * k;
    values << checksum % 103 << kStop;

    Result r;
    for (int v : values) {
        const char *pattern = kCode128[v];
        for (const char *p = pattern; *p; ++p)
            r.widths << (*p - '0');
    }
    r.humanText = data;
    return r;
}

int BrBarcodeEncoder::ean13CheckDigit(const QString &twelveDigits)
{
    if (twelveDigits.size() != 12 || !allDigits(twelveDigits))
        return -1;
    int sum = 0;
    for (int i = 0; i < 12; ++i) {
        const int d = twelveDigits.at(i).digitValue();
        sum += (i % 2 == 0) ? d : d * 3;
    }
    return (10 - sum % 10) % 10;
}

BrBarcodeEncoder::Result BrBarcodeEncoder::ean13(const QString &data)
{
    QString digits = data.trimmed();
    if (!allDigits(digits) || (digits.size() != 12 && digits.size() != 13))
        return fail(QStringLiteral("EAN-13: 12 or 13 digits expected"));
    const int check = ean13CheckDigit(digits.left(12));
    if (digits.size() == 13 && digits.at(12).digitValue() != check)
        return fail(QStringLiteral("EAN-13: invalid check digit"));
    if (digits.size() == 12)
        digits += QString::number(check);

    auto rPattern = [](int d) {
        QString s = QString::fromLatin1(kEanL[d]);
        for (QChar &c : s)
            c = c == QLatin1Char('0') ? QLatin1Char('1') : QLatin1Char('0');
        return s;
    };

    QString bits = QStringLiteral("101");
    const QString parity = QString::fromLatin1(kEanParity[digits.at(0).digitValue()]);
    for (int i = 1; i <= 6; ++i) {
        const int d = digits.at(i).digitValue();
        if (parity.at(i - 1) == QLatin1Char('L')) {
            bits += QString::fromLatin1(kEanL[d]);
        } else {
            QString g = rPattern(d);
            std::reverse(g.begin(), g.end());
            bits += g;
        }
    }
    bits += QStringLiteral("01010");
    for (int i = 7; i <= 12; ++i)
        bits += rPattern(digits.at(i).digitValue());
    bits += QStringLiteral("101");

    Result r;
    r.widths = bitsToWidths(bits);
    r.humanText = digits;
    return r;
}

BrBarcodeEncoder::Result BrBarcodeEncoder::interleaved2of5(const QString &data, int ratio)
{
    QString digits = data.trimmed();
    if (!allDigits(digits))
        return fail(QStringLiteral("Interleaved 2 of 5: only digits are allowed"));
    if (digits.size() % 2 == 1)
        digits.prepend(QLatin1Char('0'));
    const int wide = qBound(2, ratio, 3);

    Result r;
    r.widths << 1 << 1 << 1 << 1; // start: narrow bar/space/bar/space
    for (int i = 0; i < digits.size(); i += 2) {
        const char *bars = kItf[digits.at(i).digitValue()];
        const char *spaces = kItf[digits.at(i + 1).digitValue()];
        for (int k = 0; k < 5; ++k) {
            r.widths << (bars[k] == 'w' ? wide : 1);
            r.widths << (spaces[k] == 'w' ? wide : 1);
        }
    }
    r.widths << wide << 1 << 1; // stop: wide bar, narrow space, narrow bar
    r.humanText = digits;
    return r;
}

// ---------------------------------------------------------------------------
// BrBarcode
// ---------------------------------------------------------------------------

BrBarcodeEncoder::Result BrBarcodeEncoder::code39(const QString &data, int ratio)
{
    if (data.isEmpty())
        return fail(QStringLiteral("empty data"));
    const int wide = qBound(2, ratio, 3);
    const QString text = QLatin1Char('*') + data + QLatin1Char('*');
    Result r;
    for (int i = 0; i < text.size(); ++i) {
        const char *found = text.at(i).unicode() < 128 && text.at(i).unicode() != 0
                                ? strchr(kCode39Chars, text.at(i).toLatin1())
                                : nullptr;
        const bool inside = i > 0 && i + 1 < text.size();
        if (!found || (inside && text.at(i) == QLatin1Char('*')))
            return fail(QStringLiteral("Code 39 has no character '%1'").arg(text.at(i)));
        for (const char *element = kCode39[found - kCode39Chars]; *element; ++element)
            r.widths << (*element == 'w' ? wide : 1);
        if (i + 1 < text.size())
            r.widths << 1; // gap between characters
    }
    r.humanText = data;
    return r;
}

BrBarcode::BrBarcode(BrContainer *parent) : BrControl(parent)
{
    setGeometry(0, 0, 60, 15);
}

BrBarcode::BrBarcode(Br::BarcodeType type, const QString &text, BrContainer *parent)
    : BrBarcode(parent)
{
    m_type = type;
    m_text = text;
}

QString BrBarcode::currentData(BrRenderContext &)
{
    return m_text;
}

void BrBarcode::prepare(BrRenderContext &ctx)
{
    m_current = currentData(ctx);
}

void BrBarcode::renderMatrix(const BrMatrixCode &code, const QRectF &rect, BrPage &page)
{
    if (!code.isValid()) {
        qWarning("BrBarcode: %s", qPrintable(code.error));
        return;
    }
    const double module = std::min(rect.width() / (code.width + 2 * m_quietZone),
                                   rect.height() / (code.height + 2 * m_quietZone));
    const QPointF origin(rect.center().x() - code.width * module / 2,
                         rect.center().y() - code.height * module / 2);
    // One rectangle per horizontal run of dark modules. Rows overlap slightly so
    // anti-aliased viewers do not show hairlines between them.
    const double overlap = module * 0.05;
    for (int y = 0; y < code.height; ++y) {
        int x = 0;
        while (x < code.width) {
            if (!code.module(x, y)) {
                ++x;
                continue;
            }
            const int start = x;
            while (x < code.width && code.module(x, y))
                ++x;
            const double h = module + (y + 1 < code.height ? overlap : 0.0);
            page.items << BrPageItem::makeRect(QRectF(origin.x() + start * module,
                                                      origin.y() + y * module,
                                                      (x - start) * module, h),
                                               QColor(), 0, m_barColor);
        }
    }
}

void BrBarcode::render(BrRenderContext &, const QRectF &rect, BrPage &page)
{
    renderFrame(rect, page);
    if (m_type == Br::BarcodeType::QrCode || m_type == Br::BarcodeType::DataMatrix
        || m_type == Br::BarcodeType::Pdf417) {
        if (m_current.isEmpty())
            return;
        BrMatrixCode matrix;
        if (m_type == Br::BarcodeType::QrCode) {
            const BrQrEncoder::Result qr = BrQrEncoder::encode(m_current, m_qrEcc);
            matrix.width = matrix.height = qr.size;
            matrix.modules = qr.modules;
            matrix.error = qr.error;
        } else if (m_type == Br::BarcodeType::DataMatrix) {
            matrix = BrDataMatrixEncoder::encode(m_current);
        } else {
            matrix = BrPdf417Encoder::encode(m_current, rect.width() / std::max(rect.height(), 0.1));
        }
        renderMatrix(matrix, rect, page);
        return;
    }
    const BrBarcodeEncoder::Result code =
        m_type == Br::BarcodeType::Interleaved2of5 ? BrBarcodeEncoder::interleaved2of5(m_current, m_ratio)
        : m_type == Br::BarcodeType::Code39        ? BrBarcodeEncoder::code39(m_current, m_ratio)
                                                   : BrBarcodeEncoder::encode(m_type, m_current);
    if (!code.isValid()) {
        if (!m_current.isEmpty())
            qWarning("BrBarcode: %s", qPrintable(code.error));
        return;
    }

    const QFont f = font();
    double textHeight = 0.0;
    if (m_showText)
        textHeight = BrTextMetrics::lineHeight(f);
    const double barsHeight = qMax(0.0, rect.height() - textHeight);

    const int modules = code.totalModules();
    double module = m_moduleWidth > 0 ? m_moduleWidth : rect.width() / modules;
    double x = rect.left();
    if (m_moduleWidth > 0)
        x = rect.center().x() - module * modules / 2.0;

    for (int i = 0; i < code.widths.size(); ++i) {
        const double w = code.widths.at(i) * module;
        if (i % 2 == 0)
            page.items << BrPageItem::makeRect(QRectF(x, rect.top(), w, barsHeight), QColor(), 0,
                                               m_barColor);
        x += w;
    }
    if (m_showText)
        page.items << BrPageItem::makeText(
            QRectF(rect.left(), rect.top() + barsHeight, rect.width(), textHeight), code.humanText,
            f, color(), Qt::AlignHCenter | Qt::AlignVCenter);
}

// ---------------------------------------------------------------------------

BrDBBarcode::BrDBBarcode(BrContainer *parent) : BrBarcode(parent) {}

BrDBBarcode::BrDBBarcode(Br::BarcodeType type, const QString &dataField, BrContainer *parent)
    : BrBarcode(type, QString(), parent), m_dataField(dataField)
{
}

QString BrDBBarcode::currentData(BrRenderContext &ctx)
{
    if (ctx.isDesignMode()) {
        switch (barcodeType()) {
        case Br::BarcodeType::Ean13:
            return QStringLiteral("7891234567895");
        case Br::BarcodeType::Interleaved2of5:
            return QStringLiteral("12345678");
        case Br::BarcodeType::Code39:
            return QStringLiteral("CODE39");
        default:
            return QLatin1Char('[') + m_dataField + QLatin1Char(']');
        }
    }
    return ctx.fieldValue(m_dataField).toString();
}
