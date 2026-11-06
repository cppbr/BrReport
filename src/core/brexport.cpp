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

#include "brexport.h"

#include "brrenderer.h"

#include <QBuffer>
#include <QFileInfo>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTextStream>

namespace {

QPageLayout pageLayoutFor(const BrPage &page)
{
    const bool landscape = page.size.width() > page.size.height();
    const QSizeF portrait = landscape ? page.size.transposed() : page.size;
    return QPageLayout(QPageSize(portrait, QPageSize::Millimeter),
                       landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
                       QMarginsF(0, 0, 0, 0), QPageLayout::Millimeter);
}

QString num(double v)
{
    return QString::number(v, 'f', 3);
}

QString escapeXml(const QString &s)
{
    return s.toHtmlEscaped();
}

QString svgColor(const QColor &c)
{
    return c.isValid() ? c.name(QColor::HexRgb) : QStringLiteral("none");
}

QString svgOpacity(const QString &attr, const QColor &c)
{
    if (!c.isValid() || c.alpha() == 255)
        return {};
    return QStringLiteral(" %1=\"%2\"").arg(attr, QString::number(c.alphaF(), 'f', 3));
}

QString svgStroke(const BrPageItem &it)
{
    if (it.penStyle == Qt::NoPen || !it.penColor.isValid())
        return QStringLiteral(" stroke=\"none\"");
    QString s = QStringLiteral(" stroke=\"%1\" stroke-width=\"%2\"")
                    .arg(svgColor(it.penColor), num(it.penWidth > 0 ? it.penWidth : 0.1));
    s += svgOpacity(QStringLiteral("stroke-opacity"), it.penColor);
    const double w = it.penWidth > 0 ? it.penWidth : 0.1;
    if (it.penStyle == Qt::DashLine)
        s += QStringLiteral(" stroke-dasharray=\"%1 %2\"").arg(num(w * 4), num(w * 2));
    else if (it.penStyle == Qt::DotLine)
        s += QStringLiteral(" stroke-dasharray=\"%1 %2\"").arg(num(w), num(w * 2));
    return s;
}

QString svgFill(const BrPageItem &it)
{
    return QStringLiteral(" fill=\"%1\"").arg(svgColor(it.brushColor))
           + svgOpacity(QStringLiteral("fill-opacity"), it.brushColor);
}

QString svgText(const BrPageItem &it)
{
    const QRectF r = it.rect;
    QString anchor = QStringLiteral("start");
    double x = r.left();
    if (it.alignment & Qt::AlignHCenter) {
        anchor = QStringLiteral("middle");
        x = r.center().x();
    } else if (it.alignment & Qt::AlignRight) {
        anchor = QStringLiteral("end");
        x = r.right();
    }
    QString baseline = QStringLiteral("central");
    double y = r.center().y();
    if (it.alignment & Qt::AlignTop) {
        baseline = QStringLiteral("hanging");
        y = r.top();
    } else if (it.alignment & Qt::AlignBottom) {
        baseline = QStringLiteral("text-after-edge");
        y = r.bottom();
    }
    const QFont &f = it.font;
    QString style = QStringLiteral("font-family:'%1';font-size:%2px")
                        .arg(f.family().toHtmlEscaped(), num(Br::ptToMm(f.pointSizeF())));
    if (f.bold())
        style += QStringLiteral(";font-weight:bold");
    if (f.italic())
        style += QStringLiteral(";font-style:italic");
    QStringList deco;
    if (f.underline())
        deco << QStringLiteral("underline");
    if (f.strikeOut())
        deco << QStringLiteral("line-through");
    if (!deco.isEmpty())
        style += QStringLiteral(";text-decoration:") + deco.join(QLatin1Char(' '));

    QString transform;
    if (!qFuzzyIsNull(it.angle))
        transform = QStringLiteral(" transform=\"rotate(%1 %2 %3)\"")
                        .arg(num(-it.angle), num(r.center().x()), num(r.center().y()));
    return QStringLiteral("<text x=\"%1\" y=\"%2\" text-anchor=\"%3\" dominant-baseline=\"%4\" "
                          "fill=\"%5\" style=\"%6\" xml:space=\"preserve\"%7>%8</text>\n")
        .arg(num(x), num(y), anchor, baseline, svgColor(it.textColor), style, transform,
             escapeXml(it.text));
}

QString svgImage(const BrPageItem &it)
{
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    it.image.save(&buf, "PNG");
    return QStringLiteral("<image x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                          "preserveAspectRatio=\"none\" href=\"data:image/png;base64,%5\"/>\n")
        .arg(num(it.rect.x()), num(it.rect.y()), num(it.rect.width()), num(it.rect.height()),
             QString::fromLatin1(png.toBase64()));
}

} // namespace

bool BrExport::toPdf(const BrDocument &doc, const QString &fileName, int resolution)
{
    if (doc.isEmpty())
        return false;
    QPdfWriter writer(fileName);
    writer.setResolution(resolution);
    writer.setTitle(doc.title);
    writer.setCreator(QStringLiteral("BrReport " BRREPORT_VERSION_STR));
    writer.setPageLayout(pageLayoutFor(doc.pages.first()));

    QPainter painter;
    if (!painter.begin(&writer))
        return false;
    const double pxPerMm = resolution / 25.4;
    for (int i = 0; i < doc.pages.size(); ++i) {
        const BrPage &page = doc.pages.at(i);
        if (i > 0) {
            writer.setPageLayout(pageLayoutFor(page));
            writer.newPage();
        }
        BrRenderer::renderPage(&painter, page, pxPerMm);
    }
    return painter.end();
}

QStringList BrExport::toImages(const BrDocument &doc, const QString &filePattern, double dpi)
{
    QStringList written;
    for (int i = 0; i < doc.pages.size(); ++i) {
        const QString name = filePattern.arg(i + 1);
        if (!BrRenderer::toImage(doc.pages.at(i), dpi).save(name))
            return {};
        written << name;
    }
    return written;
}

QString BrExport::toSvg(const BrPage &page)
{
    QString out;
    QTextStream s(&out);
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << num(page.size.width())
      << "mm\" height=\"" << num(page.size.height()) << "mm\" viewBox=\"0 0 "
      << num(page.size.width()) << ' ' << num(page.size.height()) << "\">\n";
    s << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    for (const BrPageItem &it : page.items) {
        switch (it.kind) {
        case BrPageItem::Text:
            if (!it.text.isEmpty())
                s << svgText(it);
            break;
        case BrPageItem::Rect:
            s << "<rect x=\"" << num(it.rect.x()) << "\" y=\"" << num(it.rect.y())
              << "\" width=\"" << num(it.rect.width()) << "\" height=\"" << num(it.rect.height())
              << '"';
            if (it.radius > 0)
                s << " rx=\"" << num(it.radius) << "\" ry=\"" << num(it.radius) << '"';
            s << svgFill(it) << svgStroke(it) << "/>\n";
            break;
        case BrPageItem::Line:
            if (it.points.size() >= 2)
                s << "<line x1=\"" << num(it.points[0].x()) << "\" y1=\"" << num(it.points[0].y())
                  << "\" x2=\"" << num(it.points[1].x()) << "\" y2=\"" << num(it.points[1].y())
                  << '"' << svgStroke(it) << "/>\n";
            break;
        case BrPageItem::Ellipse:
            s << "<ellipse cx=\"" << num(it.rect.center().x()) << "\" cy=\""
              << num(it.rect.center().y()) << "\" rx=\"" << num(it.rect.width() / 2)
              << "\" ry=\"" << num(it.rect.height() / 2) << '"' << svgFill(it) << svgStroke(it)
              << "/>\n";
            break;
        case BrPageItem::Polygon: {
            QStringList pts;
            for (const QPointF &p : it.points)
                pts << num(p.x()) + QLatin1Char(',') + num(p.y());
            s << "<polygon points=\"" << pts.join(QLatin1Char(' ')) << '"' << svgFill(it)
              << svgStroke(it) << "/>\n";
            break;
        }
        case BrPageItem::Image:
            if (!it.image.isNull())
                s << svgImage(it);
            break;
        case BrPageItem::RichText: {
            QString align = QStringLiteral("left");
            if (it.alignment & Qt::AlignHCenter)
                align = QStringLiteral("center");
            else if (it.alignment & Qt::AlignRight)
                align = QStringLiteral("right");
            s << "<foreignObject x=\"" << num(it.rect.x()) << "\" y=\"" << num(it.rect.y())
              << "\" width=\"" << num(it.rect.width()) << "\" height=\"" << num(it.rect.height())
              << "\"><div xmlns=\"http://www.w3.org/1999/xhtml\" style=\"font-family:'"
              << it.font.family().toHtmlEscaped() << "';font-size:" << num(Br::ptToMm(it.font.pointSizeF()))
              << "px;color:" << svgColor(it.textColor) << ";text-align:" << align
              << ";line-height:1.15;margin-top:" << num(-it.textOffset) << "px\">" << it.text
              << "</div></foreignObject>\n";
            break;
        }
        }
    }
    s << "</svg>\n";
    return out;
}

bool BrExport::toHtml(const BrDocument &doc, const QString &fileName)
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream s(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    s.setCodec("UTF-8");
#endif
    const QString title = doc.title.isEmpty() ? QFileInfo(fileName).completeBaseName() : doc.title;
    s << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>"
      << title.toHtmlEscaped() << "</title>\n<style>\n"
      << "body{background:#808080;margin:0;padding:16px;}\n"
      << ".page{background:#fff;margin:0 auto 16px auto;box-shadow:0 2px 8px rgba(0,0,0,.4);"
         "width:fit-content;}\n"
      << ".page svg{display:block;}\n"
      << "@media print{body{background:none;padding:0}.page{box-shadow:none;margin:0;"
         "page-break-after:always}}\n"
      << "</style>\n</head>\n<body>\n";
    for (const BrPage &page : doc.pages)
        s << "<div class=\"page\">\n" << toSvg(page) << "</div>\n";
    s << "</body>\n</html>\n";
    s.flush();
    return file.commit();
}
