# Web PDFTools WASM Worker 进展（v11）

更新时间：2026-04-05

## 1. 本轮新增（在 v10 基础上）

1. 页面新增 `WASM Bridge 统计` 卡片，展示最近一次 bridge 任务核心指标。
2. renderer 已接入 bridge 指标更新与失败阶段展示。
3. 新增“WASM 失败自动回退 Native CLI”逻辑（可回退错误码触发）。
4. lint / unit tests / 真实 wasm merge smoke 全部通过。

---

## 2. UI 新增：WASM Bridge 统计卡片

文件：`web_pdftools/index.html`
1. 侧栏状态区域新增卡片：
   - 状态（pending/ok/error）
   - 任务
   - 阶段
   - 总耗时
   - 文件 in/out
   - 字节 in/out
   - 更新时间

文件：`web_pdftools/styles.css`
1. 新增 `bridge-stats-*` 样式。
2. 移动端下卡片栅格自动 1 列。

文件：`web_pdftools/renderer.mjs`
1. 新增 `updateBridgeStatsView(...)` 与 `formatBytes(...)`。
2. bridge 任务执行时：
   - 开始：状态 `执行中`
   - 成功：展示 io 与 timings 指标
   - 失败：展示失败阶段

---

## 3. 自动回退：WASM -> Native CLI

文件：`web_pdftools/renderer.mjs`

新增策略：
1. 当当前 backend=wasm 且任务失败，若错误码在回退白名单中，则自动尝试 native-cli。
2. 回退白名单覆盖：
   - `UNSUPPORTED_FEATURE`
   - `BACKEND_RESERVED`
   - `WASM_RUN_FAILED`
   - `WASM_OP_FAILED`
   - `WASM_API_MISSING`
   - `WORKER_RUNTIME_ERROR`
   - `WORKER_CLOSED`
   - `WORKER_UNAVAILABLE`
   - `NOT_INITIALIZED`
   - `IO_ERROR`
3. 回退成功后：
   - 日志输出 fallback 命令与结果
   - 返回 native 成功结果给上层流程
   - bridge 卡片状态显示 `WASM失败已回退`

---

## 4. 回归结果

1. `npm run lint:node` 通过
2. `npm test` 通过（`29/29`）
3. `npm run smoke:wasm -- --op merge ...` 通过

---

## 5. 当前状态

1. wasm 侧：
   - 编辑 + pdf2docx 均可走 bridge
2. web 侧：
   - 指标可观测
   - 错误分层
   - 自动 fallback
3. 测试与 smoke：
   - 持续通过

---

## 6. 下一步建议

1. 给 fallback 增加开关（Settings 中可配置自动回退 on/off）。
2. 统计卡片增加“最近 N 次平均耗时”。
3. 增加 e2e UI 测试覆盖 fallback 路径。
