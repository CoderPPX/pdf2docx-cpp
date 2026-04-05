# Web PDFTools WASM Worker 进展（v15）

更新时间：2026-04-05

## 1. 本轮新增（在 v14 基础上）

1. 统计卡片新增字段：`最近回退`（是否触发 fallback）。
2. 统计卡片当前字段完整为：
   - 最近任务状态
   - 任务/阶段/耗时
   - 文件 in/out
   - 字节 in/out
   - 近5次均值
   - 近5次成功率
   - 最近回退
3. lint + unit tests 继续通过。

---

## 2. 关键实现

文件：`web_pdftools/index.html`
1. 新增 `bridge-stat-fallback` 展示位。

文件：`web_pdftools/renderer.mjs`
1. 新增 `bridgeStatFallbackEl`。
2. `updateBridgeStatsView(...)` 支持 `fallback` 字段更新。
3. 状态更新规则：
   - bridge 成功：`fallback=否`
   - wasm 失败并回退成功：`fallback=是`
   - bridge 失败（未回退）：`fallback=否`
   - 执行中/初始：`fallback=--`

---

## 3. 验证

1. `npm run lint:node` 通过
2. `npm test` 通过（`29/29`）

---

## 4. 当前可观测性面板能力

1. 性能：单次耗时 + 近5次均值
2. 稳定性：近5次成功率
3. 兜底行为：最近是否发生 fallback
4. 诊断：失败阶段（prepare_input / worker_run / persist_output）

---

## 5. 下一步建议

1. 给“最近回退”补充时间戳（何时触发）。
2. 若需要长期分析，可把每次 bridge 指标写入本地日志 JSON。
3. 可增加一个“清空统计”按钮方便手动重置观察窗口。
