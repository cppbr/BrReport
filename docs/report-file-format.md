# Report file format (.brt)

A report file stores the **layout** of a report (page setup, bands, groups, sub-reports and objects)
as JSON. The **data** is not stored: the file refers to data sources by name and the application
registers them before printing.

```cpp
BrReport report;
report.addDataSource("customers", &customersDataSource);
report.load("customers.brt");
BrPrinting::preview(&report);
```

Any report built in code can be saved with `report.save("file.brt")` (every example accepts
`--save file.brt`), which is also the easiest way to start a new file.

## Structure

```json
{
    "format": "BrReport",
    "version": 1,
    "report": {
        "title": "Customer List",
        "dataSourceName": "customers",
        "margins": [15, 12, 15, 12],
        "children": [
            {
                "type": "Band",
                "bandType": "Detail",
                "height": 6,
                "alternateBackground": "#eef2f8",
                "children": [
                    { "type": "DBText", "dataField": "name", "geometry": [18, 0, 60, 6] },
                    {
                        "type": "DBText",
                        "name": "balance",
                        "dataField": "balance",
                        "geometry": [150, 0, 30, 6],
                        "alignment": "Right",
                        "displayFormat": "#,##0.00;(#,##0.00)",
                        "highlightIf": "balance < 0",
                        "highlightColor": "#ff0000"
                    }
                ]
            }
        ]
    }
}
```

- `format` and `version` identify the file. Files from a newer version are rejected.
- `report` holds the report properties and its `children`.
- Every object has a `type`, an optional `name` and its properties. Containers have `children`,
  printed in the order they appear.
- Only properties that differ from their default value are written.
- Unknown properties produce warnings and are ignored. An unknown `type`, or an object placed where
  it does not belong (a label directly in the report, for example), makes loading fail without
  changing the report.

## Object types

| Type | Can contain | Main properties |
|---|---|---|
| `Band` | objects | `bandType`, `height`, `pageBreak`, `carbonCopies`, `alternateBackground`, `columns`, `columnSpacing`, `printOnFirstPage`, `printOnLastPage`, `printIf` |
| `Group` | bands, groups, sub-reports | `dataFields`, `dataFormula`, `pageBreak`, `keepHeaderWithDetail`, `resetPageNumber` |
| `SubDetail` | bands, groups, sub-reports | `dataSourceName`, `masterFields`, `detailFields`, `positioning`, `pageBreak`, `resetPageNumber` |
| `Panel` | objects | `autoExpand`, `shiftOnStretch` |
| `Label`, `Memo` | | `text` |
| `DBText`, `DBMemo` | | `dataField`, `dataFormula`, `displayFormat`, `prefix`, `suffix` |
| `DBResult` | | `dataField`/`dataFormula`, `resultType`, `resetAfterPrint`, `displayFormat` |
| `SystemInfo` | | `info`, `format`, `displayFormat` |
| `Image`, `DBImage` | | `image` (embedded PNG) / `dataField`, `scaling` |
| `Draw` | | `kind`, `penColor`, `penWidth`, `penStyle`, `fillColor`, `radius` |
| `Barcode`, `DBBarcode` | | `barcodeType`, `text` / `dataField`, `showText`, `qrErrorCorrection`, `quietZone` |
| `Chart` | | `chartType`, `title`, `labels`, `seriesData`, `dataSourceName`, `labelField`, `valueFields`, `showLegend`, `showValues`, `valueFormat` |

Common to every object: `geometry` `[x, y, width, height]` in mm, `align`, `visible`, `printIf`,
`background`, `color`, `font`, `borders`, `stretchToBand`. Text objects also have `alignment`,
`layout`, `wordWrap`, `autoStretch`, `angle`, `padding`, `hideIfEmpty`, `shrinkToFit`, `allowHtml`,
`highlightIf`, `highlightColor`, `highlightBackground`, `highlightBold`.

Report properties: `title`, `paperSize` `[width, height]` in mm (portrait), `landscape`, `margins`
`[left, top, right, bottom]`, `continuous`, `font`, `localeName` (e.g. `"pt_BR"`), `dataSourceName`,
`variables`, `queries`, `parameters` (see the [designer guide](designer.md#report-parameters)),
`verticalGuides`, `horizontalGuides` (positions in mm of the designer guides, never printed),
`maxPages` and `watermarkText`, `watermarkFont`, `watermarkColor`, `watermarkAngle`,
`watermarkImage`, `watermarkImageOpacity`.

The property names are the `Q_PROPERTY` names of the classes (`BrBand`, `BrDBText`...), so the API
documentation in the headers also describes the file.

## Value formats

| Kind | Example |
|---|---|
| Enumerations | `"Right"`, `"Detail"`, `"QrCode"` (the C++ enumerator name) |
| Colors | `"#1f4e79"`, `"#80ff0000"` (with alpha), `null` (none) |
| Fonts | `{ "family": "Arial", "size": 9, "bold": true, "italic": true }` |
| Borders | `{ "sides": "ltrb", "width": 0.2, "color": "#000000", "style": "DashLine" }` |
| Rectangles, sizes, margins | arrays of numbers in mm |
| Images | `"data:image/png;base64,..."` |
| Chart series | `[{ "name": "2026", "values": [120, 98], "color": "#1f77b4" }]` |

## Behavior without code

Event handlers (C++ lambdas) cannot be stored in a file. These properties cover the usual cases:

| Need | Property |
|---|---|
| Print an object/band only in some cases | `printIf`: `"total > 0"`, `"not isnull(notes)"` |
| Highlight values | `highlightIf`, `highlightColor`, `highlightBackground`, `highlightBold` |
| Zebra rows | `alternateBackground` on the Detail band |
| Master/detail | `SubDetail` with `dataSourceName`, `masterFields` and `detailFields` |
| Computed text | `dataFormula`: `"qty * price"`, `"upper(name)"` |
| Values from the application | report `variables`, or `BrReport::setVariable()` before printing |

When code is still needed, give the object a `name` and attach handlers after loading:

```cpp
report.load("invoice.brt");
report.object<BrDBText>("total")->setBeforePrint([](BrControl &c, bool &printIt) { /* ... */ });
```

## Custom objects

Register custom classes (subclasses of `BrControl`, `BrBand` or `BrPager` with `Q_PROPERTY`
declarations) to use them in files:

```cpp
BrReportFile::registerType("Stamp", [] { return new MyStamp(nullptr); });   // class MyStamp
```

The type name written to files is the class name without a leading `Br`.
