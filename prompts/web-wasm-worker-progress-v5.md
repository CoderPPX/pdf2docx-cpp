# Web PDFTools WASM Worker 进展（v5）

更新时间：2026-04-04

## 1. 本轮新增完成

1. 已打通 `renderer -> wasm worker -> wasm FS -> renderer -> host file` 的最小桥接链路。
2. 新增主进程写文件 IPC：`fs:writeFile`。
3. wasm worker backend 支持 host-fs bridge 协议：
   - 输入文件注入 wasm FS
   - 执行 C ABI 任务
   - 从 wasm FS 收集输出并回传主线程
4. `task_backends` 增加 bridge 编排：
   - 把宿主路径任务映射到 wasm 内部路径（`/work/in/*`, `/work/out/*`）
   - 自动读取输入并写回输出
5. 单测扩展并全绿。

---

## 2. 关键代码改动

## 2.1 IPC 文件读写

文件：`web_pdftools/preload.js`
1. 新增 `writeFile(payload)` 暴露。

文件：`web_pdftools/main.js`
1. 新增 `fs:writeFile` handler。
2. 支持多种二进制输入格式归一化：`Uint8Array/ArrayBuffer/Buffer-like`。
3. 写文件前自动 `mkdir -p` 父目录。

## 2.2 wasm backend（worker 内）

文件：`web_pdftools/wasm/pdftools_wasm_backend.mjs`
1. 新增 host bridge payload 识别：`__hostFsBridge: true`。
2. 新增 wasm FS 操作：
   - 递归建目录
   - staged input 写入
   - output 收集读取
3. bridge 结果结构：
   - `bridge: true`
   - `wasmResult`
   - `outputFiles[{path,hostPath,data}]`

## 2.3 renderer backend 编排层

文件：`web_pdftools/backends/task_backends.mjs`
1. wasm worker backend 增加 host-fs bridge 映射逻辑。
2. 已接入的 bridge 任务：
   - `merge`
   - `delete-page`
   - `insert-page`
   - `replace-page`
3. 运行流程：
   - 读取 host 输入文件 -> 发送 worker bridge payload
   - 收到输出二进制 -> 调用 `api.writeFile` 回写 host 输出路径
4. 保留非 bridge 任务直通模式（例如后续 `pdf2docx` 可先走直通再逐步桥接）。

---

## 3. 测试新增与结果

新增/更新：
1. `web_pdftools/tests/pdftools_wasm_backend.test.mjs`
   - 新增 host-fs bridge 用例。
2. `web_pdftools/tests/task_backends.test.mjs`
   - 新增 wasm backend bridge 编排用例（验证 read/write 调用）。
3. `web_pdftools/tests/fixtures/mock_emscripten_module.mjs`
   - 新增 mock FS 与 `bridge-copy` 行为。

执行结果：
1. `npm run lint:node` 通过
2. `npm test` 通过
3. 单测：`25/25` 通过

---

## 4. wasm 构建状态

1. `./cpp_pdftools/scripts/build_wasm_stub.sh` 通过。
2. 最新产物：
   - `web_pdftools/wasm/dist/pdftools_wasm.mjs` (~91KB)
   - `web_pdftools/wasm/dist/pdftools_wasm.wasm` (~2.0MB)
3. ccache 命中率持续生效（约 63% direct hit）。

---

## 5. 当前限制与下一步

1. 当前 bridge 只覆盖 PDF 编辑任务；`pdf2docx` 仍未接入 wasm C ABI。
2. 目前输出回传走结构化 clone（未做 Transferable 优化），大文件会有额外拷贝成本。
3. 建议下一步：
   - 在 C ABI 中接入 `pdf2docx` 分发
   - 为 worker postMessage 增加 transfer list 优化大文件回传
   - 增加端到端 Electron 场景测试（真实 PDF 输入输出校验）
