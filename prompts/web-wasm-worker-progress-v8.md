# Web PDFTools WASM Worker 进展（v8）

更新时间：2026-04-04

## 1. 本轮新增完成（在 v7 基础上）

1. 修复 bridge 关键点：Emscripten `FS` 运行时方法已显式导出。
2. 执行了真实 wasm（非 mock）冒烟验证：`merge` 通过。
3. `pdf2docx` wasm 分发与 bridge 继续保持可编译可测试状态。

---

## 2. 关键修复

文件：`cpp_pdftools/scripts/build_wasm_stub.sh`

新增参数：
1. `-sEXPORTED_RUNTIME_METHODS=["FS"]`

原因：
1. host-fs bridge 需要 `module.FS` 执行 `writeFile/readFile/mkdir`。
2. 未导出时 worker 会报：`WASM_API_MISSING: WASM module FS API is required for host file bridge`。

---

## 3. 真实 wasm 冒烟结果

执行方式：
1. 使用真实 `web_pdftools/wasm/dist/pdftools_wasm.mjs/.wasm`
2. Node 侧通过 `moduleOptions.wasmBinary` 注入 wasm 二进制（规避 file:// fetch 限制）
3. 走 `__hostFsBridge` 任务：`merge`（输入 `formula.pdf` 两份）

结果：
1. `bridge=true`
2. `wasmOk=true`
3. 输出文件数量 `1`
4. 输出字节数 `67174`

---

## 4. 当前产物与测试

1. wasm 产物：
   - `pdftools_wasm.mjs` ~95KB
   - `pdftools_wasm.wasm` ~4.3MB
2. Web 单测：`27/27` 通过
3. `npm run lint:node` 通过
4. `./cpp_pdftools/scripts/build_wasm_stub.sh` 通过

---

## 5. 下一步建议

1. 增加一个可复用的“真实 wasm 冒烟脚本”落到仓库（而不是临时命令）。
2. 继续补 `pdf2docx` 真实 wasm 冒烟（当前已是功能接入完成状态，缺真实链路回归脚本）。
3. 若输出文件较大，进一步在客户端增加分块写回策略（当前已用 transfer list 但仍是整块内存）。
