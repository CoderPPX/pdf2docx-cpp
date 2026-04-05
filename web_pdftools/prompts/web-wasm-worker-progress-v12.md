# Web PDFTools WASM Worker 进展（v12）

更新时间：2026-04-05

## 1. 本轮新增（在 v11 基础上）

1. Settings 新增开关：`WASM 失败时自动回退 Native CLI`（默认开启，持久化）。
2. 侧栏新增 `WASM Bridge 统计` 卡片，展示最近一次任务关键指标。
3. renderer 日志与统计卡片联动更新。
4. lint / unit tests / 真实 wasm smoke 继续通过。

---

## 2. 设置项新增

文件：`web_pdftools/index.html`
1. Settings 对话框新增 checkbox：`settings-wasm-fallback`。

文件：`web_pdftools/renderer.mjs`
1. 新增存储键：`web_pdftools.wasm.autoFallback`。
2. 新增状态变量：`wasmAutoFallbackEnabled`。
3. 新增函数：
   - `normalizeBooleanStorage(...)`
   - `applyWasmFallbackSetting(...)`
   - `initWasmFallbackSettings()`
4. 保存 Settings 时会持久化该开关。
5. 执行任务时：
   - 开关 ON：符合条件时自动 fallback
   - 开关 OFF：日志提示可在 Settings 中开启

---

## 3. 统计卡片新增

文件：`web_pdftools/index.html`
1. 新增卡片字段：
   - 状态
   - 任务
   - 阶段
   - 耗时
   - 文件 in/out
   - 字节 in/out
   - 更新时间

文件：`web_pdftools/styles.css`
1. 新增 `bridge-stats-*` 样式与移动端适配。

文件：`web_pdftools/renderer.mjs`
1. 新增：
   - `formatBytes(...)`
   - `updateBridgeStatsView(...)`
2. 在任务执行中/成功/失败时更新卡片。

---

## 4. 回归结果

1. `npm run lint:node` 通过
2. `npm test` 通过（`29/29`）
3. `npm run smoke:wasm -- --op pdf2docx ...` 通过
   - docx: `2661 bytes`
   - ir: `8699 bytes`

---

## 5. 当前状态总结

1. wasm 任务链路：
   - 编辑 + pdf2docx 已可运行
2. 可观测性：
   - 日志 + 卡片双路径
3. 稳定性：
   - 错误分层 + 可配置自动 fallback

---

## 6. 下一步建议

1. 给统计卡片增加“最近 N 次平均值”。
2. 增加 fallback 开关的单元测试（renderer 级）。
3. 若你希望彻底禁用自动回退，可把默认值改为 OFF 并提示首次配置。
