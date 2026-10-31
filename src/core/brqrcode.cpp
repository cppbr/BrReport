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

#include "brqrcode.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdint>
#include <cstdlib>

namespace {

// Error correction codewords per block, indexed [ecc][version] (index 0 unused).
// ECC order: Low, Medium, Quartile, High.
const int8_t kEccCodewordsPerBlock[4][41] = {
    {-1, 7,  10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
     28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
     26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
     28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
     30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
};

// Number of error correction blocks, indexed [ecc][version].
const int8_t kNumErrorCorrectionBlocks[4][41] = {
    {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8,
     8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
     17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20,
     23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
     25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};

const char *const kAlphanumeric = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

enum class Mode { Numeric = 0x1, Alphanumeric = 0x2, Byte = 0x4 };

int eccIndex(Br::QrErrorCorrection e)
{
    return int(e);
}

int formatBitsOf(Br::QrErrorCorrection e)
{
    switch (e) {
    case Br::QrErrorCorrection::Low:
        return 1;
    case Br::QrErrorCorrection::Medium:
        return 0;
    case Br::QrErrorCorrection::Quartile:
        return 3;
    case Br::QrErrorCorrection::High:
        return 2;
    }
    return 0;
}

int charCountBits(Mode mode, int version)
{
    const int range = version <= 9 ? 0 : (version <= 26 ? 1 : 2);
    switch (mode) {
    case Mode::Numeric: {
        const int bits[] = {10, 12, 14};
        return bits[range];
    }
    case Mode::Alphanumeric: {
        const int bits[] = {9, 11, 13};
        return bits[range];
    }
    case Mode::Byte: {
        const int bits[] = {8, 16, 16};
        return bits[range];
    }
    }
    return 0;
}

int numRawDataModules(int version)
{
    int result = (16 * version + 128) * version + 64;
    if (version >= 2) {
        const int numAlign = version / 7 + 2;
        result -= (25 * numAlign - 10) * numAlign - 55;
        if (version >= 7)
            result -= 36;
    }
    return result;
}

int numDataCodewords(int version, Br::QrErrorCorrection e)
{
    const int i = eccIndex(e);
    return numRawDataModules(version) / 8
           - kEccCodewordsPerBlock[i][version] * kNumErrorCorrectionBlocks[i][version];
}

class BitBuffer
{
public:
    void append(uint32_t value, int bits)
    {
        for (int i = bits - 1; i >= 0; --i)
            m_bits.append(((value >> i) & 1) != 0);
    }
    int size() const { return int(m_bits.size()); }
    bool at(int i) const { return m_bits.at(i); }

private:
    QVector<bool> m_bits;
};

// --- Reed-Solomon over GF(2^8), primitive polynomial 0x11D -----------------

uint8_t gfMultiply(uint8_t x, uint8_t y)
{
    int z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return uint8_t(z);
}

QVector<uint8_t> rsDivisor(int degree)
{
    QVector<uint8_t> result(degree, 0);
    result[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[j] = gfMultiply(result[j], root);
            if (j + 1 < degree)
                result[j] ^= result[j + 1];
        }
        root = gfMultiply(root, 0x02);
    }
    return result;
}

QVector<uint8_t> rsRemainder(const QVector<uint8_t> &data, const QVector<uint8_t> &divisor)
{
    QVector<uint8_t> result(divisor.size(), 0);
    for (uint8_t b : data) {
        const uint8_t factor = b ^ result.first();
        result.removeFirst();
        result.append(0);
        for (int i = 0; i < result.size(); ++i)
            result[i] ^= gfMultiply(divisor.at(i), factor);
    }
    return result;
}

// --- Symbol construction ----------------------------------------------------

class Symbol
{
public:
    Symbol(int version) : version(version), size(version * 4 + 17)
    {
        modules.fill(false, size * size);
        isFunction.fill(false, size * size);
    }

    bool get(int x, int y) const { return modules.at(y * size + x); }
    void setFunction(int x, int y, bool dark)
    {
        modules[y * size + x] = dark;
        isFunction[y * size + x] = true;
    }

    QVector<int> alignmentPositions() const
    {
        if (version == 1)
            return {};
        const int numAlign = version / 7 + 2;
        const int step = version == 32 ? 26 : (version * 4 + numAlign * 2 + 1) / (numAlign * 2 - 2) * 2;
        QVector<int> result;
        for (int i = 0, pos = size - 7; i < numAlign - 1; ++i, pos -= step)
            result.prepend(pos);
        result.prepend(6);
        return result;
    }

    void drawFunctionPatterns()
    {
        for (int i = 0; i < size; ++i) {
            setFunction(6, i, i % 2 == 0);
            setFunction(i, 6, i % 2 == 0);
        }
        drawFinder(3, 3);
        drawFinder(size - 4, 3);
        drawFinder(3, size - 4);

        const QVector<int> pos = alignmentPositions();
        const int n = int(pos.size());
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                if (!((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0)))
                    drawAlignment(pos.at(i), pos.at(j));

        drawFormatBits(0, Br::QrErrorCorrection::Medium); // reserve; redrawn later
        drawVersion();
    }

    void drawFinder(int x, int y)
    {
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                const int dist = std::max(std::abs(dx), std::abs(dy));
                const int xx = x + dx, yy = y + dy;
                if (xx >= 0 && xx < size && yy >= 0 && yy < size)
                    setFunction(xx, yy, dist != 2 && dist != 4);
            }
        }
    }

    void drawAlignment(int x, int y)
    {
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                setFunction(x + dx, y + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }

    void drawFormatBits(int mask, Br::QrErrorCorrection e)
    {
        const int data = formatBitsOf(e) << 3 | mask;
        int rem = data;
        for (int i = 0; i < 10; ++i)
            rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        const int bits = (data << 10 | rem) ^ 0x5412;
        auto bit = [bits](int i) { return ((bits >> i) & 1) != 0; };

        for (int i = 0; i <= 5; ++i)
            setFunction(8, i, bit(i));
        setFunction(8, 7, bit(6));
        setFunction(8, 8, bit(7));
        setFunction(7, 8, bit(8));
        for (int i = 9; i < 15; ++i)
            setFunction(14 - i, 8, bit(i));

        for (int i = 0; i < 8; ++i)
            setFunction(size - 1 - i, 8, bit(i));
        for (int i = 8; i < 15; ++i)
            setFunction(8, size - 15 + i, bit(i));
        setFunction(8, size - 8, true); // always dark
    }

    void drawVersion()
    {
        if (version < 7)
            return;
        int rem = version;
        for (int i = 0; i < 12; ++i)
            rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        const long bits = long(version) << 12 | rem;
        for (int i = 0; i < 18; ++i) {
            const bool b = ((bits >> i) & 1) != 0;
            const int a = size - 11 + i % 3;
            const int c = i / 3;
            setFunction(a, c, b);
            setFunction(c, a, b);
        }
    }

    void drawCodewords(const QVector<uint8_t> &data)
    {
        int i = 0;
        const int totalBits = int(data.size()) * 8;
        for (int right = size - 1; right >= 1; right -= 2) {
            if (right == 6)
                right = 5;
            for (int vert = 0; vert < size; ++vert) {
                for (int j = 0; j < 2; ++j) {
                    const int x = right - j;
                    const bool upward = ((right + 1) & 2) == 0;
                    const int y = upward ? size - 1 - vert : vert;
                    if (!isFunction.at(y * size + x) && i < totalBits) {
                        modules[y * size + x] = ((data.at(i >> 3) >> (7 - (i & 7))) & 1) != 0;
                        ++i;
                    }
                }
            }
        }
    }

    void applyMask(int mask)
    {
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                bool invert = false;
                switch (mask) {
                case 0: invert = (x + y) % 2 == 0; break;
                case 1: invert = y % 2 == 0; break;
                case 2: invert = x % 3 == 0; break;
                case 3: invert = (x + y) % 3 == 0; break;
                case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
                case 5: invert = x * y % 2 + x * y % 3 == 0; break;
                case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
                case 7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
                }
                if (invert && !isFunction.at(y * size + x))
                    modules[y * size + x] = !modules[y * size + x];
            }
        }
    }

    long penalty() const
    {
        long result = 0;
        // N1 (runs of 5+) and N3 (finder-like patterns) in rows and columns.
        for (int pass = 0; pass < 2; ++pass) {
            for (int a = 0; a < size; ++a) {
                auto at = [&](int b) { return pass == 0 ? get(b, a) : get(a, b); };
                int run = 1;
                for (int b = 1; b < size; ++b) {
                    if (at(b) == at(b - 1)) {
                        ++run;
                    } else {
                        if (run >= 5)
                            result += 3 + (run - 5);
                        run = 1;
                    }
                }
                if (run >= 5)
                    result += 3 + (run - 5);

                // 1:1:3:1:1 dark pattern with 4 light modules on one side.
                for (int b = 0; b + 11 <= size; ++b) {
                    static const bool p1[11] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
                    static const bool p2[11] = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
                    bool m1 = true, m2 = true;
                    for (int k = 0; k < 11; ++k) {
                        const bool v = at(b + k);
                        m1 = m1 && v == p1[k];
                        m2 = m2 && v == p2[k];
                    }
                    if (m1)
                        result += 40;
                    if (m2)
                        result += 40;
                }
            }
        }
        // N2: 2x2 blocks of the same color.
        for (int y = 0; y < size - 1; ++y)
            for (int x = 0; x < size - 1; ++x) {
                const bool c = get(x, y);
                if (c == get(x + 1, y) && c == get(x, y + 1) && c == get(x + 1, y + 1))
                    result += 3;
            }
        // N4: balance of dark and light modules.
        int dark = 0;
        for (bool m : modules)
            if (m)
                ++dark;
        const int total = size * size;
        const int k = (std::abs(dark * 20 - total * 10) + total - 1) / total - 1;
        result += std::max(0, k) * 10;
        return result;
    }

    int version;
    int size;
    QVector<bool> modules;
    QVector<bool> isFunction;
};

QVector<uint8_t> addEccAndInterleave(const QVector<uint8_t> &data, int version,
                                     Br::QrErrorCorrection e)
{
    const int i = eccIndex(e);
    const int numBlocks = kNumErrorCorrectionBlocks[i][version];
    const int blockEccLen = kEccCodewordsPerBlock[i][version];
    const int rawCodewords = numRawDataModules(version) / 8;
    const int numShortBlocks = numBlocks - rawCodewords % numBlocks;
    const int shortBlockLen = rawCodewords / numBlocks;

    const QVector<uint8_t> divisor = rsDivisor(blockEccLen);
    QVector<QVector<uint8_t>> blocks;
    for (int b = 0, k = 0; b < numBlocks; ++b) {
        const int datLen = shortBlockLen - blockEccLen + (b < numShortBlocks ? 0 : 1);
        QVector<uint8_t> dat = data.mid(k, datLen);
        k += datLen;
        const QVector<uint8_t> ecc = rsRemainder(dat, divisor);
        if (b < numShortBlocks)
            dat.append(0); // placeholder, skipped when interleaving
        dat += ecc;
        blocks.append(dat);
    }

    QVector<uint8_t> result;
    for (int c = 0; c < blocks.first().size(); ++c)
        for (int b = 0; b < blocks.size(); ++b)
            if (c != shortBlockLen - blockEccLen || b >= numShortBlocks)
                result.append(blocks.at(b).at(c));
    return result;
}

BrQrEncoder::Result encodeSegment(Mode mode, int numChars, const BitBuffer &payload,
                                  Br::QrErrorCorrection e, int minVersion, int forcedMask)
{
    BrQrEncoder::Result r;
    r.errorCorrection = e;
    if (forcedMask < -1 || forcedMask > 7) {
        r.error = QStringLiteral("QR Code: invalid mask");
        return r;
    }

    int version = qBound(1, minVersion, 40);
    int usedBits = 0;
    for (;; ++version) {
        if (version > 40) {
            r.error = QStringLiteral("QR Code: data too long");
            return r;
        }
        const int ccBits = charCountBits(mode, version);
        if (numChars < (1 << ccBits)) {
            usedBits = 4 + ccBits + payload.size();
            if (usedBits <= numDataCodewords(version, e) * 8)
                break;
        }
    }

    BitBuffer bb;
    bb.append(uint32_t(mode), 4);
    bb.append(uint32_t(numChars), charCountBits(mode, version));
    for (int i = 0; i < payload.size(); ++i)
        bb.append(payload.at(i) ? 1 : 0, 1);

    const int capacityBits = numDataCodewords(version, e) * 8;
    bb.append(0, std::min(4, capacityBits - bb.size()));
    bb.append(0, (8 - bb.size() % 8) % 8);
    for (uint8_t pad = 0xEC; bb.size() < capacityBits; pad ^= 0xEC ^ 0x11)
        bb.append(pad, 8);

    QVector<uint8_t> dataCodewords(bb.size() / 8, 0);
    for (int i = 0; i < bb.size(); ++i)
        if (bb.at(i))
            dataCodewords[i >> 3] |= uint8_t(1 << (7 - (i & 7)));

    Symbol sym(version);
    sym.drawFunctionPatterns();
    sym.drawCodewords(addEccAndInterleave(dataCodewords, version, e));

    int mask = forcedMask;
    if (mask < 0) {
        long best = LONG_MAX;
        for (int m = 0; m < 8; ++m) {
            sym.applyMask(m);
            sym.drawFormatBits(m, e);
            const long p = sym.penalty();
            if (p < best) {
                best = p;
                mask = m;
            }
            sym.applyMask(m); // XOR again to undo
        }
    }
    sym.applyMask(mask);
    sym.drawFormatBits(mask, e);

    r.version = version;
    r.size = sym.size;
    r.mask = mask;
    r.modules = sym.modules;
    return r;
}

} // namespace

BrQrEncoder::Result BrQrEncoder::encode(const QString &text, Br::QrErrorCorrection ecc,
                                        int minVersion, int mask)
{
    bool numeric = !text.isEmpty();
    bool alnum = !text.isEmpty();
    for (QChar c : text) {
        const ushort u = c.unicode();
        if (u < '0' || u > '9')
            numeric = false;
        if (u > 127 || !std::strchr(kAlphanumeric, char(u)) || u == 0)
            alnum = false;
    }

    BitBuffer payload;
    if (numeric) {
        for (int i = 0; i < text.size(); i += 3) {
            const QString group = text.mid(i, 3);
            payload.append(uint32_t(group.toUInt()), int(group.size()) * 3 + 1);
        }
        return encodeSegment(Mode::Numeric, int(text.size()), payload, ecc, minVersion, mask);
    }
    if (alnum) {
        auto value = [](QChar c) {
            return uint32_t(std::strchr(kAlphanumeric, char(c.unicode())) - kAlphanumeric);
        };
        int i = 0;
        for (; i + 1 < text.size(); i += 2)
            payload.append(value(text.at(i)) * 45 + value(text.at(i + 1)), 11);
        if (i < text.size())
            payload.append(value(text.at(i)), 6);
        return encodeSegment(Mode::Alphanumeric, int(text.size()), payload, ecc, minVersion,
                             mask);
    }
    return encodeBytes(text.toUtf8(), ecc, minVersion, mask);
}

BrQrEncoder::Result BrQrEncoder::encodeBytes(const QByteArray &data, Br::QrErrorCorrection ecc,
                                             int minVersion, int mask)
{
    BitBuffer payload;
    for (char c : data)
        payload.append(uint8_t(c), 8);
    return encodeSegment(Mode::Byte, int(data.size()), payload, ecc, minVersion, mask);
}
