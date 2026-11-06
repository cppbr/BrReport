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

#include <QPoint>
#include <QTest>

class TstBarcode : public QObject
{
    Q_OBJECT
private slots:
    void code128Numeric()
    {
        const auto r = BrBarcodeEncoder::code128(QStringLiteral("12345678"));
        QVERIFY(r.isValid());
        // start C + 4 pairs + check = 6 symbols * 11 modules + stop (13)
        QCOMPARE(r.totalModules(), 6 * 11 + 13);
        // Every symbol has 3 bars and 3 spaces; the stop has 4 bars and 3 spaces.
        QCOMPARE(r.widths.size(), 6 * 6 + 7);
    }
    void code128Text()
    {
        const auto r = BrBarcodeEncoder::code128(QStringLiteral("BrReport-2026"));
        QVERIFY(r.isValid());
        QCOMPARE(r.totalModules() % 11, 13 % 11);
        QVERIFY(!BrBarcodeEncoder::code128(QStringLiteral("café")).isValid());
    }
    void code128Mixed()
    {
        // Letters followed by a long digit run switch to code set C.
        const auto r = BrBarcodeEncoder::code128(QStringLiteral("AB123456"));
        QVERIFY(r.isValid());
        // start B, A, B, CODE C, 12, 34, 56, check = 8 symbols
        QCOMPARE(r.totalModules(), 8 * 11 + 13);
    }
    void ean13()
    {
        QCOMPARE(BrBarcodeEncoder::ean13CheckDigit(QStringLiteral("590123412345")), 7);
        const auto r = BrBarcodeEncoder::ean13(QStringLiteral("590123412345"));
        QVERIFY(r.isValid());
        QCOMPARE(r.humanText, QStringLiteral("5901234123457"));
        QCOMPARE(r.totalModules(), 95);
        QVERIFY(!BrBarcodeEncoder::ean13(QStringLiteral("5901234123450")).isValid());
        QVERIFY(!BrBarcodeEncoder::ean13(QStringLiteral("12ab")).isValid());
    }
    void itf()
    {
        const auto r = BrBarcodeEncoder::interleaved2of5(QStringLiteral("1234"));
        QVERIFY(r.isValid());
        // start 4 + 2 pairs * (2 digits * (2 wide*3 + 3 narrow)) + stop (3+1+1)
        QCOMPARE(r.totalModules(), 4 + 2 * 18 + 5);
        QCOMPARE(BrBarcodeEncoder::interleaved2of5(QStringLiteral("123")).humanText,
                 QStringLiteral("0123"));
        // Brazilian bank slip: 44 digits
        QVERIFY(BrBarcodeEncoder::interleaved2of5(QString(44, QLatin1Char('9'))).isValid());
    }
    void code39()
    {
        const auto r = BrBarcodeEncoder::code39(QStringLiteral("A1"));
        QVERIFY(r.isValid());
        QCOMPARE(r.humanText, QStringLiteral("A1"));
        // * A 1 *: 4 characters of 9 elements (3 wide) and 3 gaps
        QCOMPARE(r.widths.size(), 4 * 9 + 3);
        QCOMPARE(r.totalModules(), 4 * (6 + 3 * 3) + 3);
        // The start character, then the gap
        QCOMPARE(r.widths.mid(0, 10), QVector<int>({1, 3, 1, 1, 3, 1, 3, 1, 1, 1}));
        QCOMPARE(BrBarcodeEncoder::code39(QStringLiteral("A1"), 2).totalModules(), 4 * (6 + 3 * 2) + 3);
        QVERIFY(!BrBarcodeEncoder::code39(QStringLiteral("abc")).isValid());
        QVERIFY(!BrBarcodeEncoder::code39(QStringLiteral("A*B")).isValid());
        QVERIFY(!BrBarcodeEncoder::code39(QString()).isValid());
    }

    void dataMatrixReferenceCodewords()
    {
        // The example of a 10x10 symbol: digit pairs, then Reed-Solomon
        QCOMPARE(BrDataMatrixEncoder::codewords(QStringLiteral("123456")),
                 QVector<int>({142, 164, 186, 114, 25, 5, 88, 102}));
    }

    void dataMatrixSizesAndFinder()
    {
        const auto check = [](const QString &text, int size) {
            const BrMatrixCode code = BrDataMatrixEncoder::encode(text);
            QVERIFY(code.isValid());
            QCOMPARE(code.width, size);
            QCOMPARE(code.height, size);
            for (int i = 0; i < size; ++i) {
                QVERIFY(code.module(0, i));        // solid left edge
                QVERIFY(code.module(i, size - 1)); // solid bottom edge
                QCOMPARE(code.module(i, 0), i % 2 == 0); // timing along the top
            }
        };
        check(QStringLiteral("AAA"), 10);
        check(QStringLiteral("AAAA"), 12);
        check(QString(44, QLatin1Char('A')), 26);
        check(QString(62, QLatin1Char('A')), 32); // two data regions per side
        check(QString(1558, QLatin1Char('A')), 144);
        QVERIFY(!BrDataMatrixEncoder::encode(QString(1559, QLatin1Char('A'))).isValid());
        QVERIFY(!BrDataMatrixEncoder::encode(QString()).isValid());

        // The inner finder lines of a symbol with several data regions
        const BrMatrixCode code = BrDataMatrixEncoder::encode(QString(62, QLatin1Char('A')));
        for (int i = 0; i < code.height; ++i)
            QVERIFY(code.module(16, i));
    }

    void pdf417Structure()
    {
        const BrMatrixCode code = BrPdf417Encoder::encode(QStringLiteral("Hello, PDF417 world!"));
        QVERIFY(code.isValid());
        QCOMPARE(code.height % 3, 0); // rows of 3 modules
        QCOMPARE((code.width - 69) % 17, 0);
        const auto bits = [&code](int row, int from, int count) {
            QString s;
            for (int x = from; x < from + count; ++x)
                s += code.module(x, row * 3) ? QLatin1Char('1') : QLatin1Char('0');
            return s;
        };
        for (int row = 0; row < code.height / 3; ++row) {
            QCOMPARE(bits(row, 0, 17), QStringLiteral("11111111010101000"));
            QCOMPARE(bits(row, code.width - 18, 18), QStringLiteral("111111101000101001"));
        }
    }

    void pdf417Shape()
    {
        const QString text(300, QLatin1Char('x'));
        const BrMatrixCode wide = BrPdf417Encoder::encode(text, 6.0);
        const BrMatrixCode tall = BrPdf417Encoder::encode(text, 0.5);
        QVERIFY(double(wide.width) / wide.height > double(tall.width) / tall.height);
        QVERIFY(!BrPdf417Encoder::encode(QString(2000, QLatin1Char('x'))).isValid());
        QVERIFY(!BrPdf417Encoder::encode(QString()).isValid());
    }

    void qrReferenceMatrix()
    {
        // Version 1-Q, mask 6, verified against an independent encoder.
        const char *expected[] = {
            "111111100001001111111",
            "100000101100101000001",
            "101110100101101011101",
            "101110101111101011101",
            "101110101101001011101",
            "100000100100101000001",
            "111111101010101111111",
            "000000001101100000000",
            "010111101100111011010",
            "101111010000111101110",
            "001010110001001100000",
            "101101000101100011000",
            "110111111110111011111",
            "000000001000100101000",
            "111111100110011001111",
            "100000101010010010111",
            "101110101101001000111",
            "101110101011100010100",
            "101110100100001000011",
            "100000101110011100110",
            "111111100101000000010",
        };
        const auto r = BrQrEncoder::encode(QStringLiteral("HELLO WORLD"),
                                           Br::QrErrorCorrection::Quartile, 1, 6);
        QVERIFY(r.isValid());
        QCOMPARE(r.version, 1);
        QCOMPARE(r.size, 21);
        for (int y = 0; y < 21; ++y) {
            QString row;
            for (int x = 0; x < 21; ++x)
                row += r.module(x, y) ? QLatin1Char('1') : QLatin1Char('0');
            QCOMPARE(row, QString::fromLatin1(expected[y]));
        }
    }
    void qrVersionsAndModes()
    {
        using E = Br::QrErrorCorrection;
        // Version 40-L holds 7089 digits, 4296 alphanumeric characters or 2953 bytes.
        QCOMPARE(BrQrEncoder::encode(QString(7089, QLatin1Char('7')), E::Low).version, 40);
        QVERIFY(!BrQrEncoder::encode(QString(7090, QLatin1Char('7')), E::Low).isValid());
        QCOMPARE(BrQrEncoder::encode(QString(4296, QLatin1Char('A')), E::Low).version, 40);
        QCOMPARE(BrQrEncoder::encode(QString(2953, QLatin1Char('a')), E::Low).version, 40);
        QVERIFY(!BrQrEncoder::encode(QString(2954, QLatin1Char('a')), E::Low).isValid());
        // Smallest version that fits
        QCOMPARE(BrQrEncoder::encode(QString(41, QLatin1Char('1')), E::Low).version, 1);
        QCOMPARE(BrQrEncoder::encode(QString(42, QLatin1Char('1')), E::Low).version, 2);
        // Minimum version is honored; size follows the version
        const auto big = BrQrEncoder::encode(QStringLiteral("x"), E::High, 10);
        QCOMPARE(big.version, 10);
        QCOMPARE(big.size, 57);
        QCOMPARE(big.modules.size(), 57 * 57);
        // UTF-8 byte mode, invalid mask
        QVERIFY(BrQrEncoder::encode(QStringLiteral("Ação €")).isValid());
        QVERIFY(!BrQrEncoder::encode(QStringLiteral("x"), E::Low, 1, 9).isValid());
    }
    void qrFunctionPatterns()
    {
        const auto r = BrQrEncoder::encode(QStringLiteral("https://example.com/brreport"),
                                           Br::QrErrorCorrection::Medium, 7);
        QVERIFY(r.isValid());
        QVERIFY(r.mask >= 0 && r.mask <= 7);
        const int n = r.size;
        for (const QPoint &c : {QPoint(0, 0), QPoint(n - 7, 0), QPoint(0, n - 7)}) {
            QVERIFY(r.module(c.x(), c.y()));
            QVERIFY(r.module(c.x() + 3, c.y() + 3));
            QVERIFY(!r.module(c.x() + 1, c.y() + 1));
        }
        for (int i = 8; i < n - 8; ++i) {
            QCOMPARE(r.module(i, 6), i % 2 == 0);
            QCOMPARE(r.module(6, i), i % 2 == 0);
        }
        QVERIFY(r.module(8, n - 8)); // always-dark module
    }
};

QTEST_GUILESS_MAIN(TstBarcode)
#include "tst_barcode.moc"
