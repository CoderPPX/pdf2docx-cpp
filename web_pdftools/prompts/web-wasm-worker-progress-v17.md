# Web PDFTools WASM Worker 进展（v17）

更新时间：2026-04-05

## 1. 本轮最终新增（在 v16 基础上）

1. `WASM Bridge 统计` 卡片新增字段：`回退时间`（`bridge-stat-fallback-time`）。
2. 当发生 `WASM -> Native CLI` 自动回退并成功时：
   - `最近回退 = 是`
   - `回退时间 = 当前时间`
3. 点击“清空统计”时会一并重置回退时间为 `--`。

---

## 2. 本轮关键实现汇总

### 2.1 统计面板交互完善

文件：`web_pdftools/renderer.mjs`
1. 新增：`bridgeStatsResetEl` 绑定；实现 `bindBridgeStatsControls()`。
2. 新增：`buildDefaultBridgeStatsState()` 统一默认状态。
3. 新增：`resetBridgeStats({ log })` 清空统计窗口并刷新 UI。
4. 新增：`lastFallbackAt` 状态；新增展示字段 `fallbackTime`。
5. 回退成功分支写入 `lastFallbackAt = now()` 并刷新统计卡片。
6. `bootstrap()` 统一通过 `resetBridgeStats({ log: false })` 初始化。

文件：`web_pdftools/index.html`
1. bridge 统计网格新增：
   - 标签：`回退时间`
   - 元素：`#bridge-stat-fallback-time`

### 2.2 单测增强

文件：`web_pdftools/tests/pdftools_wasm_backend.test.mjs`
1. 新增错误分支用例：bridge 输出描述缺失 `path`。
2. 新增错误分支用例：bridge 读取缺失 wasm 输出文件。

---

## 3. 构建/测试/实测结果

1. `cd web_pdftools && npm run lint:node`：通过
2. `cd web_pdftools && npm test`：通过（`31/31`）
3. `./cpp_pdftools/scripts/build_wasm_stub.sh`：通过
   - `web_pdftools/wasm/dist/pdftools_wasm.mjs` ~95KB
   - `web_pdftools/wasm/dist/pdftools_wasm.wasm` ~4.3MB
4. smoke（真实 wasm）：
   - merge：`/tmp/pdftools_smoke_merge.pdf`（`793322` bytes）
   - pdf2docx：`/tmp/pdftools_smoke.docx`（`1751` bytes）
   - dump ir：`/tmp/pdftools_smoke.ir.json`（`5986` bytes）

---

## 4. 当前状态（给下一个 LLM）

1. wasm worker 主通路可重复构建，bridge I/O 已可用于核心任务。
2. UI 侧统计能力现在包含：性能、成功率、fallback 标记、fallback 时间、失败阶段。
3. 统计支持手动清空，便于并行调试时观察窗口重新起算。

---

## 5. 建议下一步

1. bridge 统计加入持久化开关（localStorage）并提供“启动时恢复最近窗口”。
2. 增加 Electron 端到端冒烟（打开 tab -> 任务执行 -> fallback 展示检查）。
3. 与 cpp 线程联调后补全 `extract-images` 等 wasm 任务映射并补 smoke。
