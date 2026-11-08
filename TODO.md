# TODO

Planned work, roughly in order of priority.

## Project

- [ ] Try the vcpkg port in `packaging/vcpkg` (the Conan recipe was tried on Linux).

## Known limitations

- Interleaved 2 of 5 with only 2 digits is rejected by some scanners (ZXing requires at least 6).
- A chart cannot iterate the data source being printed by its own report/sub-report.
- Text justification is done line by line (no hyphenation).
