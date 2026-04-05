# Web PDFTools 多 Backend 改造进展（v1）

更新时间：2026-04-04  
范围：`/home/zhj/Projects/pdftools/web_pdftools`

---

## 1. 目标

将 Web PDFTools 的任务执行层从“单一 native CLI 调用”改造成“多 backend 架构”，并在 wasm 尚未完成时保留可切换的占位入口，确保后续 wasm 实现可以无缝接入。

---

## 2. 已完成事项

## 2.1 新增后端抽象层

新增文件：
1. `web_pdftools/backends/task_backends.mjs`

实现内容：
1. `BackendId`
   - `native-cli`
   - `wasm`
2. `resolveBackendId(value, fallback)`
   - 对非法 backend id 自动回退到 `native-cli`
3. `NativeCliTaskBackend`
   - `getStatus()` 通过 preload API 查询 `pdftools` binary 状态
   - `runTask(task)` 透传到 `api.runTask(task)`
4. `WasmReservedTaskBackend`（预留）
   - `getStatus()` 返回 `ready=false` + `BACKEND_RESERVED`
   - `runTask(task)` 返回结构化失败结果，不执行真实任务
5. `createTaskBackendRegistry(api, options)`
   - 返回 backend registry（目前默认包含 `native-cli` + wasm 预留 backend）

---

## 2.2 Renderer 接入 backend registry

修改文件：
1. `web_pdftools/renderer.mjs`

核心改造：
1. 引入 backend 模块并初始化 registry
2. 增加 backend 选择持久化 key：
   - `web_pdftools.backend.mode`
3. 新增 backend 管理逻辑：
   - `getActiveBackend()`
   - `applyBackendMode(...)`
   - `refreshBackendStatus(...)`
   - `initBackendSettings()`
4. `initStatus()` 改为统一走 backend 状态刷新
5. `runTask()` 从固定 `api.runTask()` 改为 `activeBackend.runTask(task)`
6. 日志中附带 backend 信息，例如：
   - `开始任务: merge (backend=native-cli)`
7. 当 backend=wasm（预留）时，任务失败后给出明确提示：
   - “wasm backend 仍是预留位，请切回 Native CLI backend 执行任务。”

---

## 2.3 设置面板支持切换 backend

修改文件：
1. `web_pdftools/index.html`
2. `web_pdftools/styles.css`

变更内容：
1. 设置标题由 `Theme Settings` 调整为 `Theme & Backend Settings`
2. 新增 `Task Backend` 下拉框：
   - `Native CLI Tools`
   - `WASM (Reserved)`
3. 增加说明文案：
   - `WASM backend 当前为预留位，暂不执行真实任务。`
4. 增加 `.settings-note` 样式

---

## 2.4 测试补充

新增文件：
1. `web_pdftools/tests/task_backends.test.mjs`

覆盖点：
1. `resolveBackendId` 非法值回退逻辑
2. native backend 的状态与任务执行返回
3. wasm 预留 backend 的占位行为（状态和失败返回）

同时修改：
1. `web_pdftools/package.json`
   - `lint:node` 增加：
     - `backends/task_backends.mjs`
     - `tests/task_backends.test.mjs`

---

## 3. 验证结果

在 `web_pdftools` 目录执行：

1. `npm run lint:node`：通过
2. `npm test`：通过
   - 总计 `14/14` 通过（包含新增 task_backends 测试）

---

## 4. 当前行为（用户视角）

1. 默认 backend：`Native CLI Tools`
2. 在 `File -> Settings -> Theme & Backend Settings` 中可切换 backend
3. 若切到 `WASM (Reserved)`：
   - 状态区显示“预留中”提示
   - 所有任务执行会返回“未实现”提示，不会触发实际操作
4. 切回 `Native CLI Tools` 后恢复现有功能

---

## 5. 与异常规范的一致性

本次改造与 `prompts/exception_convention.md` 对齐点：

1. backend 层失败返回是结构化结果（而非抛裸字符串）
2. wasm 预留 backend 也保持可观测错误信息
3. renderer 日志具备 backend 上下文，便于后续定位

---

## 6. 已改文件清单

1. `web_pdftools/backends/task_backends.mjs`（新增）
2. `web_pdftools/renderer.mjs`
3. `web_pdftools/index.html`
4. `web_pdftools/styles.css`
5. `web_pdftools/tests/task_backends.test.mjs`（新增）
6. `web_pdftools/package.json`

---

## 7. 下一步建议

1. 用真实 wasm worker client 替换 `WasmReservedTaskBackend` 的占位实现
2. 在 wasm backend 中统一对接 `code/message/context/details` 错误 envelope
3. 增加 wasm 集成测试（真实模块而非 mock）
4. 在 UI 中为 backend 切换增加“能力矩阵”提示（例如 wasm 暂不支持哪些操作）

