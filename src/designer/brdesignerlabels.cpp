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

// Translatable names of properties, enumeration values and object types shown
// by the designer. Generated from the Q_PROPERTY declarations and the Br
// enumerations; only used by lupdate and BrDesignerLabels.

#include "brdesignerlabels_p.h"

#include <QCoreApplication>
#include <QHash>

namespace {

[[maybe_unused]] const char *const kPropertyLabels[] = {
    QT_TRANSLATE_NOOP("BrProperty", "After"),
    QT_TRANSLATE_NOOP("BrProperty", "Align"),
    QT_TRANSLATE_NOOP("BrProperty", "Alignment"),
    QT_TRANSLATE_NOOP("BrProperty", "Allow html"),
    QT_TRANSLATE_NOOP("BrProperty", "Alternate background"),
    QT_TRANSLATE_NOOP("BrProperty", "Angle"),
    QT_TRANSLATE_NOOP("BrProperty", "Auto expand"),
    QT_TRANSLATE_NOOP("BrProperty", "Auto stretch"),
    QT_TRANSLATE_NOOP("BrProperty", "Average"),
    QT_TRANSLATE_NOOP("BrProperty", "Background"),
    QT_TRANSLATE_NOOP("BrProperty", "Band type"),
    QT_TRANSLATE_NOOP("BrProperty", "Bar"),
    QT_TRANSLATE_NOOP("BrProperty", "Bar color"),
    QT_TRANSLATE_NOOP("BrProperty", "Barcode type"),
    QT_TRANSLATE_NOOP("BrProperty", "Before"),
    QT_TRANSLATE_NOOP("BrProperty", "Borders"),
    QT_TRANSLATE_NOOP("BrProperty", "Bottom"),
    QT_TRANSLATE_NOOP("BrProperty", "Allow split"),
    QT_TRANSLATE_NOOP("BrProperty", "Carbon copies"),
    QT_TRANSLATE_NOOP("BrProperty", "Carbon copy"),
    QT_TRANSLATE_NOOP("BrProperty", "Center"),
    QT_TRANSLATE_NOOP("BrProperty", "Chart type"),
    QT_TRANSLATE_NOOP("BrProperty", "Client"),
    QT_TRANSLATE_NOOP("BrProperty", "Code128"),
    QT_TRANSLATE_NOOP("BrProperty", "Color"),
    QT_TRANSLATE_NOOP("BrProperty", "Column footer"),
    QT_TRANSLATE_NOOP("BrProperty", "Column header"),
    QT_TRANSLATE_NOOP("BrProperty", "Column spacing"),
    QT_TRANSLATE_NOOP("BrProperty", "Columns"),
    QT_TRANSLATE_NOOP("BrProperty", "Continuous"),
    QT_TRANSLATE_NOOP("BrProperty", "Count"),
    QT_TRANSLATE_NOOP("BrProperty", "Custom dash line"),
    QT_TRANSLATE_NOOP("BrProperty", "Dash dot dot line"),
    QT_TRANSLATE_NOOP("BrProperty", "Dash dot line"),
    QT_TRANSLATE_NOOP("BrProperty", "Dash line"),
    QT_TRANSLATE_NOOP("BrProperty", "Data field"),
    QT_TRANSLATE_NOOP("BrProperty", "Data fields"),
    QT_TRANSLATE_NOOP("BrProperty", "Data formula"),
    QT_TRANSLATE_NOOP("BrProperty", "Data source name"),
    QT_TRANSLATE_NOOP("BrProperty", "Date"),
    QT_TRANSLATE_NOOP("BrProperty", "Date time"),
    QT_TRANSLATE_NOOP("BrProperty", "Detail"),
    QT_TRANSLATE_NOOP("BrProperty", "Detail count"),
    QT_TRANSLATE_NOOP("BrProperty", "Detail fields"),
    QT_TRANSLATE_NOOP("BrProperty", "Diagonal down"),
    QT_TRANSLATE_NOOP("BrProperty", "Diagonal up"),
    QT_TRANSLATE_NOOP("BrProperty", "Display format"),
    QT_TRANSLATE_NOOP("BrProperty", "Document page count"),
    QT_TRANSLATE_NOOP("BrProperty", "Document page number"),
    QT_TRANSLATE_NOOP("BrProperty", "Dot line"),
    QT_TRANSLATE_NOOP("BrProperty", "Ean13"),
    QT_TRANSLATE_NOOP("BrProperty", "Ellipse"),
    QT_TRANSLATE_NOOP("BrProperty", "Enabled"),
    QT_TRANSLATE_NOOP("BrProperty", "Fill color"),
    QT_TRANSLATE_NOOP("BrProperty", "First"),
    QT_TRANSLATE_NOOP("BrProperty", "Font"),
    QT_TRANSLATE_NOOP("BrProperty", "Footer"),
    QT_TRANSLATE_NOOP("BrProperty", "Format"),
    QT_TRANSLATE_NOOP("BrProperty", "Geometry"),
    QT_TRANSLATE_NOOP("BrProperty", "Header"),
    QT_TRANSLATE_NOOP("BrProperty", "Height"),
    QT_TRANSLATE_NOOP("BrProperty", "Hide if empty"),
    QT_TRANSLATE_NOOP("BrProperty", "High"),
    QT_TRANSLATE_NOOP("BrProperty", "Highlight background"),
    QT_TRANSLATE_NOOP("BrProperty", "Highlight bold"),
    QT_TRANSLATE_NOOP("BrProperty", "Highlight color"),
    QT_TRANSLATE_NOOP("BrProperty", "Highlight if"),
    QT_TRANSLATE_NOOP("BrProperty", "Horizontal bar"),
    QT_TRANSLATE_NOOP("BrProperty", "Horizontal line"),
    QT_TRANSLATE_NOOP("BrProperty", "Image"),
    QT_TRANSLATE_NOOP("BrProperty", "Info"),
    QT_TRANSLATE_NOOP("BrProperty", "Interleaved2of5"),
    QT_TRANSLATE_NOOP("BrProperty", "Justify"),
    QT_TRANSLATE_NOOP("BrProperty", "Keep aspect"),
    QT_TRANSLATE_NOOP("BrProperty", "Keep header with detail"),
    QT_TRANSLATE_NOOP("BrProperty", "Kind"),
    QT_TRANSLATE_NOOP("BrProperty", "Label field"),
    QT_TRANSLATE_NOOP("BrProperty", "Labels"),
    QT_TRANSLATE_NOOP("BrProperty", "Landscape"),
    QT_TRANSLATE_NOOP("BrProperty", "Last"),
    QT_TRANSLATE_NOOP("BrProperty", "Last page number"),
    QT_TRANSLATE_NOOP("BrProperty", "Layout"),
    QT_TRANSLATE_NOOP("BrProperty", "Left"),
    QT_TRANSLATE_NOOP("BrProperty", "Line"),
    QT_TRANSLATE_NOOP("BrProperty", "Locale name"),
    QT_TRANSLATE_NOOP("BrProperty", "Low"),
    QT_TRANSLATE_NOOP("BrProperty", "MPen style"),
    QT_TRANSLATE_NOOP("BrProperty", "Margins"),
    QT_TRANSLATE_NOOP("BrProperty", "Master fields"),
    QT_TRANSLATE_NOOP("BrProperty", "Max"),
    QT_TRANSLATE_NOOP("BrProperty", "Max pages"),
    QT_TRANSLATE_NOOP("BrProperty", "Medium"),
    QT_TRANSLATE_NOOP("BrProperty", "Min"),
    QT_TRANSLATE_NOOP("BrProperty", "Module width"),
    QT_TRANSLATE_NOOP("BrProperty", "No pen"),
    QT_TRANSLATE_NOOP("BrProperty", "None"),
    QT_TRANSLATE_NOOP("BrProperty", "Padding"),
    QT_TRANSLATE_NOOP("BrProperty", "Page break"),
    QT_TRANSLATE_NOOP("BrProperty", "Page number"),
    QT_TRANSLATE_NOOP("BrProperty", "Page preview"),
    QT_TRANSLATE_NOOP("BrProperty", "Paper size"),
    QT_TRANSLATE_NOOP("BrProperty", "Pen color"),
    QT_TRANSLATE_NOOP("BrProperty", "Pen style"),
    QT_TRANSLATE_NOOP("BrProperty", "Pen width"),
    QT_TRANSLATE_NOOP("BrProperty", "Pie"),
    QT_TRANSLATE_NOOP("BrProperty", "Positioning"),
    QT_TRANSLATE_NOOP("BrProperty", "Prefix"),
    QT_TRANSLATE_NOOP("BrProperty", "Print if"),
    QT_TRANSLATE_NOOP("BrProperty", "Print on first page"),
    QT_TRANSLATE_NOOP("BrProperty", "Print on last page"),
    QT_TRANSLATE_NOOP("BrProperty", "Qr code"),
    QT_TRANSLATE_NOOP("BrProperty", "Qr error correction"),
    QT_TRANSLATE_NOOP("BrProperty", "Quartile"),
    QT_TRANSLATE_NOOP("BrProperty", "Queries"),
    QT_TRANSLATE_NOOP("BrProperty", "Quiet zone"),
    QT_TRANSLATE_NOOP("BrProperty", "Radius"),
    QT_TRANSLATE_NOOP("BrProperty", "Ratio"),
    QT_TRANSLATE_NOOP("BrProperty", "Rec no"),
    QT_TRANSLATE_NOOP("BrProperty", "Rectangle"),
    QT_TRANSLATE_NOOP("BrProperty", "Reset after print"),
    QT_TRANSLATE_NOOP("BrProperty", "Reset page number"),
    QT_TRANSLATE_NOOP("BrProperty", "Result type"),
    QT_TRANSLATE_NOOP("BrProperty", "Right"),
    QT_TRANSLATE_NOOP("BrProperty", "Rounded rectangle"),
    QT_TRANSLATE_NOOP("BrProperty", "Scaling"),
    QT_TRANSLATE_NOOP("BrProperty", "Series data"),
    QT_TRANSLATE_NOOP("BrProperty", "Shift on stretch"),
    QT_TRANSLATE_NOOP("BrProperty", "Show legend"),
    QT_TRANSLATE_NOOP("BrProperty", "Show text"),
    QT_TRANSLATE_NOOP("BrProperty", "Show values"),
    QT_TRANSLATE_NOOP("BrProperty", "Shrink to fit"),
    QT_TRANSLATE_NOOP("BrProperty", "Solid line"),
    QT_TRANSLATE_NOOP("BrProperty", "Stretch"),
    QT_TRANSLATE_NOOP("BrProperty", "Stretch to band"),
    QT_TRANSLATE_NOOP("BrProperty", "Suffix"),
    QT_TRANSLATE_NOOP("BrProperty", "Sum"),
    QT_TRANSLATE_NOOP("BrProperty", "Summary"),
    QT_TRANSLATE_NOOP("BrProperty", "Text"),
    QT_TRANSLATE_NOOP("BrProperty", "Time"),
    QT_TRANSLATE_NOOP("BrProperty", "Title"),
    QT_TRANSLATE_NOOP("BrProperty", "Top"),
    QT_TRANSLATE_NOOP("BrProperty", "Triangle"),
    QT_TRANSLATE_NOOP("BrProperty", "Value fields"),
    QT_TRANSLATE_NOOP("BrProperty", "Value format"),
    QT_TRANSLATE_NOOP("BrProperty", "Variables"),
    QT_TRANSLATE_NOOP("BrProperty", "Vertical line"),
    QT_TRANSLATE_NOOP("BrProperty", "Visible"),
    QT_TRANSLATE_NOOP("BrProperty", "Watermark angle"),
    QT_TRANSLATE_NOOP("BrProperty", "Watermark color"),
    QT_TRANSLATE_NOOP("BrProperty", "Watermark font"),
    QT_TRANSLATE_NOOP("BrProperty", "Watermark image"),
    QT_TRANSLATE_NOOP("BrProperty", "Watermark image opacity"),
    QT_TRANSLATE_NOOP("BrProperty", "Watermark text"),
    QT_TRANSLATE_NOOP("BrProperty", "Word wrap"),
};

struct TypeLabel
{
    const char *type;
    const char *label;
};

const TypeLabel kTypeLabels[] = {
    {"Report", QT_TRANSLATE_NOOP("BrObjectType", "Report")},
    {"Band", QT_TRANSLATE_NOOP("BrObjectType", "Band")},
    {"Group", QT_TRANSLATE_NOOP("BrObjectType", "Group")},
    {"SubDetail", QT_TRANSLATE_NOOP("BrObjectType", "Sub-report")},
    {"Panel", QT_TRANSLATE_NOOP("BrObjectType", "Panel")},
    {"Label", QT_TRANSLATE_NOOP("BrObjectType", "Text")},
    {"Memo", QT_TRANSLATE_NOOP("BrObjectType", "Memo")},
    {"DBText", QT_TRANSLATE_NOOP("BrObjectType", "Field")},
    {"DBMemo", QT_TRANSLATE_NOOP("BrObjectType", "Data memo")},
    {"DBResult", QT_TRANSLATE_NOOP("BrObjectType", "Total")},
    {"SystemInfo", QT_TRANSLATE_NOOP("BrObjectType", "System info")},
    {"Image", QT_TRANSLATE_NOOP("BrObjectType", "Image")},
    {"DBImage", QT_TRANSLATE_NOOP("BrObjectType", "Data image")},
    {"Draw", QT_TRANSLATE_NOOP("BrObjectType", "Shape")},
    {"Barcode", QT_TRANSLATE_NOOP("BrObjectType", "Barcode")},
    {"DBBarcode", QT_TRANSLATE_NOOP("BrObjectType", "Data barcode")},
    {"Chart", QT_TRANSLATE_NOOP("BrObjectType", "Chart")},
};

} // namespace

QString BrDesignerLabels::label(const QString &identifier)
{
    QString out;
    for (int i = 0; i < identifier.size(); ++i) {
        const QChar c = identifier.at(i);
        if (i > 0 && c.isUpper() && !identifier.at(i - 1).isUpper())
            out += QLatin1Char(' ') + c.toLower();
        else
            out += i == 0 ? c.toUpper() : c;
    }
    return QCoreApplication::translate("BrProperty", out.toUtf8().constData());
}

QString BrDesignerLabels::typeLabel(const QString &typeName)
{
    for (const TypeLabel &t : kTypeLabels)
        if (typeName == QLatin1String(t.type))
            return QCoreApplication::translate("BrObjectType", t.label);
    return typeName;
}
