# Contributing to BrReport

Thank you for your interest in BrReport! Everyone is welcome: bug reports, documentation fixes,
examples, new controls, exporters and data sources.

## Reporting issues

Please include:

- BrReport version (or commit), Qt version, compiler and operating system;
- a minimal program that reproduces the problem (the examples are a good starting point);
- what you expected and what happened (a PDF or screenshot helps a lot).

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep changes focused; one feature or fix per pull request.
3. Add or update unit tests in `tests/` and run them:
   ```sh
   cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
   ```
4. Update the documentation (`README.md`, `docs/`) and `CHANGELOG.md` when behavior changes.
5. Code, comments, commit messages and documentation are written in **English**.

## Translations

User interface strings are written in English in the code (`tr()`), and translated with Qt Linguist.
The translation sources live in `translations/brreport_<language>.ts`.

- **Update** the existing files after changing strings in the code:
  `cmake --build build --target update_translations`
- **Translate** with Qt Linguist (`linguist translations/brreport_pt_BR.ts`) or any `.ts` editor.
- **Add a language**: copy `brreport_pt_BR.ts` to `brreport_<language>.ts` (e.g. `brreport_es.ts`),
  set the `language` attribute, run `update_translations` and translate. The build compiles every
  `.ts` file and embeds it in the libraries; `BrTranslations::install()` picks it up.

## Coding style

- C++17, Qt naming conventions (`camelCase` methods, `m_` members, `Br` class prefix).
- Format with the provided `.clang-format`.
- Build without warnings; `QT_NO_CAST_FROM_ASCII` is enabled in the libraries, so use
  `QStringLiteral`/`QLatin1String`.
- Public classes are exported with `BRREPORT_EXPORT` / `BRREPORT_WIDGETS_EXPORT` and documented in
  the header.
- Keep the core library free of QtWidgets.
- Every source file starts with the Apache 2.0 license header (see any existing file).

## License

By contributing, you agree that your contributions are licensed under the Apache License 2.0.
