# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
follows [Semantic Versioning](https://semver.org/).

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
