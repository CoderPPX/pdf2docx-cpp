# Web PDFTools WASM Worker 进展（v7）

更新时间：2026-04-04

## 1. 本轮新增完成（在 v6 基础上）

1. `pdf2docx` 已接入 wasm C ABI 分发。
2. wasm 构建脚本已扩展：编译 `legacy_pdf2docx` 所需源码并链接 `minizip/tinyxml2`。
3. renderer bridge 已支持 `pdf2docx` 的输入/输出文件映射与回写。
4. 单测继续扩展并保持全绿。

---

## 2. C++ / WASM 侧改动

## 2.1 C ABI 分发新增 `pdf2docx`

文件：`cpp_pdftools/src/wasm/pdftools_wasm_capi.cpp`

新增：
1. `HandlePdf2Docx(...)`
2. 分发入口 `operation == "pdf2docx"`

支持字段：
1. `inputPdf`
2. `outputDocx`
3. `dumpIrPath` / `dumpIrJsonPath`（可选）
4. `noImages`
5. `anchoredImages`
6. `enableFontFallback`
7. `overwrite`

返回 payload 包含：
1. `operation`
2. `outputDocx`
3. `pageCount`
4. `imageCount`
5. `warningCount`
6. `backend`
7. `elapsedMs`

## 2.2 wasm 构建脚本扩展

文件：`cpp_pdftools/scripts/build_wasm_stub.sh`

新增：
1. 编译 `src/convert/pdf2docx/pdf2docx.cpp`
2. 编译 legacy 源：
   - `core/status.cpp`
   - `core/ir_html.cpp`
   - `core/ir_json.cpp`
   - `api/converter.cpp`
   - `backend/podofo/podofo_backend.cpp`
   - `font/freetype_probe.cpp`
   - `font/font_resolver.cpp`
   - `pipeline/pipeline.cpp`
   - `docx/p0_writer.cpp`
3. 链接新增库：
   - `thirdparty/build_wasm/minizip-ng/libminizip.a`
   - `thirdparty/build_wasm/tinyxml2/libtinyxml2.a`
4. 新增 include：
   - `thirdparty/minizip-ng`
   - `thirdparty/minizip-ng/compat`
   - `thirdparty/tinyxml2`
   - `cpp_pdftools/src/legacy_pdf2docx`
5. 新增编译宏：
   - `PDF2DOCX_XML_BACKEND_STR="tinyxml2"`
   - `PDF2DOCX_PDF_BACKEND_STR="podofo"`
   - `PDF2DOCX_HAS_ZLIB=1`
   - `PDF2DOCX_HAS_TINYXML2=1`
   - `PDF2DOCX_HAS_MINIZIP=1`
   - `PDF2DOCX_HAS_FREETYPE=1`
   - `PDF2DOCX_HAS_PODOFO=1`

编译结果：
1. wasm 产物增大到约 `4.3MB`（含 pdf2docx 逻辑）。

---

## 3. Web bridge 侧改动

文件：`web_pdftools/backends/task_backends.mjs`

新增：
1. `pdf2docx` 纳入 host-fs bridge 支持集。
2. 任务映射：
   - host `inputPdf` -> wasm `/work/in/0.pdf`
   - host `outputDocx` -> wasm `/work/out/0.docx`
   - 可选 `dumpIrPath` -> wasm `/work/out/1.json`
3. worker 返回输出后自动回写宿主文件。

---

## 4. 验证结果

1. `./cpp_pdftools/scripts/build_wasm_stub.sh` 通过
2. `cd web_pdftools && npm run lint:node` 通过
3. `cd web_pdftools && npm test` 通过
4. 单测：`27/27` 通过

新增测试点：
1. `task backends: wasm worker host fs bridge supports pdf2docx`

---

## 5. 当前剩余建议

1. 增加真实 PDF -> DOCX 的 wasm 集成测试（非 mock module）。
2. 为 `pdf2docx` bridge 增加更细粒度错误分类（输入缺失/输出回写失败/转换失败）。
3. 在 Electron UI 上补一条 wasm backend 专用日志，显示 bridge 文件同步开销（便于后续性能分析）。
