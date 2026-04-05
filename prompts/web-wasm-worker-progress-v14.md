# Web PDFTools WASM Worker 进展（v14）

更新时间：2026-04-05

## 1. 本轮新增（在 v13 基础上）

1. 统计卡片新增 `近5次成功率`。
2. 统计维度形成完整闭环：
   - 近5次平均耗时
   - 近5次成功率
3. 成功/失败都参与稳定性统计（针对 wasm bridge 任务）。
4. lint + unit tests 全通过。

---

## 2. 关键改动

文件：`web_pdftools/index.html`
1. 统计卡片新增字段 `bridge-stat-success`。

文件：`web_pdftools/renderer.mjs`
1. 新增历史数组：`bridgeHistoryOutcomes`（最大 5 条）。
2. 新增：
   - `recordBridgeOutcome(ok)`
   - `formatBridgeSuccessRate()`
3. `updateBridgeStatsView(...)` 支持更新 `success` 字段。
4. 执行流程：
   - bridge 成功：`recordBridgeOutcome(true)`
   - wasm bridge 失败：`recordBridgeOutcome(false)`
5. 卡片展示样式：`x/5 (xx%)`。

---

## 3. 回归结果

1. `npm run lint:node` 通过
2. `npm test` 通过（`29/29`）

---

## 4. 当前状态

1. bridge 观测：
   - 单次指标（耗时/文件/字节/阶段）
   - 近5次均值
   - 近5次成功率
2. 失败处理：
   - 可配置自动 fallback
   - 失败阶段可定位

---

## 5. 下一步建议

1. 可在卡片增加“最近一次回退是否发生”标记。
2. 为 fallback 结果补一条轻量 telemetry（回退触发次数）。
3. 若需要自动化对比，可把成功率/均值写入日志文件用于离线分析。
