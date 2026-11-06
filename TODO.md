# TODO

Planned work, roughly in order of priority.

## Project

- [ ] Publish the repository; replace `<repository-url>` in the README; add the homepage URL to
      `CMakeLists.txt`.
- [ ] See the first run of the CI (`.github/workflows/ci.yml`: Linux, Windows with MSVC and MinGW,
      macOS, Qt 5.15 and Qt 6) and add the build badge to the README.
- [ ] Try the Conan and vcpkg recipes in `packaging/`, and fill the repository of the vcpkg port.

## Known limitations

- Interleaved 2 of 5 with only 2 digits is rejected by some scanners (ZXing requires at least 6).
- A chart cannot iterate the data source being printed by its own report/sub-report.
- Text justification is done line by line (no hyphenation).
