# Web PDFTools WASM Worker 进展（v3）

更新时间：2026-04-04

## 1. 本轮完成

1. 把 web backend 架构从“wasm reserved 占位”升级为“真实 wasm worker preview + reserved 兼容模式”。
2. 新增 `cpp_pdftools` wasm C ABI 最小导出并完成编译产物落地。
3. 将 wasm 构建纳入 `web_pdftools` npm script，支持一键重编。
4. 保持现有 JS 单测/语法检查全绿。

---

## 2. 关键实现细节

## 2.1 backend 层

文件：`web_pdftools/backends/task_backends.mjs`

新增/调整：
1. `WasmBackendMode`：
   - `worker`（默认）
   - `reserved`（兼容旧逻辑）
2. `WasmWorkerTaskBackend`：
   - 使用 `createPdftoolsWorkerClient` 启动 worker
   - `start({ moduleUrl })` 指向 `wasm/dist/pdftools_wasm.mjs`
   - `getStatus()` 调用 `ping()` 检测可用性
   - `runTask(task)` 走 worker `run`，并映射成 UI 统一结构
3. 错误映射：
   - 统一返回 `ok/code/stderr/context/details`
   - worker 关闭类错误会触发 backend 内部重置，业务类错误不重置
4. 保留 `WasmReservedTaskBackend` 作为 fallback 模式。

## 2.2 renderer 层

文件：`web_pdftools/renderer.mjs`

改动：
1. `createTaskBackendRegistry(..., { wasmMode: WasmBackendMode.Worker })`
2. 状态展示逻辑更新：
   - wasm ready -> `ok`
   - wasm not ready -> `pending`
3. 任务日志文案更新为 preview 模式提示。
4. 新增生命周期管理：`beforeunload` 时自动 `dispose()` backend（避免 worker 残留）。

## 2.3 settings UI

文件：`web_pdftools/index.html`

更新：
1. backend 选项文案从 `WASM (Reserved)` 改为 `WASM Worker (Preview)`。
2. 提示文案改为“预览模式，功能逐步接入”。

## 2.4 wasm C ABI（cpp）

新增文件：
1. `cpp_pdftools/src/wasm/pdftools_wasm_capi.cpp`

导出：
1. `pdftools_wasm_op`
2. `pdftools_wasm_free`

行为：
1. 支持 `ping/version` 的成功响应。
2. 其它操作返回标准结构化错误：
   - `UNSUPPORTED_FEATURE`
   - `code/message/context/details`
3. details 中包含：
   - `requestBytes`
   - `operation`

## 2.5 wasm 构建脚本

新增：
1. `cpp_pdftools/scripts/build_wasm_stub.sh`

输出：
1. `web_pdftools/wasm/dist/pdftools_wasm.mjs`
2. `web_pdftools/wasm/dist/pdftools_wasm.wasm`

并接入 npm script：
1. `web_pdftools/package.json` 增加 `build:wasm-stub`

执行：
1. `cd web_pdftools && npm run build:wasm-stub`

---

## 3. 测试与验证

已通过：
1. `npm run build:wasm-stub`
2. `npm run lint:node`
3. `npm test`

当前单测：16/16 通过。

新增/更新测试：
1. `web_pdftools/tests/task_backends.test.mjs`
   - reserved 模式
   - worker 模式成功路径
   - worker 模式错误映射路径

---

## 4. 当前限制（重要）

1. C++ wasm 目前是 stub dispatcher：仅 `ping/version` 可成功。
2. `merge/delete/insert/replace/pdf2docx` 仍返回 `UNSUPPORTED_FEATURE`（符合预期）。
3. Node 环境下直接运行该 wasm 模块不是当前目标，主目标是 Electron renderer worker 场景。

---

## 5. 下一步建议（按优先级）

1. 在 `pdftools_wasm_capi.cpp` 中接入真实任务分发（先做 `merge` 和 `page delete`）。
2. 引入轻量 JSON 解析（或手写最小 parser）替换字符串匹配分发。
3. 把 C++ `Status` 直接转换为统一 error payload，减少手工拼 JSON。
4. 增加 wasm 集成测试（真实 wasm 模块 + worker），覆盖：
   - init/ping
   - unsupported
   - 参数错误
5. 最后再将 `pdf2docx` 接入 wasm 路径。

