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

#include "brmatrixcode.h"

#include "brpdf417table_p.h"

#include <QByteArray>

#include <algorithm>
#include <cmath>

namespace {

BrMatrixCode failed(const QString &error)
{
    BrMatrixCode code;
    code.error = error;
    return code;
}

// ---------------------------------------------------------------------------
// Data Matrix
// ---------------------------------------------------------------------------

struct DataMatrixSize
{
    int size;    // modules per side, finder pattern included
    int regions; // data regions per side
    int data;    // data codewords
    int ecc;     // error correction codewords per block
    int blocks;
};

const DataMatrixSize kDataMatrixSizes[] = {
    {10, 1, 3, 5, 1},      {12, 1, 5, 7, 1},       {14, 1, 8, 10, 1},      {16, 1, 12, 12, 1},
    {18, 1, 18, 14, 1},    {20, 1, 22, 18, 1},     {22, 1, 30, 20, 1},     {24, 1, 36, 24, 1},
    {26, 1, 44, 28, 1},    {32, 2, 62, 36, 1},     {36, 2, 86, 42, 1},     {40, 2, 114, 48, 1},
    {44, 2, 144, 56, 1},   {48, 2, 174, 68, 1},    {52, 2, 204, 42, 2},    {64, 4, 280, 56, 2},
    {72, 4, 368, 36, 4},   {80, 4, 456, 48, 4},    {88, 4, 576, 56, 4},    {96, 4, 696, 68, 4},
    {104, 4, 816, 56, 6},  {120, 6, 1050, 68, 6},  {132, 6, 1304, 62, 8},  {144, 6, 1558, 62, 10},
};

QVector<int> dataMatrixAscii(const QByteArray &bytes)
{
    const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    QVector<int> out;
    for (int i = 0; i < bytes.size(); ++i) {
        const auto c = static_cast<unsigned char>(bytes.at(i));
        if (isDigit(bytes.at(i)) && i + 1 < bytes.size() && isDigit(bytes.at(i + 1))) {
            out << 130 + (c - '0') * 10 + (bytes.at(i + 1) - '0');
            ++i;
        } else if (c < 128) {
            out << c + 1;
        } else {
            out << 235 << c - 127; // upper shift
        }
    }
    return out;
}

class GaloisField256
{
public:
    GaloisField256()
    {
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            m_exp[i] = x;
            m_log[x] = i;
            x <<= 1;
            if (x & 0x100)
                x ^= 0x12D;
        }
    }
    int multiply(int a, int b) const
    {
        return (a && b) ? m_exp[(m_log[a] + m_log[b]) % 255] : 0;
    }
    int power(int n) const { return m_exp[n % 255]; }

private:
    int m_exp[255];
    int m_log[256] = {};
};

QVector<int> reedSolomon256(const QVector<int> &data, int ecc)
{
    static const GaloisField256 field;
    // Generator: (x - a)(x - a^2)...(x - a^ecc), lowest degree first, monic.
    QVector<int> generator(ecc + 1, 0);
    generator[0] = 1;
    for (int i = 1; i <= ecc; ++i) {
        for (int j = i; j > 0; --j)
            generator[j] = generator[j - 1] ^ field.multiply(generator[j], field.power(i));
        generator[0] = field.multiply(generator[0], field.power(i));
    }
    QVector<int> rest(ecc, 0);
    for (int value : data) {
        const int feedback = value ^ rest.last();
        for (int j = ecc - 1; j > 0; --j)
            rest[j] = rest[j - 1] ^ field.multiply(feedback, generator[j]);
        rest[0] = field.multiply(feedback, generator[0]);
    }
    std::reverse(rest.begin(), rest.end());
    return rest;
}

const DataMatrixSize *dataMatrixSizeFor(int codewords)
{
    for (const DataMatrixSize &s : kDataMatrixSizes)
        if (s.data >= codewords)
            return &s;
    return nullptr;
}

QVector<int> dataMatrixCodewords(QVector<int> data, const DataMatrixSize &s)
{
    // Padding: 129, then pseudo-random values that depend on the position.
    if (data.size() < s.data)
        data << 129;
    while (data.size() < s.data) {
        const int position = int(data.size()) + 1;
        int pad = 129 + (149 * position) % 253 + 1;
        if (pad > 254)
            pad -= 254;
        data << pad;
    }
    // Blocks take the codewords in turn.
    QVector<int> all = data;
    all.resize(s.data + s.ecc * s.blocks);
    for (int b = 0; b < s.blocks; ++b) {
        QVector<int> block;
        for (int i = b; i < s.data; i += s.blocks)
            block << data.at(i);
        const QVector<int> ecc = reedSolomon256(block, s.ecc);
        for (int i = 0; i < s.ecc; ++i)
            all[s.data + i * s.blocks + b] = ecc.at(i);
    }
    return all;
}

/*! Places the codewords in the data region, along the diagonals of ISO/IEC 16022. */
class DataMatrixPlacement
{
public:
    DataMatrixPlacement(const QVector<int> &codewords, int size)
        : m_codewords(codewords), m_size(size), m_bits(size * size, -1)
    {
        int position = 0, row = 4, column = 0;
        do {
            if (row == m_size && column == 0)
                corner({{m_size - 1, 0}, {m_size - 1, 1}, {m_size - 1, 2}, {0, m_size - 2},
                        {0, m_size - 1}, {1, m_size - 1}, {2, m_size - 1}, {3, m_size - 1}}, position++);
            if (row == m_size - 2 && column == 0 && m_size % 4 != 0)
                corner({{m_size - 3, 0}, {m_size - 2, 0}, {m_size - 1, 0}, {0, m_size - 4},
                        {0, m_size - 3}, {0, m_size - 2}, {0, m_size - 1}, {1, m_size - 1}}, position++);
            if (row == m_size - 2 && column == 0 && m_size % 8 == 4)
                corner({{m_size - 3, 0}, {m_size - 2, 0}, {m_size - 1, 0}, {0, m_size - 2},
                        {0, m_size - 1}, {1, m_size - 1}, {2, m_size - 1}, {3, m_size - 1}}, position++);
            if (row == m_size + 4 && column == 2 && m_size % 8 == 0)
                corner({{m_size - 1, 0}, {m_size - 1, m_size - 1}, {0, m_size - 3}, {0, m_size - 2},
                        {0, m_size - 1}, {1, m_size - 3}, {1, m_size - 2}, {1, m_size - 1}}, position++);
            do {
                if (row < m_size && column >= 0 && bit(row, column) < 0)
                    utah(row, column, position++);
                row -= 2;
                column += 2;
            } while (row >= 0 && column < m_size);
            row += 1;
            column += 3;
            do {
                if (row >= 0 && column < m_size && bit(row, column) < 0)
                    utah(row, column, position++);
                row += 2;
                column -= 2;
            } while (row < m_size && column >= 0);
            row += 3;
            column += 1;
        } while (row < m_size || column < m_size);
        // Fixed pattern of the bottom right corner, when no codeword reaches it.
        if (bit(m_size - 1, m_size - 1) < 0) {
            m_bits[(m_size - 1) * m_size + m_size - 1] = 1;
            m_bits[(m_size - 2) * m_size + m_size - 2] = 1;
        }
    }

    bool isDark(int row, int column) const { return bit(row, column) == 1; }

private:
    struct Cell
    {
        int row;
        int column;
    };

    int bit(int row, int column) const { return m_bits.at(row * m_size + column); }

    void module(int row, int column, int position, int bitNumber)
    {
        if (row < 0) {
            row += m_size;
            column += 4 - ((m_size + 4) % 8);
        }
        if (column < 0) {
            column += m_size;
            row += 4 - ((m_size + 4) % 8);
        }
        m_bits[row * m_size + column] = (m_codewords.at(position) >> (8 - bitNumber)) & 1;
    }

    void corner(const QVector<Cell> &cells, int position)
    {
        for (int i = 0; i < 8; ++i)
            module(cells.at(i).row, cells.at(i).column, position, i + 1);
    }

    void utah(int row, int column, int position)
    {
        module(row - 2, column - 2, position, 1);
        module(row - 2, column - 1, position, 2);
        module(row - 1, column - 2, position, 3);
        module(row - 1, column - 1, position, 4);
        module(row - 1, column, position, 5);
        module(row, column - 2, position, 6);
        module(row, column - 1, position, 7);
        module(row, column, position, 8);
    }

    const QVector<int> &m_codewords;
    int m_size;
    QVector<int> m_bits; // -1 = not placed yet
};

// ---------------------------------------------------------------------------
// PDF417
// ---------------------------------------------------------------------------

const int kPdf417Modulus = 929;
const int kPdf417RowHeight = 3;
const int kPdf417Start = 0x1fea8; // 17 modules
const int kPdf417Stop = 0x3fa29;  // 18 modules
const int kPad = 900;
const int kLatchByte = 901;
const int kLatchNumeric = 902;
const int kLatchByteSix = 924;

QVector<int> pdf417Numeric(const QByteArray &digits)
{
    QVector<int> out{kLatchNumeric};
    for (int start = 0; start < digits.size(); start += 44) {
        // A group of digits, with a leading 1, as a number in base 900.
        QVector<int> number{1};
        for (char c : digits.mid(start, 44))
            number << c - '0';
        QVector<int> group;
        while (!number.isEmpty()) {
            int rest = 0;
            QVector<int> quotient;
            for (int digit : std::as_const(number)) {
                const int value = rest * 10 + digit;
                if (!quotient.isEmpty() || value / 900 > 0)
                    quotient << value / 900;
                rest = value % 900;
            }
            group.prepend(rest);
            number = quotient;
        }
        out << group;
    }
    return out;
}

QVector<int> pdf417Bytes(const QByteArray &bytes)
{
    QVector<int> out{bytes.size() % 6 == 0 ? kLatchByteSix : kLatchByte};
    int i = 0;
    for (; i + 6 <= bytes.size(); i += 6) {
        quint64 value = 0;
        for (int j = 0; j < 6; ++j)
            value = value * 256 + static_cast<unsigned char>(bytes.at(i + j));
        int group[5];
        for (int j = 4; j >= 0; --j) {
            group[j] = int(value % 900);
            value /= 900;
        }
        for (int v : group)
            out << v;
    }
    for (; i < bytes.size(); ++i)
        out << static_cast<unsigned char>(bytes.at(i));
    return out;
}

QVector<int> pdf417ErrorCorrection(const QVector<int> &data, int count)
{
    // Generator: (x - 3)(x - 3^2)...(x - 3^count), lowest degree first, monic.
    QVector<int> generator(count + 1, 0);
    generator[0] = 1;
    int power = 1;
    for (int i = 1; i <= count; ++i) {
        power = power * 3 % kPdf417Modulus;
        for (int j = i; j > 0; --j)
            generator[j] = (generator[j - 1]
                            + (kPdf417Modulus - power) * generator[j] % kPdf417Modulus) % kPdf417Modulus;
        generator[0] = (kPdf417Modulus - power) * generator[0] % kPdf417Modulus;
    }
    QVector<int> rest(count, 0);
    for (int value : data) {
        const int feedback = (value + rest.last()) % kPdf417Modulus;
        for (int j = count - 1; j > 0; --j)
            rest[j] = (rest[j - 1] + kPdf417Modulus - feedback * generator[j] % kPdf417Modulus)
                      % kPdf417Modulus;
        rest[0] = (kPdf417Modulus - feedback * generator[0] % kPdf417Modulus) % kPdf417Modulus;
    }
    QVector<int> ecc;
    for (int j = count - 1; j >= 0; --j)
        ecc << (kPdf417Modulus - rest.at(j)) % kPdf417Modulus;
    return ecc;
}

int pdf417Level(int dataCodewords)
{
    if (dataCodewords <= 40)
        return 2;
    if (dataCodewords <= 160)
        return 3;
    if (dataCodewords <= 320)
        return 4;
    return 5;
}

} // namespace

QVector<int> BrDataMatrixEncoder::codewords(const QString &text)
{
    const QVector<int> data = dataMatrixAscii(text.toUtf8());
    const DataMatrixSize *size = dataMatrixSizeFor(int(data.size()));
    return size ? dataMatrixCodewords(data, *size) : QVector<int>();
}

BrMatrixCode BrDataMatrixEncoder::encode(const QString &text)
{
    if (text.isEmpty())
        return failed(QStringLiteral("empty data"));
    const QVector<int> data = dataMatrixAscii(text.toUtf8());
    const DataMatrixSize *size = dataMatrixSizeFor(int(data.size()));
    if (!size)
        return failed(QStringLiteral("too much data for a Data Matrix symbol"));

    const QVector<int> all = dataMatrixCodewords(data, *size);
    const int region = size->size / size->regions - 2;
    const DataMatrixPlacement placement(all, region * size->regions);

    BrMatrixCode code;
    code.width = code.height = size->size;
    code.modules.resize(size->size * size->size);
    const int cell = region + 2;
    for (int y = 0; y < size->size; ++y) {
        for (int x = 0; x < size->size; ++x) {
            const int rx = x % cell, ry = y % cell;
            bool dark;
            if (rx == 0 || ry == cell - 1) // solid edges of the finder pattern
                dark = true;
            else if (ry == 0) // timing patterns
                dark = rx % 2 == 0;
            else if (rx == cell - 1)
                dark = ry % 2 == 1;
            else
                dark = placement.isDark(y / cell * region + ry - 1, x / cell * region + rx - 1);
            code.modules[y * size->size + x] = dark;
        }
    }
    return code;
}

BrMatrixCode BrPdf417Encoder::encode(const QString &text, double aspect, int securityLevel)
{
    if (text.isEmpty())
        return failed(QStringLiteral("empty data"));
    const QByteArray bytes = text.toUtf8();
    const bool numeric = std::all_of(bytes.begin(), bytes.end(), [](char c) { return c >= '0' && c <= '9'; });
    const QVector<int> data = numeric ? pdf417Numeric(bytes) : pdf417Bytes(bytes);

    const int level = securityLevel >= 0 ? std::min(securityLevel, 8) : pdf417Level(int(data.size()));
    const int eccCount = 2 << level;
    const int used = 1 + int(data.size()) + eccCount; // the first codeword is the length
    if (used > 928)
        return failed(QStringLiteral("too much data for a PDF417 symbol"));

    // The number of columns that gets closest to the shape asked for.
    int columns = 0, rows = 0;
    double best = 0.0;
    for (int c = 1; c <= 30; ++c) {
        const int r = std::max(3, (used + c - 1) / c);
        if (r > 90 || r * c > 928)
            continue;
        const double shape = double(17 * c + 69) / (r * kPdf417RowHeight);
        const double distance = std::abs(std::log(shape / aspect));
        if (columns == 0 || distance < best) {
            columns = c;
            rows = r;
            best = distance;
        }
    }
    if (columns == 0)
        return failed(QStringLiteral("too much data for a PDF417 symbol"));

    QVector<int> all{columns * rows - eccCount};
    all << data;
    while (all.size() < columns * rows - eccCount)
        all << kPad;
    all << pdf417ErrorCorrection(all, eccCount);

    BrMatrixCode code;
    code.width = 17 + 17 + 17 * columns + 17 + 18; // start, indicators, data, stop
    code.height = rows * kPdf417RowHeight;
    code.modules.resize(code.width * code.height);
    for (int row = 0; row < rows; ++row) {
        const int cluster = row % 3;
        const int base = 30 * (row / 3);
        const int rowCount = (rows - 1) / 3;
        const int security = level * 3 + (rows - 1) % 3;
        const int lastColumn = columns - 1;
        const int left = base + (cluster == 0 ? rowCount : cluster == 1 ? security : lastColumn);
        const int right = base + (cluster == 0 ? lastColumn : cluster == 1 ? rowCount : security);

        QVector<bool> line;
        const auto put = [&line](int pattern, int length) {
            for (int bit = length - 1; bit >= 0; --bit)
                line << ((pattern >> bit) & 1);
        };
        put(kPdf417Start, 17);
        put(int(kPdf417Patterns[cluster][left]), 17);
        for (int c = 0; c < columns; ++c)
            put(int(kPdf417Patterns[cluster][all.at(row * columns + c)]), 17);
        put(int(kPdf417Patterns[cluster][right]), 17);
        put(kPdf417Stop, 18);

        for (int repeat = 0; repeat < kPdf417RowHeight; ++repeat)
            std::copy(line.begin(), line.end(),
                      code.modules.begin() + (row * kPdf417RowHeight + repeat) * code.width);
    }
    return code;
}
