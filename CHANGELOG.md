# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Changed
- `hideIfEmpty` also gives up the space of the object: the objects below move up and the band gets
  shorter (with `shiftOnStretch`, the default), so an empty line of an address leaves no gap.

### Added
- Bands can be split across pages (`allowSplit`, *Allow split* in the designer): the part that fits
  is printed and the rest continues on the next page, for very long memos and contracts, in plain
  or HTML text. Lines of text, images, barcodes and charts are not cut. A band taller than a page is
  always split, instead of going over the bottom of the page.
- The designer can remember the window layout, the zoom and the recent files
  (`BrDesignerWidget::saveSettings()` / `restoreSettings()`, *File > Recent files*); `brdesigner`
  does it.
- Multi-selection in the designer: *Format > Distribute horizontally / vertically*, and the position
  and size in the *Properties* panel apply to every selected object, one value at a time. Values
  that differ among the objects show as blank.
- Structure panel of the designer: drag bands and objects to reorder them among their siblings, and
  rename them with a double-click or F2.
- New report wizard in the designer (*File > New from wizard*, `BrDesignerWidget::newReportWizard()`):
  pick a data source or SQL query and the fields, with totals and an optional group, and get a list
  report ready to adjust.
- Protected objects: the developer can lock the position, deletion, properties and contents of
  objects, bands, groups, sub-reports and the report itself, and hide properties, so that end users
  change only parts of a report (`BrProtection`, `protection` in the report file). The designer obeys
  it by default; `BrDesignerWidget::setDeveloperMode()` and `brdesigner --developer` edit it.
- Rulers and guides in the designer: drag from a ruler to create a guide, drag it back to remove it.
  Moved and resized objects snap to the edges and centers of other objects and to the guides. Guides
  are stored in the report file (`verticalGuides`, `horizontalGuides`) and never printed.
- Expression builder in the designer: `Formula`, `Print if`, `Highlight if` and group formulas have
  a `…` button that lists the fields, parameters, variables, functions and operators and checks the
  formula as it is typed. New `BrExpression::identifiers()`/`functions()` and
  `BrReport::functionNames()`.
- Report parameters: declared in the report (`BrReport::setParameters()`/`addParameter()`, `Report
  parameters` in the designer's Data menu), asked for by `BrPrinting::preview()`/`print()` and the
  designer preview (`BrParametersDialog`), available to SQL queries and formulas. Their choices can
  be a fixed list or come from a SQL query (`choicesSql`).
- Continuous pages for receipt/roll printers (`BrReport::setContinuous()`).
- Watermarks with text and/or image (`BrReport::setWatermark()`).
- Charts (`BrChart`): pie, bars, horizontal bars and lines, from static data or a data source.
- Rich text: HTML in text objects (`BrCustomText::setAllowHtml()`), exported to PDF, images,
  preview and HTML/SVG.
- Multi-column detail bands (`BrBand::setColumns()`).
- Zebra rows (`BrBand::setAlternateBackground()`).
- Report header/footer bands only on the first/last page (`setPrintOnFirstPage()`,
  `setPrintOnLastPage()`).
- Group/sub-detail headers kept with their first record (`BrPager::setKeepHeaderWithDetail()`).
- Objects pushed down by stretched objects above them (`BrContainer::setShiftOnStretch()`,
  on by default) and objects stretched to the band height (`BrControl::setStretchToBand()`).
- Text options: `setShrinkToFit()` and `setHideIfEmpty()`.
- Text search in the preview window (Ctrl+F, F3).
- `brview` tool to view, print and convert `.brp` documents.
- `dashboard` and `receipt` examples.
- Brazilian Portuguese translation of the preview window and the designer, embedded in the
  libraries (`BrTranslations::install()`); `brdesigner`, `brview` and the examples follow the system
  language; `update_translations` build target for maintainers.
- Report designer (`BrReport::Designer`, `BrDesignerWidget`) and the standalone `brdesigner`
  application: toolbox, drag fields from the data panel, move/resize with grid snapping, band
  resizing, structure tree, property editor, groups, sub-reports, SQL query editor, alignment,
  copy/paste, undo/redo, zoom and preview; save to files or through a save handler.
- SQL queries stored in report files (`BrReport::setQueries()`/`addQuery()`), bound to report
  variables; design mode rendering (`BrDesignContext`).
- Report files: layouts saved/loaded as JSON (`BrReport::save()`/`load()`, `BrReportFile`), with
  every object property exposed as `Q_PROPERTY` and custom object types.
- Data sources registered by name (`BrReport::addDataSource()`, `dataSourceName` properties).
- Declarative behavior: `printIf` on every object and band, conditional highlight on texts
  (`highlightIf`, `highlightColor`, `highlightBackground`, `highlightBold`) and master/detail links
  (`BrSubDetail::setMasterFields()`/`setDetailFields()`).
- `report_file` example; every example accepts `--save file.brt`.
- Page numbering per group/sub-detail (`BrPager::setResetPageNumber()`), with
  `Br::InfoType::DocumentPageNumber`/`DocumentPageCount` and the `documentpage()`/`documentpages()`
  formula functions for the whole document.

## [0.1.0] - 2026-10-31

### Added
- Band engine: Header, Title, Column Header, Detail, Column Footer, Summary and Footer bands,
  automatic page breaks, forced page breaks, carbon copies and growing bands.
- Nested groups (`BrGroup`) and master/detail sub-reports (`BrSubDetail`).
- Objects: `BrLabel`, `BrMemo`, `BrDBText`, `BrDBMemo`, `BrDBResult`, `BrSystemInfo`, `BrImage`,
  `BrDBImage`, `BrDraw`, `BrPanel`, `BrBarcode`, `BrDBBarcode`.
- Barcodes: QR Code (versions 1-40, levels L/M/Q/H), Code 128, EAN-13, Interleaved 2 of 5.
- Data sources: list, Qt item model, SQL query and callback.
- Formula language for data objects, group keys and custom C++ functions.
- Picture masks for numbers and locale-aware formatting.
- Export to PDF, HTML, SVG and images; `.brp` document format.
- Preview widget/dialog and printing (`BrReport::Widgets`).
- Examples and unit tests.
