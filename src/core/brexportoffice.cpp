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

// Export to formats edited by other programs: plain text, XLSX and RTF.

#include "brexport.h"

#include "brrenderer.h"
#include "brtextmetrics.h"

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <cmath>

namespace {

/*! The text of a page as single lines: the lines of rich text are taken from its layout. */
QList<BrPageItem> textLines(const BrPage &page)
{
    QList<BrPageItem> lines;
    for (const BrPageItem &item : page.items) {
        if (item.text.isEmpty() || !qFuzzyIsNull(item.angle))
            continue; // rotated text is a watermark or a decoration
        if (item.kind == BrPageItem::Text) {
            lines << item;
        } else if (item.kind == BrPageItem::RichText) {
            const auto doc = BrTextMetrics::richDocument(item.font, item.text, item.rect.width());
            doc->size(); // lays out the whole document
            const double mmPerPixel = 25.4 / BrTextMetrics::referenceDpi();
            for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
                const double blockTop = doc->documentLayout()->blockBoundingRect(block).top() * mmPerPixel;
                const QTextLayout *layout = block.layout();
                for (int i = 0; i < layout->lineCount(); ++i) {
                    const QTextLine line = layout->lineAt(i);
                    const double top = blockTop + line.y() * mmPerPixel - item.textOffset;
                    // A band split across pages has a part of the text on each one.
                    if (top < -0.01 || top >= item.rect.height() - 0.01)
                        continue;
                    const QString text = block.text().mid(line.textStart(), line.textLength()).trimmed();
                    if (!text.isEmpty())
                        lines << BrPageItem::makeText(
                            QRectF(item.rect.left(), item.rect.top() + top, item.rect.width(),
                                   line.height() * mmPerPixel),
                            text, item.font, item.textColor, item.alignment);
                }
            }
        }
    }
    return lines;
}

bool writeFile(const QString &fileName, const QByteArray &data)
{
    QSaveFile file(fileName);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

// ---------------------------------------------------------------------------
// ZIP (the container of XLSX files), without compression
// ---------------------------------------------------------------------------

quint32 crc32Of(const QByteArray &data)
{
    static const QVector<quint32> table = []() {
        QVector<quint32> t(256);
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[int(i)] = c;
        }
        return t;
    }();
    quint32 crc = 0xFFFFFFFFu;
    for (char byte : data)
        crc = table.at(int((crc ^ static_cast<unsigned char>(byte)) & 0xFF)) ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

class ZipWriter
{
public:
    void add(const QString &name, const QByteArray &data)
    {
        const QByteArray path = name.toUtf8();
        const quint32 crc = crc32Of(data);
        const auto offset = quint32(m_out.size());
        const auto common = [&](QByteArray &to) {
            le(to, 20, 2);                 // version needed
            le(to, 0x0800, 2);             // file names in UTF-8
            le(to, 0, 2);                  // stored
            le(to, 0, 2);                  // time
            le(to, 0x5C61, 2);             // date
            le(to, crc, 4);
            le(to, quint32(data.size()), 4);
            le(to, quint32(data.size()), 4);
            le(to, quint32(path.size()), 2);
            le(to, 0, 2);                  // extra field
        };
        le(m_out, 0x04034B50, 4);
        common(m_out);
        m_out += path;
        m_out += data;

        le(m_directory, 0x02014B50, 4);
        le(m_directory, 20, 2);            // version made by
        common(m_directory);
        le(m_directory, 0, 2);             // comment
        le(m_directory, 0, 2);             // disk
        le(m_directory, 0, 2);             // internal attributes
        le(m_directory, 0, 4);             // external attributes
        le(m_directory, offset, 4);
        m_directory += path;
        ++m_count;
    }

    QByteArray finish()
    {
        const auto start = quint32(m_out.size());
        QByteArray out = m_out + m_directory;
        le(out, 0x06054B50, 4);
        le(out, 0, 2);
        le(out, 0, 2);
        le(out, quint32(m_count), 2);
        le(out, quint32(m_count), 2);
        le(out, quint32(m_directory.size()), 4);
        le(out, start, 4);
        le(out, 0, 2);
        return out;
    }

private:
    static void le(QByteArray &to, quint32 value, int bytes)
    {
        for (int i = 0; i < bytes; ++i)
            to += char((value >> (8 * i)) & 0xFF);
    }

    QByteArray m_out;
    QByteArray m_directory;
    int m_count = 0;
};

// ---------------------------------------------------------------------------
// XLSX
// ---------------------------------------------------------------------------

QString columnName(int index)
{
    QString name;
    for (++index; index > 0; index = (index - 1) / 26)
        name.prepend(QLatin1Char(char('A' + (index - 1) % 26)));
    return name;
}

QString xmlText(const QString &text)
{
    QString out;
    for (const QChar c : text)
        if (c.unicode() >= 0x20 || c == QLatin1Char('\t'))
            out += c;
    return out.toHtmlEscaped();
}

/*!
 * The value of \a text if it reads as a number, in the styles that reports print
 * ("1.234,56", "1,234.56", "1234"). Codes with leading zeros stay as text.
 */
bool parseNumber(QString text, double *value)
{
    static const QRegularExpression plain(QStringLiteral("^-?(0|[1-9]\\d*)([.,]\\d+)?$"));
    static const QRegularExpression commaDecimal(QStringLiteral("^-?[1-9]\\d{0,2}(\\.\\d{3})+(,\\d+)?$"));
    static const QRegularExpression pointDecimal(QStringLiteral("^-?[1-9]\\d{0,2}(,\\d{3})+(\\.\\d+)?$"));
    text = text.trimmed();
    if (commaDecimal.match(text).hasMatch())
        text = text.remove(QLatin1Char('.')).replace(QLatin1Char(','), QLatin1Char('.'));
    else if (pointDecimal.match(text).hasMatch())
        text.remove(QLatin1Char(','));
    else if (plain.match(text).hasMatch())
        text.replace(QLatin1Char(','), QLatin1Char('.'));
    else
        return false;
    bool ok = false;
    *value = text.toDouble(&ok);
    return ok;
}

const char kContentTypes[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/></Types>)";
const char kRootRelations[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)";
const char kWorkbook[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Report" sheetId="1" r:id="rId1"/></sheets></workbook>)";
const char kWorkbookRelations[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>)";
const char kStyles[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="2"><font><sz val="10"/><name val="Arial"/></font><font><b/><sz val="10"/><name val="Arial"/></font></fonts><fills count="2"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill></fills><borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="4"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/><xf numFmtId="0" fontId="1" fillId="0" borderId="0" xfId="0" applyFont="1"/><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0" applyAlignment="1"><alignment horizontal="right"/></xf><xf numFmtId="0" fontId="1" fillId="0" borderId="0" xfId="0" applyFont="1" applyAlignment="1"><alignment horizontal="right"/></xf></cellXfs></styleSheet>)";

// ---------------------------------------------------------------------------
// RTF
// ---------------------------------------------------------------------------

int twips(double mm)
{
    return qRound(mm * 1440.0 / 25.4);
}

QByteArray rtfText(const QString &text)
{
    QByteArray out;
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (u == '\\' || u == '{' || u == '}') {
            out += '\\';
            out += char(u);
        } else if (u >= 0x20 && u < 0x80) {
            out += char(u);
        } else if (u >= 0x20) {
            out += "\\u" + QByteArray::number(short(u)) + '?';
        }
    }
    return out;
}

/*! Fonts and colors of the document, which RTF lists in tables at the start. */
class RtfTables
{
public:
    int font(const QString &family)
    {
        const int index = int(m_fonts.indexOf(family));
        if (index >= 0)
            return index;
        m_fonts << family;
        return int(m_fonts.size()) - 1;
    }
    int color(const QColor &c)
    {
        const QRgb rgb = c.rgb() & 0xFFFFFF;
        const int index = int(m_colors.indexOf(rgb));
        if (index >= 0)
            return index + 1; // color 0 is the automatic one
        m_colors << rgb;
        return int(m_colors.size());
    }
    QByteArray header() const
    {
        QByteArray out = "{\\fonttbl";
        for (int i = 0; i < m_fonts.size(); ++i)
            out += "{\\f" + QByteArray::number(i) + ' ' + rtfText(m_fonts.at(i)) + ";}";
        out += "}{\\colortbl;";
        for (QRgb rgb : m_colors)
            out += "\\red" + QByteArray::number(qRed(rgb)) + "\\green" + QByteArray::number(qGreen(rgb))
                   + "\\blue" + QByteArray::number(qBlue(rgb)) + ';';
        out += "}\n";
        return out;
    }

private:
    QStringList m_fonts;
    QList<QRgb> m_colors;
};

/*! A drawing object positioned on the page. */
QByteArray rtfShape(const QRectF &r, int type, const QList<QPair<const char *, int>> &properties, int order)
{
    QByteArray out = "{\\shp{\\*\\shpinst\\shpleft" + QByteArray::number(twips(r.left()))
                     + "\\shptop" + QByteArray::number(twips(r.top()))
                     + "\\shpright" + QByteArray::number(twips(r.right()))
                     + "\\shpbottom" + QByteArray::number(twips(r.bottom()))
                     + "\\shpfhdr0\\shpbxpage\\shpbxignore\\shpbypage\\shpbyignore\\shpwr3\\shpfblwtxt1\\shpz"
                     + QByteArray::number(order)
                     + "{\\sp{\\sn shapeType}{\\sv " + QByteArray::number(type) + "}}";
    for (const auto &p : properties)
        out += QByteArray("{\\sp{\\sn ") + p.first + "}{\\sv " + QByteArray::number(p.second) + "}}";
    return out + "}}\n";
}

int rtfColor(const QColor &c)
{
    return c.red() | (c.green() << 8) | (c.blue() << 16);
}

QByteArray rtfShapeOf(const BrPageItem &item, int order)
{
    const bool filled = item.brushColor.isValid() && item.kind != BrPageItem::Line;
    const bool outlined = item.penStyle != Qt::NoPen && item.penColor.isValid();
    QList<QPair<const char *, int>> properties{{"fFilled", filled ? 1 : 0}, {"fLine", outlined ? 1 : 0}};
    if (filled)
        properties << qMakePair("fillColor", rtfColor(item.brushColor));
    if (outlined) {
        properties << qMakePair("lineColor", rtfColor(item.penColor));
        properties << qMakePair("lineWidth", qRound(std::max(item.penWidth, 0.1) * 36000.0));
    }
    if (item.kind == BrPageItem::Line) {
        const QPointF a = item.points.at(0), b = item.points.at(1);
        if ((a.x() < b.x()) != (a.y() < b.y()) && a.x() != b.x() && a.y() != b.y())
            properties << qMakePair("fFlipV", 1);
        return rtfShape(QRectF(a, b).normalized(), 20, properties, order);
    }
    return rtfShape(item.rect, item.kind == BrPageItem::Ellipse ? 3 : 1, properties, order);
}

QByteArray rtfImage(const QRectF &r, const QImage &image)
{
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return "{\\pard\\pvpg\\phpg\\posx" + QByteArray::number(twips(r.left())) + "\\posy"
           + QByteArray::number(twips(r.top())) + "\\absw" + QByteArray::number(twips(r.width()))
           + "{\\pict\\pngblip\\picw" + QByteArray::number(image.width()) + "\\pich"
           + QByteArray::number(image.height()) + "\\picwgoal" + QByteArray::number(twips(r.width()))
           + "\\pichgoal" + QByteArray::number(twips(r.height())) + ' ' + png.toHex() + "}\\par}\n";
}

QByteArray rtfParagraph(const BrPageItem &line, RtfTables &tables)
{
    const QRectF &r = line.rect;
    const char *align = (line.alignment & Qt::AlignHCenter) ? "\\qc"
                        : (line.alignment & Qt::AlignRight) ? "\\qr"
                                                            : "\\ql";
    QByteArray out = "{\\pard\\pvpg\\phpg\\posx" + QByteArray::number(twips(r.left())) + "\\posy"
                     + QByteArray::number(twips(r.top())) + "\\absw" + QByteArray::number(twips(r.width()) + 20)
                     + "\\absh-" + QByteArray::number(std::max(twips(r.height()), 100)) + align
                     + "\\f" + QByteArray::number(tables.font(line.font.family())) + "\\fs"
                     + QByteArray::number(qRound(line.font.pointSizeF() * 2.0)) + "\\cf"
                     + QByteArray::number(tables.color(line.textColor));
    if (line.font.bold())
        out += "\\b";
    if (line.font.italic())
        out += "\\i";
    if (line.font.underline())
        out += "\\ul";
    return out + ' ' + rtfText(line.text) + "\\par}\n";
}

} // namespace

QString BrExport::toText(const BrDocument &doc, double charWidthMm, double lineHeightMm)
{
    QStringList pages;
    for (const BrPage &page : doc.pages) {
        QMap<int, QString> rows;
        for (const BrPageItem &line : textLines(page)) {
            const int row = int(line.rect.center().y() / lineHeightMm);
            int column = qRound(line.rect.left() / charWidthMm);
            if (line.alignment & Qt::AlignRight)
                column = std::max(0, qRound(line.rect.right() / charWidthMm) - int(line.text.size()));
            else if (line.alignment & Qt::AlignHCenter)
                column = std::max(0, qRound(line.rect.center().x() / charWidthMm) - int(line.text.size()) / 2);
            QString &text = rows[row];
            if (text.size() < column + line.text.size())
                text = text.leftJustified(column + int(line.text.size()));
            text.replace(column, line.text.size(), line.text);
        }
        QStringList lines;
        if (!rows.isEmpty())
            for (int row = 0; row <= rows.lastKey(); ++row) {
                QString text = rows.value(row);
                while (text.endsWith(QLatin1Char(' ')))
                    text.chop(1);
                lines << text;
            }
        pages << lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    }
    return pages.join(QLatin1Char('\f'));
}

bool BrExport::toTextFile(const BrDocument &doc, const QString &fileName, double charWidthMm, double lineHeightMm)
{
    return !doc.isEmpty() && writeFile(fileName, toText(doc, charWidthMm, lineHeightMm).toUtf8());
}

bool BrExport::toXlsx(const BrDocument &doc, const QString &fileName)
{
    if (doc.isEmpty())
        return false;
    const double tolerance = 1.0; // mm: objects closer than this share a column or a row

    // Columns: where the objects start, in all the pages.
    QList<double> lefts;
    QList<QList<BrPageItem>> pages;
    for (const BrPage &page : doc.pages) {
        pages << textLines(page);
        for (const BrPageItem &line : std::as_const(pages.last()))
            lefts << line.rect.left();
    }
    std::sort(lefts.begin(), lefts.end());
    QList<double> columns;
    for (double left : std::as_const(lefts))
        if (columns.isEmpty() || left - columns.last() > tolerance)
            columns << left;
    const auto columnOf = [&columns, tolerance](double left) {
        const auto it = std::upper_bound(columns.begin(), columns.end(), left + tolerance);
        return std::max(0, int(it - columns.begin()) - 1);
    };

    QByteArray sheet = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                       "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">";
    if (!columns.isEmpty()) {
        sheet += "<cols>";
        for (int i = 0; i < columns.size(); ++i) {
            const double next = i + 1 < columns.size() ? columns.at(i + 1) : columns.at(i) + 30.0;
            sheet += QStringLiteral("<col min=\"%1\" max=\"%1\" width=\"%2\" customWidth=\"1\"/>")
                         .arg(i + 1)
                         .arg(std::max(3.0, (next - columns.at(i)) * 0.55), 0, 'f', 1)
                         .toUtf8();
        }
        sheet += "</cols>";
    }
    sheet += "<sheetData>";
    int rowNumber = 0;
    for (QList<BrPageItem> &lines : pages) {
        std::stable_sort(lines.begin(), lines.end(), [](const BrPageItem &a, const BrPageItem &b) {
            return a.rect.center().y() < b.rect.center().y();
        });
        for (int i = 0; i < lines.size();) {
            // Objects at the same height make a row.
            const double top = lines.at(i).rect.center().y();
            int end = i;
            while (end < lines.size() && lines.at(end).rect.center().y() - top <= tolerance)
                ++end;
            std::stable_sort(lines.begin() + i, lines.begin() + end, [](const BrPageItem &a, const BrPageItem &b) {
                return a.rect.left() < b.rect.left();
            });
            ++rowNumber;
            sheet += "<row r=\"" + QByteArray::number(rowNumber) + "\">";
            int lastColumn = -1;
            for (; i < end; ++i) {
                const BrPageItem &line = lines.at(i);
                const int column = std::max(columnOf(line.rect.left()), lastColumn + 1);
                lastColumn = column;
                const QByteArray cell = (columnName(column) + QString::number(rowNumber)).toLatin1();
                const int style = (line.font.bold() ? 1 : 0) + ((line.alignment & Qt::AlignRight) ? 2 : 0);
                double number = 0.0;
                if (parseNumber(line.text, &number))
                    sheet += "<c r=\"" + cell + "\" s=\"" + QByteArray::number(style) + "\"><v>"
                             + QByteArray::number(number, 'g', 15) + "</v></c>";
                else
                    sheet += "<c r=\"" + cell + "\" s=\"" + QByteArray::number(style)
                             + "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">" + xmlText(line.text).toUtf8()
                             + "</t></is></c>";
            }
            sheet += "</row>";
        }
    }
    sheet += "</sheetData></worksheet>";

    ZipWriter zip;
    zip.add(QStringLiteral("[Content_Types].xml"), kContentTypes);
    zip.add(QStringLiteral("_rels/.rels"), kRootRelations);
    zip.add(QStringLiteral("xl/workbook.xml"), kWorkbook);
    zip.add(QStringLiteral("xl/_rels/workbook.xml.rels"), kWorkbookRelations);
    zip.add(QStringLiteral("xl/styles.xml"), kStyles);
    zip.add(QStringLiteral("xl/worksheets/sheet1.xml"), sheet);
    return writeFile(fileName, zip.finish());
}

bool BrExport::toRtf(const BrDocument &doc, const QString &fileName)
{
    if (doc.isEmpty())
        return false;
    RtfTables tables;
    QByteArray body;
    for (int p = 0; p < doc.pages.size(); ++p) {
        const BrPage &page = doc.pages.at(p);
        body += "\\sectd\\sbkpage\\pgwsxn" + QByteArray::number(twips(page.size.width())) + "\\pghsxn"
                + QByteArray::number(twips(page.size.height()))
                + "\\marglsxn0\\margrsxn0\\margtsxn0\\margbsxn0\n";
        if (page.size.width() > page.size.height())
            body += "\\lndscpsxn\n";

        // Shapes and frames are anchored to the paragraph that follows them.
        int order = 0;
        QByteArray shapes;
        for (const BrPageItem &item : page.items) {
            switch (item.kind) {
            case BrPageItem::Rect:
            case BrPageItem::Ellipse:
                shapes += rtfShapeOf(item, ++order);
                break;
            case BrPageItem::Line:
                if (item.points.size() >= 2)
                    shapes += rtfShapeOf(item, ++order);
                break;
            case BrPageItem::Polygon: {
                // Pie slices and the like: drawn as an image.
                const QRectF bounds = item.points.boundingRect();
                BrPage piece;
                piece.size = bounds.size();
                BrPageItem moved = item;
                moved.points.translate(-bounds.topLeft());
                piece.items << moved;
                body += rtfImage(bounds, BrRenderer::toImage(piece, 150.0, Qt::transparent));
                break;
            }
            case BrPageItem::Image:
                if (!item.image.isNull())
                    body += rtfImage(item.rect, item.image);
                break;
            default:
                break;
            }
        }
        for (const BrPageItem &line : textLines(page))
            body += rtfParagraph(line, tables);
        body += "\\pard\\plain\\fs2 " + shapes + "\\par\n";
        if (p + 1 < doc.pages.size())
            body += "\\sect\n";
    }
    const QByteArray out = "{\\rtf1\\ansi\\ansicpg1252\\deff0\\uc1\n" + tables.header() + body + "}\n";
    return writeFile(fileName, out);
}
