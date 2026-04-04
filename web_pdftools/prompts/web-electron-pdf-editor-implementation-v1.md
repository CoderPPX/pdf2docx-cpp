# Web/Electron PDF 编辑器实现方案（含 PDF -> DOCX，V1）

- 更新时间：`2026-04-04`
- 目标目录：`/home/zhj/Projects/pdftools/web_pdftools`
- 核心要求：桌面端（Electron）+ Web 技术栈 + 必须支持 `PDF -> DOCX`

---

## 1. 目标与范围

## 1.1 MVP 目标（第一阶段可交付）

1. 本地打开 PDF，支持缩放、翻页、缩略图导航。
2. 页面级编辑：删除页、插入页、替换页、合并 PDF。
3. 标注级编辑：高亮、下划线、文本注释、矩形批注（非内容重排）。
4. 导出与转换：
   - 保存编辑后 PDF。
   - `PDF -> DOCX`（必须功能）。
5. 任务中心：显示进度、成功/失败状态、日志与输出路径。

## 1.2 暂不纳入 MVP（放到二期）

1. 类 Word 的“直接改 PDF 文字并自动回流排版”。
2. OCR 驱动的扫描件深度重建（可后续接入）。
3. 在线协作与云同步。

说明：MVP 先保证“稳定可用 + 转换可交付”，避免首版陷入高难度排版编辑。

---

## 2. 总体架构

采用 `Electron + React + TypeScript`，并复用仓库现有 `cpp_pdftools` 二进制能力。

```text
Renderer (React)
  -> IPC (typed channel)
Main Process (Electron)
  -> Task Queue + Worker Threads
  -> Child Process: pdftools CLI (C++ binary)
  -> File System / Temp / Output
```

## 2.1 分层职责

1. Renderer：UI、参数输入、任务发起、结果展示。
2. Preload：暴露最小安全 API，隔离 Node 能力。
3. Main：参数校验、任务调度、生命周期管理。
4. Native Tool Adapter：封装 `pdftools` 命令调用与输出解析。
5. Storage 层：最近文件、任务历史、用户设置。

## 2.2 关键原则

1. 重计算在主进程/worker，Renderer 不跑重 CPU 任务。
2. 所有文件操作走 Main，不在 Renderer 直接读写磁盘。
3. 统一任务协议，编辑任务与转换任务共用队列和状态机。

---

## 3. 技术选型（建议）

1. 桌面壳：`Electron`。
2. 前端：`React + TypeScript + Vite`。
3. 状态管理：`Zustand`（轻量、易拆模块）。
4. PDF 预览：`pdf.js`（`pdfjs-dist`）。
5. PDF 标注写回（MVP）：`pdf-lib`（仅处理标注/简单对象）。
6. 核心 PDF 能力与 PDF->DOCX：复用 `cpp_pdftools` CLI。
7. 打包：`electron-builder`（按平台附带 `pdftools` 二进制）。

说明：`PDF -> DOCX` 不建议在 JS 侧重写，优先复用已有 C++ 能力，交付速度与质量更稳。

---

## 4. 与现有 `cpp_pdftools` 的集成方案

当前仓库已存在命令（可直接对接）：

1. `pdftools merge ...`
2. `pdftools page delete|insert|replace ...`
3. `pdftools pdf info --input ...`
4. `pdftools text extract ...`
5. `pdftools convert pdf2docx --input ... --output ... [--dump-ir] [--no-images] [--docx-anchored]`

## 4.1 Electron 侧适配器接口

```ts
interface PdfToDocxOptions {
  inputPdf: string;
  outputDocx: string;
  extractImages?: boolean;      // default true
  anchoredImages?: boolean;     // maps to --docx-anchored
  dumpIrJsonPath?: string;      // maps to --dump-ir
}

interface TaskResult {
  ok: boolean;
  code: number;
  stdout: string;
  stderr: string;
  outputPath?: string;
}
```

## 4.2 参数映射规则（重点）

1. `extractImages=false` -> 追加 `--no-images`。
2. `anchoredImages=true` -> 追加 `--docx-anchored`。
3. `dumpIrJsonPath` 非空 -> 追加 `--dump-ir <path>`。
4. 默认不开启破坏性覆盖；如输出存在，先提示用户确认。

## 4.3 任务进度策略

首版建议：
1. 阶段进度：`排队 -> 启动 -> 转换中 -> 打包结果 -> 完成`。
2. 解析 `stdout` 最终统计（pages/images/warnings）。
3. 失败时保留 `stderr` 原文，供问题定位。

二期可做：
1. 在 C++ 工具增加 JSON 流式进度输出。
2. Electron 实时显示页级进度。

---

## 5. 目录结构建议（`web_pdftools`）

```text
web_pdftools/
  apps/
    desktop/
      src/
        main/                # Electron main + task queue
        preload/             # secure bridge
        renderer/            # React app
      resources/
        bin/
          linux-x64/pdftools
          win-x64/pdftools.exe
          mac-arm64/pdftools
  packages/
    shared-types/            # IPC types / task contracts
    pdftools-adapter/        # CLI command builder + parser
  prompts/
    web-electron-pdf-editor-implementation-v1.md
```

---

## 6. 核心功能设计

## 6.1 PDF 编辑（MVP）

1. 页面编辑：删除/插入/替换/合并，全部走 `pdftools` CLI。
2. 标注编辑：Renderer 记录 annotation model，保存时由 `pdf-lib` 写回。
3. 文件保存：
   - `另存为` 默认；
   - `覆盖保存` 需显式确认。

## 6.2 PDF -> DOCX（必须能力）

1. 主路径：调用 `pdftools convert pdf2docx`。
2. 输出校验：检查 `.docx` 文件存在且大小 > 0。
3. 失败处理：
   - 展示错误日志；
   - 支持一键重试；
   - 可选“关闭图片提取后重试”（`--no-images`）。

## 6.3 批量任务

1. 支持多文件批量转换。
2. 队列并发默认 `1`（稳定优先），可在设置中开到 `2`。
3. 每个任务独立输出目录，避免产物覆盖。

---

## 7. 安全与稳定性

1. Electron 安全基线：
   - `contextIsolation: true`
   - `nodeIntegration: false`
   - 禁止远程代码注入（CSP + 禁用 `eval`）
2. IPC 白名单：仅暴露业务 API，不透传 shell。
3. 路径安全：
   - 规范化并校验路径；
   - 禁止非预期目录写入（尤其临时目录清理时）。
4. 子进程执行：
   - `spawn(file, args)`，不拼接 shell 字符串；
   - 超时与取消机制（kill + 状态回收）。

---

## 8. 测试方案

## 8.1 自动化测试

1. 单元测试：命令构建器、参数校验器、stdout/stderr 解析器。
2. 集成测试：
   - 以样例 PDF 调用本地 `pdftools`，断言输出文件。
   - 覆盖 `pdf2docx` 常见选项组合。
3. E2E（Playwright）：
   - 打开文件 -> 执行转换 -> 任务中心显示完成。

## 8.2 回归样本集（最小）

1. 纯文本 PDF。
2. 图文混排 PDF。
3. 大体积 PDF（100+ 页）。
4. 含异常字体/编码 PDF。

---

## 9. 里程碑排期（建议 6~8 周）

1. 第 1-2 周：工程脚手架、IPC 框架、PDF 预览、任务中心骨架。
2. 第 3-4 周：页面编辑链路接通（merge/delete/insert/replace），本地保存。
3. 第 5-6 周：`PDF -> DOCX` 全链路、批量任务、错误恢复与日志。
4. 第 7-8 周：跨平台打包、回归测试、性能优化与发布准备。

---

## 10. 风险与应对

1. 风险：复杂 PDF 转换质量波动。
   - 应对：提供参数开关（是否提图/锚定图），并沉淀失败样本回归集。
2. 风险：跨平台二进制分发复杂。
   - 应对：CI 按平台构建 `pdftools`，打包到 `resources/bin/<platform>`。
3. 风险：大文件转换耗时长导致 UI 卡顿感。
   - 应对：严格异步队列 + 可取消 + 明确阶段进度。

---

## 11. 首版验收标准（Definition of Done）

1. 用户可在 GUI 完成：打开 PDF、执行页面编辑、保存输出。
2. 用户可在 GUI 完成：单文件与批量 `PDF -> DOCX`。
3. 任务失败时可查看完整错误日志并重试。
4. Windows/macOS/Linux 至少各一套构建产物可运行。
5. 自动化测试通过，包含 `pdf2docx` 集成测试。

---

## 12. 下一步可直接开工的任务清单

1. 初始化 `apps/desktop`（Electron + React + TS + Vite）。
2. 建立 `packages/pdftools-adapter`，先接通 `pdf info` 与 `convert pdf2docx`。
3. 完成任务中心状态机（queued/running/success/failed/cancelled）。
4. 增加最小 E2E：导入 PDF -> 转 DOCX -> 校验输出存在。
