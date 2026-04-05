# Web PDFTools WASM Worker 进展（v4）

更新时间：2026-04-04

## 1. 本轮新增完成

1. `cpp_pdftools` wasm 构建从“最小 stub”升级为“真实链接 PoDoFo 依赖”。
2. `pdftools_wasm_capi.cpp` 已接入真实 PDF 编辑操作：
   - `merge`
   - `delete-page`
   - `insert-page`
   - `replace-page`
3. `build_wasm_stub.sh` 增强：
   - 自动可选启用 `ccache`（并将 `CCACHE_DIR` 放到项目可写目录）
   - 链接 `libpodofo.a + libpodofo_private.a + libpodofo_3rdparty.a`
   - 链接 `libxml2/freetype/zlib/libiconv`
   - 打开 wasm 文件系统支持（`-sFILESYSTEM=1`）
   - 启用异常捕获（`-fexceptions -sDISABLE_EXCEPTION_CATCHING=0`）
4. web 单测新增 `pdftools_wasm_backend.test.mjs`，覆盖 wasm backend 的核心响应解析路径。

---

## 2. 关键技术细节

## 2.1 C++ wasm C ABI

文件：`cpp_pdftools/src/wasm/pdftools_wasm_capi.cpp`

当前分发能力：
1. `ping`
2. `version`
3. `merge`
4. `delete-page`
5. `insert-page`
6. `replace-page`

输入字段兼容：
1. `insert-page` 支持 `at` / `insertAt`
2. `insert-page` 与 `replace-page` 支持 `sourcePage` / `sourcePageNumber`

错误输出统一：
1. `ok=false`
2. `error.code/message/context/details`
3. code 来源于 `ErrorCodeToString(...)`，与异常规范对齐

## 2.2 wasm 构建脚本

文件：`cpp_pdftools/scripts/build_wasm_stub.sh`

已实现：
1. 依赖存在性检查（缺失立即失败）
2. 依赖库完整链接（含 PoDoFo private/3rdparty）
3. `USE_CCACHE=auto|on|off` 控制
4. 产物输出：
   - `web_pdftools/wasm/dist/pdftools_wasm.mjs`
   - `web_pdftools/wasm/dist/pdftools_wasm.wasm`

本轮实测产物大小：
1. `pdftools_wasm.mjs` ~91KB
2. `pdftools_wasm.wasm` ~2.0MB

## 2.3 单测增强

新增文件：`web_pdftools/tests/pdftools_wasm_backend.test.mjs`
辅助 fixture：`web_pdftools/tests/fixtures/mock_emscripten_module.mjs`

新增覆盖点：
1. wasm backend init + run 成功路径
2. `{ ok:false, error }` envelope 错误映射
3. `{ status:{ ok:false } }` envelope 错误映射（含 `kNotFound -> NOT_FOUND` 标准化）
4. `pdftools_wasm_op` 非零返回码映射（`WASM_OP_FAILED`）
5. 非法 JSON 响应映射（`WASM_RESPONSE_INVALID`）

---

## 3. 已执行验证

1. `./cpp_pdftools/scripts/build_wasm_stub.sh`：通过
2. `cd web_pdftools && npm run lint:node`：通过
3. `cd web_pdftools && npm test`：通过
4. 单测统计：`21/21` 通过

---

## 4. 当前限制（仍需后续实现）

1. wasm backend 仍是 preview：尚未打通“宿主文件系统 <-> wasm FS”的文件桥接。
2. 前端传入的是宿主绝对路径，当前 wasm 侧不能直接读取宿主文件（需后续实现文件同步/挂载策略）。
3. `pdf2docx` wasm 路径尚未接入。

---

## 5. 建议下一步

1. 先实现 wasm 文件桥接层（renderer/worker 与 wasm FS 的输入输出同步策略）。
2. 在 web backend 增加“wasm 不支持时自动回退 native-cli”的可选策略。
3. 再接入 `pdf2docx` wasm 分发，并补充对应单测与错误映射测试。
