# Web PDFTools WASM Worker 进展（v10）

更新时间：2026-04-05

## 1. 本轮新增（在 v9 基础上）

1. 完成 wasm bridge 的可观测性增强：返回输入/输出字节、文件数、分阶段耗时。
2. 完成 wasm bridge 的错误分层：
   - `prepare_input`（宿主输入读取）
   - `worker_run`（worker 执行）
   - `persist_output`（输出回写）
3. renderer 日志已展示 bridge 指标与失败阶段。
4. 单测新增 2 个错误分层场景，当前全绿。
5. 真实 wasm smoke 再次回归（merge/pdf2docx）均通过。

---

## 2. 关键代码改动

## 2.1 backend 可观测性 + 分层错误

文件：`web_pdftools/backends/task_backends.mjs`

新增：
1. 统一时间工具 `nowMs()`。
2. 结构化错误构造 `createStructuredError(...)`。
3. bridge 输入读取失败会带：
   - `code=IO_ERROR`
   - `context=wasm.hostfs.readInput`
4. bridge 输出回写失败会带：
   - `code=IO_ERROR`
   - `context=wasm.hostfs.writeOutput`
5. `runTask` 成功时 `details.bridge` 输出：
   - `ioFiles.input/output`
   - `ioBytes.input/output`
   - `timingsMs.prepareInput/workerRun/persistOutput/total`
6. `runTask` 失败时 `details.stage` 输出：
   - `prepare_input | worker_run | persist_output`

## 2.2 renderer 日志增强

文件：`web_pdftools/renderer.mjs`

新增日志：
1. `WASM bridge: files(in/out), bytes(in/out)`
2. `WASM bridge timings(ms): prepare/run/persist/total`
3. 失败时显示 `失败阶段: <stage>`

---

## 3. 测试新增

文件：`web_pdftools/tests/task_backends.test.mjs`

新增/增强：
1. bridge 成功场景校验 `details.bridge` 指标字段。
2. 新增：`wasm worker bridge input read failure maps to prepare_input stage`
3. 新增：`wasm worker bridge output write failure maps to persist_output stage`

结果：
1. `npm run lint:node` 通过
2. `npm test` 通过
3. 单测 `29/29` 通过

---

## 4. 真实 wasm 回归结果

命令（merge）：
1. `npm run smoke:wasm -- --op merge --input formula.pdf --input formula.pdf --output /tmp/pdftools_wasm_merge_smoke_v2.pdf`
2. 结果：`ok=true`, 输出 `67174 bytes`

命令（pdf2docx）：
1. `npm run smoke:wasm -- --op pdf2docx --input formula.pdf --output /tmp/pdftools_wasm_pdf2docx_smoke_v2.docx --dump-ir /tmp/pdftools_wasm_pdf2docx_smoke_v2.ir.json`
2. 结果：`ok=true`, docx `2661 bytes`, ir `8699 bytes`

---

## 5. 当前状态

1. wasm C ABI：
   - `merge/delete-page/insert-page/replace-page/pdf2docx` 已接入
2. web backend：
   - host-fs bridge 可运行
   - 输出回写可运行
   - 可观测指标与分层错误已接入
3. worker：
   - transfer list 已启用
4. 冒烟：
   - merge/pdf2docx 真实 wasm 均通过

---

## 6. 下一步建议

1. 在 UI 加一个简洁的“任务统计面板”（最近一次 bridge 的耗时与字节量）。
2. 若大文件场景增多，可把输出回写做成分块写入。
3. 将 `smoke:wasm` 纳入 CI 可选 job。
