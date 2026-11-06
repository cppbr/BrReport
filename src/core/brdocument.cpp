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

#include "brdocument.h"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>

namespace {
const quint32 kMagic = 0x42525054; // "BRPT"
const quint16 kFormatVersion = 2; // 2: textOffset

void writeItem(QDataStream &out, const BrPageItem &it)
{
    out << quint8(it.kind) << it.rect << it.penColor << it.penWidth << qint32(it.penStyle)
        << it.brushColor << it.radius << it.points << it.text << it.font << it.textColor
        << qint32(it.alignment) << it.angle << it.textOffset;
    const bool hasImage = !it.image.isNull();
    out << hasImage;
    if (hasImage)
        out << it.image;
}

void readItem(QDataStream &in, BrPageItem &it, quint16 version)
{
    quint8 kind = 0;
    qint32 penStyle = 0, alignment = 0;
    bool hasImage = false;
    in >> kind >> it.rect >> it.penColor >> it.penWidth >> penStyle >> it.brushColor >> it.radius
        >> it.points >> it.text >> it.font >> it.textColor >> alignment >> it.angle;
    if (version >= 2)
        in >> it.textOffset;
    in >> hasImage;
    if (hasImage)
        in >> it.image;
    it.kind = BrPageItem::Kind(kind);
    it.penStyle = Qt::PenStyle(penStyle);
    it.alignment = alignment;
}
} // namespace

BrPageItem BrPageItem::makeText(const QRectF &rect, const QString &text, const QFont &font,
                                const QColor &color, int alignment, double angle)
{
    BrPageItem it;
    it.kind = Text;
    it.rect = rect;
    it.text = text;
    it.font = font;
    it.textColor = color;
    it.alignment = alignment;
    it.angle = angle;
    return it;
}

BrPageItem BrPageItem::makeRect(const QRectF &rect, const QColor &pen, double penWidth,
                                const QColor &brush, Qt::PenStyle style)
{
    BrPageItem it;
    it.kind = Rect;
    it.rect = rect;
    it.penColor = pen;
    it.penWidth = penWidth;
    it.penStyle = pen.isValid() ? style : Qt::NoPen;
    it.brushColor = brush;
    return it;
}

BrPageItem BrPageItem::makeLine(const QPointF &p1, const QPointF &p2, const QColor &pen,
                                double penWidth, Qt::PenStyle style)
{
    BrPageItem it;
    it.kind = Line;
    it.points = QPolygonF({p1, p2});
    it.rect = QRectF(p1, p2).normalized();
    it.penColor = pen;
    it.penWidth = penWidth;
    it.penStyle = style;
    return it;
}

BrPageItem BrPageItem::makeImage(const QRectF &rect, const QImage &image)
{
    BrPageItem it;
    it.kind = Image;
    it.rect = rect;
    it.image = image;
    it.penStyle = Qt::NoPen;
    return it;
}

void BrDocument::clear()
{
    title.clear();
    pages.clear();
}

void BrDocument::replaceText(const QString &token, const QString &value)
{
    for (BrPage &page : pages)
        for (BrPageItem &item : page.items)
            if (item.kind == BrPageItem::Text && item.text.contains(token))
                item.text.replace(token, value);
}

QString BrDocument::pageText(int page) const
{
    if (page < 0 || page >= pages.size())
        return {};
    QStringList lines;
    for (const BrPageItem &item : pages.at(page).items)
        if (item.kind == BrPageItem::Text)
            lines << item.text;
    return lines.join(QLatin1Char('\n'));
}

bool BrDocument::save(QIODevice *device) const
{
    QDataStream out(device);
    out.setVersion(QDataStream::Qt_5_15);
    out << kMagic << kFormatVersion << title << qint32(pages.size());
    for (const BrPage &page : pages) {
        out << page.size << qint32(page.items.size());
        for (const BrPageItem &item : page.items)
            writeItem(out, item);
    }
    return out.status() == QDataStream::Ok;
}

bool BrDocument::save(const QString &fileName) const
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (!save(&file))
        return false;
    return file.commit();
}

bool BrDocument::load(QIODevice *device)
{
    QDataStream in(device);
    in.setVersion(QDataStream::Qt_5_15);
    quint32 magic = 0;
    quint16 version = 0;
    in >> magic >> version;
    if (magic != kMagic || version > kFormatVersion)
        return false;

    BrDocument doc;
    qint32 pageCount = 0;
    in >> doc.title >> pageCount;
    for (qint32 p = 0; p < pageCount && in.status() == QDataStream::Ok; ++p) {
        BrPage page;
        qint32 itemCount = 0;
        in >> page.size >> itemCount;
        for (qint32 i = 0; i < itemCount && in.status() == QDataStream::Ok; ++i) {
            BrPageItem item;
            readItem(in, item, version);
            page.items.append(item);
        }
        doc.pages.append(page);
    }
    if (in.status() != QDataStream::Ok)
        return false;
    *this = doc;
    return true;
}

bool BrDocument::load(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return load(&file);
}
