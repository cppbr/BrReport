# TODO

Planned work, roughly in order of priority. See also the roadmap in the [README](README.md#roadmap).

## Report engine

- [ ] **Bands split across pages** (very long memos, contracts): print the part that fits and
      continue the rest on the next page.
- [ ] `hideIfEmpty` should also collapse the space of empty objects (shift the objects below up).
- [ ] Multi-page report templates (e.g. a cover page followed by the list).
- [ ] Multi-column details filled top to bottom (only left to right today).
- [ ] Watermark on continuous pages: center it on the final page height.

## Objects and output

- [ ] More barcodes: PDF417, Data Matrix, Code 39.
- [ ] Exporters: XLSX, RTF and plain text (for dot-matrix/thermal printers).
- [ ] SVG images (QtSvg, optional dependency) kept as vectors in PDF.

## Examples

- [ ] `master_detail_sql` and `grouped_sales` still use C++ lambdas (SQL re-query per invoice,
      group by month): convert them to declarative properties (masterFields/detailFields, group
      `dataFormula` such as `month(date)`) so that `--save` produces complete report files.

## Project

- [ ] Publish the repository; replace `<repository-url>` in the README; add the homepage URL to
      `CMakeLists.txt`.
- [ ] Continuous integration (GitHub Actions): Linux, Windows (MSVC and MinGW) and macOS, Qt 5.15
      and Qt 6; build badge in the README.
- [ ] Build and test with Qt 5.15 (only Qt 6 has been tested); embed translations with Qt 5 too
      (`qt5_add_translation`).
- [ ] API reference with Doxygen (the public headers are already documented).
- [ ] Packages: vcpkg and Conan recipes.
- [ ] More translations (Spanish first).

## Known limitations

- Interleaved 2 of 5 with only 2 digits is rejected by some scanners (ZXing requires at least 6).
- A chart cannot iterate the data source being printed by its own report/sub-report.
- Text justification is done line by line (no hyphenation).
