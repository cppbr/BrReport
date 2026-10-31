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

#include "brtextmetrics.h"

#include <QFontMetricsF>
#include <QImage>
#include <QTextLayout>

namespace {

const double kRefDpi = 1200.0;

const QPaintDevice *referenceDevice()
{
    static QImage device = [] {
        QImage img(1, 1, QImage::Format_ARGB32);
        const int dpm = qRound(kRefDpi / 0.0254);
        img.setDotsPerMeterX(dpm);
        img.setDotsPerMeterY(dpm);
        return img;
    }();
    return &device;
}

QFont normalized(const QFont &font)
{
    QFont f = font;
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

double pxToMm(double px)
{
    return px * 25.4 / kRefDpi;
}

} // namespace

double BrTextMetrics::width(const QFont &font, const QString &text)
{
    QFontMetricsF fm(normalized(font), const_cast<QPaintDevice *>(referenceDevice()));
    return pxToMm(fm.horizontalAdvance(text));
}

double BrTextMetrics::lineHeight(const QFont &font)
{
    QFontMetricsF fm(normalized(font), const_cast<QPaintDevice *>(referenceDevice()));
    return pxToMm(fm.height());
}

QStringList BrTextMetrics::wrap(const QFont &font, const QString &text, double widthMm,
                                bool wordWrap, QList<bool> *paragraphEnds)
{
    QString normalizedText = text;
    normalizedText.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    const QStringList paragraphs = normalizedText.split(QLatin1Char('\n'));
    if (paragraphEnds)
        paragraphEnds->clear();
    if (!wordWrap || widthMm <= 0.0) {
        if (paragraphEnds)
            for (int i = 0; i < paragraphs.size(); ++i)
                paragraphEnds->append(true);
        return paragraphs;
    }

    const QFont f = normalized(font);
    const double widthPx = widthMm * kRefDpi / 25.4;
    QStringList lines;
    for (const QString &paragraph : paragraphs) {
        if (paragraph.isEmpty()) {
            lines << QString();
            if (paragraphEnds)
                paragraphEnds->append(true);
            continue;
        }
        QTextLayout layout(paragraph, f, const_cast<QPaintDevice *>(referenceDevice()));
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        layout.beginLayout();
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            line.setLineWidth(widthPx);
            QString part = paragraph.mid(line.textStart(), line.textLength());
            while (part.endsWith(QLatin1Char(' ')))
                part.chop(1);
            lines << part;
            if (paragraphEnds)
                paragraphEnds->append(false);
        }
        layout.endLayout();
        if (paragraphEnds && !paragraphEnds->isEmpty())
            paragraphEnds->last() = true;
    }
    return lines;
}
