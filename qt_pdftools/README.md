# qt_pdftools

Qt6 GUI for `cpp_pdftools` (native-lib backend only).

## Features (current)

- Multi-tab PDF preview (`Qt6Pdf` + `QPdfView`)
- File menu operations:
  - Open PDF to New Tab
  - Close Current Tab
  - Close All Tabs
  - Settings (theme)
- Sidebar tool pages:
  - Merge
  - Delete Page
  - Insert Page
  - Replace Page
  - PDF -> DOCX
- Native backend execution (in-process, via `pdftools::core`)
- Task log panel and status footer

## Build

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug -j8
```

## Run

```bash
./qt_pdftools/build/linux-debug/qt_pdftools
```

## Notes

- This project intentionally uses `native-lib` only.
- `native-cli` backend is not enabled.
- Theme setting is persisted with `QSettings`.
