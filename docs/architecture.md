# BrReport architecture

This document explains how a report is modeled and how it is turned into pages.

## Libraries

```
BrReport::Widgets   BrPreviewWidget, BrPreviewDialog, BrPrinting      (QtWidgets, QtPrintSupport)
        │
BrReport::Core      model ─► engine ─► BrDocument ─► renderer / exporters  (QtCore, QtGui[, QtSql])
```

The core library never touches widgets, so it can run in console programs and servers.

## Object model

```
BrReport (pager, page setup, main data source)
├── BrBand (Header / Title / ColumnHeader / Detail / ColumnFooter / Summary / Footer)
│   ├── BrLabel, BrDBText, BrDBResult, BrSystemInfo, BrMemo, BrDBMemo
│   ├── BrImage, BrDBImage, BrDraw, BrBarcode, BrDBBarcode (1D and QR Code)
│   └── BrPanel (container) ── more controls
├── BrGroup (pager: groups consecutive records with the same key)
│   ├── BrBand ...
│   └── BrGroup (nested) ...
└── BrSubDetail (pager with its own data source, master/detail)
    └── BrBand ...
```

Ownership follows the QObject tree: deleting the report deletes everything. Data sources are *not*
owned by the report.

| Class | Role |
|---|---|
| `BrPager` | Owns bands, drives the print flow |
| `BrReport` | Root pager, page setup, preparation, export |
| `BrGroup` | Breaks the records by key |
| `BrSubDetail` | Iterates a second data source |
| `BrBand` | Horizontal strip with controls |
| `BrControl` | Base of every printable element |
| `BrContainer` | Control that lays out children |

## Print flow

`BrReport::prepare()` creates a `BrEngine` (private) which walks the model:

```
run():
    push report frame
    newPage()                         ─ Header, Title (first page only), ColumnHeader
    for each record of the report data source:
        if the report has a group: runGroup(group)   (consumes records while the key holds)
        else: printRecord(); next record
    finish report: ColumnFooter, Summary
    closePage()                       ─ ColumnFooters of open pagers, report Footer at the bottom
    replace the page-count placeholder in every page

runGroup(g):
    PageBreak::Before? -> new page
    push frame, remember key, reset g's results
    print Title, Header, ColumnHeader
    while records and no key of any open group changed:
        nested group? runGroup(child) : printRecord(); next record
    print ColumnFooter, Summary, Footer; pop
    PageBreak::After? -> new page before the next printed band

printRecord():
    for each child of the current pager, in creation order:
        Detail band  -> printBand
        BrSubDetail  -> callBeforeOpen, iterate its data source (like a small report)
    accumulate the BrDBResult controls of the current data owner
```

### Printing a band

1. `checkPrint()` runs the `BeforePrint` handlers (band first, then controls); a handler may change
   properties or cancel printing.
2. `prepare()` computes the content (text of fields, formulas, system info...).
3. `measureHeight()` lays out the children: aligned controls (`Align::Top/Bottom/Left/Right/Client`),
   stretched memos; with `autoExpand()` the band grows to fit.
4. If the band does not fit above the reserved bottom area, a new page is started (unless the page is
   still empty, which prevents infinite loops with bands taller than a page).
5. `render()` appends `BrPageItem`s to the current page; `AfterPrint` runs.

The reserved bottom area is the sum of the report Footer bands plus the ColumnFooter bands of every
open pager and the Footer bands of open groups/sub-details.

### Page breaks

On a page break the engine:

- prints ColumnFooter bands of all open pagers (innermost first) and Footer bands of open
  groups/sub-details right after the content;
- prints the report Footer anchored to the bottom margin;
- starts a new page and prints Header/ColumnHeader of the report and of every open group/sub-detail
  (Title bands are not repeated).

### Results (BrDBResult)

Each `BrDBResult` belongs to the pager of its band. It accumulates when a record of its *data owner*
(the nearest report or sub-detail above that pager) is processed while the pager is open, and resets
when the pager starts. Accumulation happens after the page-break decision of the record's first detail
band, so page totals in the report Footer (with `resetAfterPrint`) never include the record that moved
to the next page.

### Total number of pages

The total is unknown until the end. `BrSystemInfo` (`LastPageNumber`, `PagePreview`) and the `pages()`
expression function emit a placeholder that is replaced in every text item once the document is
complete.

## The document

`BrDocument` is a list of `BrPage`, each a list of `BrPageItem` primitives (text line, rectangle, line,
ellipse, polygon, image) in absolute millimeters:

- `BrRenderer` paints it on any `QPainter` (screen, printer, `QPdfWriter`, `QImage`), using 1/100 mm
  logical units and converting font points to the device resolution;
- `BrExport` writes PDF, images, SVG and HTML;
- `BrDocument::save()/load()` store it in the binary `.brp` format (`QDataStream`, versioned).

## Text layout

Text is measured on a 1200 dpi reference device without font hinting (`BrTextMetrics`), and memo lines
are broken while preparing. Each line becomes its own text item, so the line breaks are identical on
screen, paper, PDF and images.

## Expressions

`BrExpression` is a small recursive-descent parser producing an AST that is evaluated against a
`BrExpressionContext`. During preparation the context is the engine: identifiers resolve to fields of
the innermost data source first (or `dataSourceName.field`), then report variables; report functions
(`page()`, `pages()`, `recno()`, `title()`...) and functions registered with
`BrReport::registerFunction()` are available alongside the built-ins.

## Extending

- **Custom control**: subclass `BrControl` (or `BrCustomText` for text) and override `prepare()`,
  `measureHeight()` and `render()`, appending `BrPageItem`s to the page.
- **Custom data source**: subclass `BrDataSource` and implement `first()`, `next()`, `eof()`,
  `fieldNames()` and `value()`.
- **Custom exporter**: iterate `BrDocument::pages` and their items.
