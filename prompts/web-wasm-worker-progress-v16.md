# Web PDFTools WASM Worker 进展（v16）

更新时间：2026-04-05

## 1. 本轮新增（在 v15 基础上）

1. `WASM Bridge 统计` 卡片已完成“清空统计”交互：
   - 清空近 5 次耗时窗口（avg）
   - 清空近 5 次成功率窗口（success）
   - 重置任务/阶段/耗时/IO/fallback 展示为初始值
2. 新增 bridge 错误路径单测（wasm backend 层）：
   - 输出描述缺失 `path` -> `BAD_REQUEST`
   - 期望输出文件不存在 -> `IO_ERROR` + `context=wasm.fs.readFile`
3. 重新编译 wasm 并实测 smoke（merge + pdf2docx）全部通过。

---

## 2. 关键代码改动

文件：`web_pdftools/renderer.mjs`
1. 新增 DOM 引用：`bridgeStatsResetEl`。
2. 新增 `buildDefaultBridgeStatsState()`，统一统计面板默认状态。
3. 新增 `resetBridgeStats({ log })`：
   - 清空 `bridgeHistoryTotals`、`bridgeHistoryOutcomes`
   - 刷新 UI 到默认态
4. 新增 `bindBridgeStatsControls()`：绑定 `#bridge-stats-reset` 点击重置。
5. `bootstrap()` 改为调用 `resetBridgeStats({ log: false })`，避免初始化与手动重置逻辑分叉。

文件：`web_pdftools/tests/pdftools_wasm_backend.test.mjs`
1. 新增 2 条用例：
   - `host fs bridge rejects output spec without path`
   - `host fs bridge maps missing wasm output file to IO_ERROR`

---

## 3. 验证结果

1. 语法检查：
   - `cd web_pdftools && npm run lint:node` 通过
2. 单测：
   - `cd web_pdftools && npm test` 通过
   - 通过数：`31/31`
3. wasm 重编译：
   - `./cpp_pdftools/scripts/build_wasm_stub.sh` 通过
   - 产物：
     - `web_pdftools/wasm/dist/pdftools_wasm.mjs`（约 95KB）
     - `web_pdftools/wasm/dist/pdftools_wasm.wasm`（约 4.3MB）
4. smoke：
   - merge：输出 `/tmp/pdftools_smoke_merge.pdf`，约 `793322` bytes
   - pdf2docx：输出 `/tmp/pdftools_smoke.docx`（`1751` bytes）+ `/tmp/pdftools_smoke.ir.json`（`5986` bytes）

---

## 4. 当前状态判断

1. wasm worker 主路径稳定：构建可重复、bridge I/O 可用、核心任务可跑。
2. 错误模型和日志可观测性继续增强，便于与另一条 cpp 线程并行联调。
3. 统计面板已具备“快速清零后重新观测”的闭环能力。

---

## 5. 下一步建议（按优先级）

1. 把 bridge 统计做持久化（localStorage 可选开关），支持重启后保留最近窗口。
2. 给 fallback 增加“最近回退时间”字段，减少排查时的时间线歧义。
3. 增加一组端到端 Electron UI 自动化（至少覆盖：开 tab -> merge -> 预览 -> fallback 展示）。
