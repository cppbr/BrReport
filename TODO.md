# TODO

Planned work, roughly in order of priority. See also the roadmap in the [README](README.md#roadmap).

## Report engine


## Objects and output


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
