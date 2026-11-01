# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
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
