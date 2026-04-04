# `web_pdftools` Roadmap 进度总结（V1）

- 更新时间：`2026-04-04`
- 汇总范围：
  1. `web-electron-pdf-editor-implementation-v1.md`
  2. `web-cpp-pdftools-wasm-roadmap-and-testcases-v1.md`

---

## 1. 已完成内容（Done）

## 1.1 Electron/Web 主线（已落地）

1. 工程已初始化为可运行 Electron 应用（`main.js + preload.js + index.html + renderer.mjs`）。
2. 已接入 `cpp_pdftools` 命令调用（主进程 `spawn`）：
   - `merge`
   - `page delete`
   - `page insert`
   - `page replace`
   - `convert pdf2docx`
3. `pdf2docx` 参数映射已实现：
   - `--no-images`
   - `--docx-anchored`
   - `--dump-ir`
4. PDF 预览已改为 `pdf.js`（`pdfjs-dist/legacy`）canvas 渲染。
5. 预览支持多 Tab：打开、切换、关闭当前、关闭全部。
6. 文件操作已从网页菜单迁移到 Electron 原生 menubar（`File`）：
   - Open PDF...
   - Close Current Tab
   - Close All Tabs
7. 工具侧栏已支持“从已打开 Tab 选择输入”。
8. 预览控制（页码/缩放）已实现，并已按要求下移到预览区底部。
9. 任务日志已实现，显示命令、stdout、stderr、成功/失败。
10. 安全基线已满足核心项：
    - `contextIsolation: true`
    - `nodeIntegration: false`
    - `sandbox: true`
11. `pdftools` 二进制自动探测已实现（环境变量 / build 目录 / PATH）。
12. 基础可用性检查已做：`npm run lint:node`。

## 1.2 与 roadmap 对齐情况（阶段视角）

1. 对齐 `MVP` 核心：
   - PDF 打开预览
   - 页面级编辑（merge/delete/insert/replace）
   - PDF -> DOCX
2. 对齐“复用 `cpp_pdftools`”主策略：已执行。
3. 对齐“任务结果可见”主策略：日志已可追踪。

---

## 2. 未完成内容（Todo）

## 2.1 Electron/Web 主线（尚未落地）

1. 未实现“标注级编辑”（高亮/下划线/注释/矩形批注）。
2. 未实现“批量转换任务”和队列并发配置。
3. 未实现“任务中心状态机”（queued/running/success/failed/cancelled）可视化。
4. 未实现“任务取消、超时控制、重试策略 UI”。
5. 未实现“输出覆盖确认/冲突处理”完整流程。
6. 未实现 `pdf info` / `text extract` / `attachments extract` UI 入口。
7. 未实现自动化测试：
   - 单元测试（命令构建/解析）
   - 集成测试
   - E2E（Playwright）
8. 未实现跨平台打包发布（`electron-builder`/artifact 分发）。
9. 当前实现未采用 roadmap 建议的 `React + TypeScript + Vite` 结构，仍是原生 HTML/JS。

## 2.2 WASM 主线（基本未启动）

1. `M0~M5` 均未进入开发态。
2. 未新增 Emscripten preset。
3. 未实现内存化 API（`*FromBytes`）。
4. 未实现 C ABI / JS SDK / Worker 协议。
5. 未实现 `ExtractImages` 公共 API（`pdftools::pdf` 层）。
6. 未执行 WASM 路线测试矩阵（B/M/D/I/R/E/S/P 系列用例）。

---

## 3. 偏差说明（Roadmap vs 当前实现）

1. 架构偏差：roadmap 规划 `React + TS + Vite`，当前为轻量 Electron + 原生 JS。
2. 任务系统偏差：roadmap规划明确任务队列与进度协议，当前是“串行执行 + 日志输出”。
3. 质量保障偏差：roadmap规划自动化测试矩阵，当前仅语法检查。
4. WASM 偏差：roadmap已成文，但尚未开始代码实现。

---

## 4. 建议下一步（按优先级）

1. **P0**：补“任务中心状态机 + 批量任务 + 取消/重试”。
2. **P0**：补自动化集成测试（至少覆盖 merge/delete/insert/replace/pdf2docx）。
3. **P1**：补 `pdf info` / `text extract` / `attachments extract` 的侧栏入口。
4. **P1**：将当前 UI 迁移到 `React + TypeScript`（与 roadmap 技术栈对齐）。
5. **P2**：启动 WASM `M0`（先做 `GetPdfInfo` 编译贯通验证）。

---

## 5. 当前结论

1. **可用状态**：Electron 本地工具链已可用于“PDF 预览 + 页面编辑 + PDF->DOCX”。
2. **未闭环部分**：测试、批量任务、任务治理、跨平台发布、WASM 全链路。
3. **整体进度（主观估算）**：
   - Electron/Web 主线：`~45%`
   - WASM 主线：`~5%`（仅文档与规划）
