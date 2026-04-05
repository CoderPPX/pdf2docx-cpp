# Web PDFTools WASM Worker 进展（v13）

更新时间：2026-04-05

## 1. 本轮新增（在 v12 基础上）

1. Settings 增加可配置开关：`WASM 失败时自动回退 Native CLI`。
2. `WASM Bridge 统计` 卡片升级，新增 `近5次均值`。
3. renderer 已将回退开关接入执行路径，支持开启/关闭并持久化。
4. lint / unit tests 继续全绿。

---

## 2. 关键代码改动

## 2.1 Settings 开关

文件：`web_pdftools/index.html`
1. 新增 `settings-wasm-fallback` checkbox。

文件：`web_pdftools/renderer.mjs`
1. 新增 localStorage key：`web_pdftools.wasm.autoFallback`。
2. 新增：
   - `wasmAutoFallbackEnabled` 状态变量
   - `normalizeBooleanStorage(...)`
   - `applyWasmFallbackSetting(...)`
   - `initWasmFallbackSettings()`
3. 保存设置时持久化开关。
4. fallback 执行前检查开关：
   - ON：按错误码自动回退
   - OFF：日志提示可在 Settings 开启

## 2.2 统计卡片增强

文件：`web_pdftools/index.html`
1. 新增字段：`bridge-stat-avg`（近5次均值）。

文件：`web_pdftools/renderer.mjs`
1. 新增历史数组：`bridgeHistoryTotals`（最大 5 条）。
2. 新增：
   - `recordBridgeTotal(...)`
   - `formatBridgeAverage()`
3. bridge 成功后记录总耗时并刷新均值。

文件：`web_pdftools/styles.css`
1. Settings 新增 `.settings-check` 样式。

---

## 3. 执行结果

1. `npm run lint:node` 通过
2. `npm test` 通过（`29/29`）

---

## 4. 当前状态

1. wasm bridge：
   - 任务能力：编辑 + pdf2docx
   - 可观测：单次指标 + 最近 5 次均值
   - 稳定性：分层错误 + 可配置自动回退
2. web UI：
   - Settings 可控行为更完整

---

## 5. 下一步建议

1. 把“自动回退开关”同步展示到主界面状态栏（减少切 settings 频率）。
2. 对 `bridgeHistoryTotals` 增加成功率统计（最近 5 次成功/失败）。
3. 增加 renderer 层回退行为单测（目前主要是 backend/worker 单测）。
