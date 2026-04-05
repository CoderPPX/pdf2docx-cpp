# Web PDFTools WASM Worker 进展（v6）

更新时间：2026-04-04

## 1. 本轮新增完成

1. 在 `v5` 基础上继续完成 worker 大文件回传优化：已支持 `Transferable`。
2. host-fs bridge 链路维持可用：
   - renderer 读宿主文件 -> worker 写入 wasm FS -> C ABI 执行 -> worker 读取输出 -> renderer 回写宿主文件。
3. 单测继续扩展，当前全部通过。

---

## 2. 本轮关键改动

## 2.1 worker dispatch 传输优化

文件：`web_pdftools/wasm/worker_dispatcher.mjs`
1. 新增 response 扫描逻辑，自动收集 payload 中的 `ArrayBuffer/TypedArray`。
2. `handleMessage` 现在向回调传递 `(response, transferables)`。
3. 对 `ok=true` 响应启用 transfer list，减少大输出拷贝开销。

文件：`web_pdftools/wasm/pdftools.worker.mjs`
1. `self.postMessage(response, transferables)`。

文件：`web_pdftools/wasm/pdftools.node_worker.mjs`
1. `parentPort.postMessage(response, transferables)`。

## 2.2 host-fs bridge（延续）

文件：`web_pdftools/backends/task_backends.mjs`
1. wasm bridge 任务映射仍覆盖：
   - `merge`
   - `delete-page`
   - `insert-page`
   - `replace-page`
2. 运行后自动回写 `outputFiles` 到宿主路径。

文件：`web_pdftools/wasm/pdftools_wasm_backend.mjs`
1. bridge payload：`__hostFsBridge=true`。
2. worker 内 wasm FS 分阶段：mkdir -> write input -> invoke -> read output。

## 2.3 IPC 文件写入（延续）

文件：`web_pdftools/main.js`, `web_pdftools/preload.js`
1. 已启用 `fs:writeFile` 与 `api.writeFile`。

---

## 3. 测试变化

新增/增强：
1. `web_pdftools/tests/wasm_worker_dispatcher.test.mjs`
   - 新增 `run response forwards transferable buffers` 用例。
2. 其余 bridge 测试保留：
   - `pdftools_wasm_backend.test.mjs`（worker 内 FS bridge）
   - `task_backends.test.mjs`（renderer 侧 bridge 编排）

执行结果：
1. `npm run lint:node` 通过
2. `npm test` 通过
3. 单测：`26/26` 通过

---

## 4. 当前状态总结

1. wasm C ABI 已接入 PDF 编辑核心操作（merge/delete/insert/replace）。
2. web 端已具备“本地路径任务经 bridge 调用 wasm”的可运行基础。
3. 性能侧已处理回传阶段的 transfer list，降低大输出复制成本。

---

## 5. 下一步建议

1. 接入 `pdf2docx` 到 wasm C ABI。
2. 为 bridge 增加更细粒度错误定位（输入读取失败/回写失败分层 code）。
3. 增加 Electron 端到端冒烟测试（真实 PDF 输入输出比对）。
