# Webpage 进度交接（Electron + pdf.js）V2

- 更新时间：2026-04-04
- 适用范围：`/home/zhj/Projects/pdftools/web_pdftools`
- 目标：给下一个 LLM 直接接手 Web 页面与 Electron 端。

## 1. 当前状态总览

当前 `web_pdftools` 已经是可运行的 Electron 页面应用，完成了：

1. `pdf.js` 预览渲染（canvas）
2. 多 Tab 打开/切换/关闭 PDF
3. 文件操作从网页按钮迁移到 Electron 原生 menubar（File）
4. 侧边栏提供 PDF 编辑与 PDF->DOCX 操作
5. 所有主要输入支持“从已打开 Tab 选择”
6. 预览信息与翻页/缩放控件已下移到页面底部区域

## 2. 已完成需求对照

对应用户需求“页面主要空间用于预览 + 侧边栏工具 + 文件菜单并入原生 menubar + Tab 选择输入”：

1. 页面布局已重构为双栏：左工具栏、右预览区。
2. 预览区支持多 Tab，默认主空间给预览。
3. 网页顶部大标题区已去掉（不再显示“Web PDFTools ...”大栏）。
4. `Open PDF to new tab` 等文件操作已迁移到 `File` 原生菜单。
5. `Close Current Tab`、`Close All Tabs` 已进入原生菜单。
6. 侧栏工具（merge/delete/insert/replace/pdf2docx）都可从已打开 Tab 选输入。
7. 底部显示当前 PDF 路径、页码、缩放，并提供上一页/下一页/缩放按钮。

## 3. 关键代码入口（接手必看）

1. `/home/zhj/Projects/pdftools/web_pdftools/main.js`
- Electron 主进程
- 原生菜单构建（`buildNativeMenu`）
- `menu:action` 事件下发到 renderer
- `pdftools` 二进制探测与子进程调用
- IPC：`pdftools:run`、文件读写、open/save dialog

2. `/home/zhj/Projects/pdftools/web_pdftools/preload.js`
- 安全桥接 API（`window.pdftoolsAPI`）
- renderer 不直接用 Node API

3. `/home/zhj/Projects/pdftools/web_pdftools/renderer.mjs`
- `pdf.js` worker 配置
- 多 Tab 状态管理（`previewTabs`, `activeTabId`）
- 打开/渲染/翻页/缩放逻辑
- 监听 `menu:action`：open/close-current/close-all
- 侧栏操作按钮与任务执行日志

4. `/home/zhj/Projects/pdftools/web_pdftools/index.html`
- 左侧工具卡片、右侧预览、底部预览工具栏

5. `/home/zhj/Projects/pdftools/web_pdftools/styles.css`
- 双栏布局、Tab 样式、预览区域样式、移动端响应式

## 4. 当前可用功能（实际代码）

1. PDF 预览
- 通过 `pdfjs-dist/legacy/build/pdf.mjs`
- 读取文件由 `api.readFile(path)` 完成
- 每个 Tab 维持 `pdfDoc/currentPage/scale`

2. 文件菜单
- `File > Open PDF...`
- `File > Close Current Tab`
- `File > Close All Tabs`

3. PDF 编辑（调用 `cpp_pdftools`）
- merge
- page delete
- page insert
- page replace

4. PDF -> DOCX（调用 `cpp_pdftools`）
- 支持 `--no-images`
- 支持 `--docx-anchored`
- 支持 `--dump-ir`

5. 输入联动
- 单选下拉：选中后自动填充输入框并聚焦到该 PDF Tab
- merge 支持多选 Tab 批量填充输入

## 5. 已验证项

已在 2026-04-04 重新执行：

1. `node --check main.js` 通过
2. `node --check preload.js` 通过
3. `node --check renderer.mjs` 通过
4. `npm run lint:node` 通过

说明：本轮没有 GUI 自动化点击测试，仍需要桌面环境手动 `npm start` 验证交互。

## 6. 目前缺口 / 风险

1. 还没有“提取图片”UI入口（用户早期需求有提到）。
2. 还没有任务队列（当前是前端串行执行，执行中禁用 primary 按钮）。
3. 还没有任务取消/重试/超时治理。
4. 还没有 E2E 自动化测试（Playwright/Cypress）。
5. 还没有打包发布流程（`electron-builder` 等）。
6. 当前前端是原生 JS，不是 TS/React 架构。

## 7. 下一个 LLM 建议优先级

1. P0：补 `extract images` 功能卡片 + CLI 参数映射 + 结果日志。
2. P0：补“批量任务列表 + 状态机（queued/running/success/failed）”。
3. P1：加最小 E2E 冒烟（打开 PDF、翻页、执行 merge、执行 pdf2docx）。
4. P1：补错误分类（参数错误 vs 二进制缺失 vs 执行失败）。
5. P2：再考虑前端框架迁移（如 TS/React）。

## 8. 快速接手命令

```bash
cd /home/zhj/Projects/pdftools/web_pdftools
npm install
npm start
```

如 `pdftools` 不在默认路径，先设置：

```bash
export PDFTOOLS_BIN=/abs/path/to/pdftools
npm start
```
