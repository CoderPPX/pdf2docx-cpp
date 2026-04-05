# Web PDFTools WASM Worker 进展（v9）

更新时间：2026-04-04

## 1. 本轮新增（在 v8 基础上）

1. 新增真实 wasm 冒烟脚本：`web_pdftools/scripts/wasm_bridge_smoke.mjs`。
2. `package.json` 增加脚本：`npm run smoke:wasm`。
3. 真实链路已实跑：
   - `merge` 成功
   - `pdf2docx` 成功（含 IR JSON 输出）

---

## 2. 新增脚本能力

文件：`web_pdftools/scripts/wasm_bridge_smoke.mjs`

支持：
1. `--op merge`
2. `--op pdf2docx`

参数：
1. `--input`（merge 至少 2 个，pdf2docx 恰好 1 个）
2. `--output`
3. `--dump-ir`（可选，仅 pdf2docx）

实现要点：
1. 自动加载 `wasm/dist/pdftools_wasm.mjs/.wasm`
2. 通过 `moduleOptions.wasmBinary` 注入 wasm 二进制
3. 使用 `__hostFsBridge` 执行
4. 把 worker 回传输出写回宿主文件

---

## 3. 真实执行结果

命令 1（merge）：
`npm run smoke:wasm -- --op merge --input formula.pdf --input formula.pdf --output /tmp/pdftools_wasm_merge_smoke.pdf`

输出：
1. `ok=true`
2. 输出文件 `/tmp/pdftools_wasm_merge_smoke.pdf`
3. 输出字节 `67174`

命令 2（pdf2docx）：
`npm run smoke:wasm -- --op pdf2docx --input formula.pdf --output /tmp/pdftools_wasm_pdf2docx_smoke.docx --dump-ir /tmp/pdftools_wasm_pdf2docx_smoke.ir.json`

输出：
1. `ok=true`
2. `/tmp/pdftools_wasm_pdf2docx_smoke.docx`（2661 bytes）
3. `/tmp/pdftools_wasm_pdf2docx_smoke.ir.json`（8699 bytes）

---

## 4. 当前总体状态

1. wasm C ABI：
   - `merge/delete-page/insert-page/replace-page/pdf2docx` 已接入
2. Web backend：
   - 已支持 host-fs bridge 与输出回写
3. worker 协议：
   - 已支持 transfer list（二进制回传优化）
4. 构建：
   - `build_wasm_stub.sh` 可稳定产出 wasm（含 pdf2docx 依赖）
5. 测试：
   - `npm run lint:node` 通过
   - `npm test` 通过
   - 单测 `27/27` 通过

---

## 5. 下一步建议

1. 把 smoke 脚本纳入 CI 的可选 job（例如 nightly 或手动触发）。
2. 给 UI 增加 wasm bridge 执行耗时与文件同步耗时日志。
3. 若需要进一步降低内存峰值，可做 output 分块回写策略。
