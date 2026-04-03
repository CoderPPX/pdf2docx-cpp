# `cpp_pdftools` 统一框架 Roadmap（V1）

- 更新时间：`2026-04-03`
- 目标目录：`/home/zhj/Projects/pdftools/cpp_pdftools`
- 依赖约束：优先复用 `thirdparty/`（`podofo`/`zlib`/`minizip-ng`/`tinyxml2`/`freetype2`）

---

## 1. 总目标

构建一个可持续扩展的 C++ `pdftools` 框架，覆盖：
1. PDF 合并/拆分
2. PDF 文本提取
3. 图片转 PDF
4. PDF 附件/嵌入文件提取
5. PDF 增加/删除/替换页面
6. PDF -> DOCX（迁移自 `cpp_pdf2docx`）

并满足：
- 同一套核心库同时支持 CLI 与 GUI
- API 稳定、可测试、可模块化新增功能
- 新功能接入不破坏既有命令与数据结构

---

## 2. 设计原则

1. **能力分层**：`core`（通用）/`domain`（PDF能力）/`adapters`（CLI/GUI）。
2. **统一请求模型**：每个能力都用 `Request/Result + Status`。
3. **同步 + 异步双接口**：CLI 用同步，GUI 用异步任务调度。
4. **可插拔后端**：先以 PoDoFo 为默认后端，后续可扩展 Poppler/PDFium。
5. **兼容迁移**：先迁入 `cpp_pdf2docx` 现有链路，再统一命名与目录。

---

## 3. 目标目录结构（建议）

```text
cpp_pdftools/
  CMakeLists.txt
  CMakePresets.json
  cmake/
  include/pdftools/
    status.hpp
    types.hpp
    task.hpp
    runtime.hpp
    pdf/
      document_ops.hpp
      extract_ops.hpp
      create_ops.hpp
      attachment_ops.hpp
    convert/
      pdf2docx.hpp
  src/
    core/
    runtime/
    backend/podofo/
    pdf/
      document_ops/
      extract_ops/
      create_ops/
      attachment_ops/
    convert/pdf2docx/
    app/
      command_registry/
      task_runner/
  tools/cli/
    main.cpp
    commands/
  tests/
    unit/
    integration/
```

---

## 4. 接口设计（支持 CLI + GUI）

## 4.1 同步接口（核心）

- `Status MergePdf(const MergePdfRequest&, MergePdfResult*)`
- `Status ExtractText(const ExtractTextRequest&, ExtractTextResult*)`
- `Status ImagesToPdf(const ImagesToPdfRequest&, ImagesToPdfResult*)`
- `Status ExtractAttachments(const ExtractAttachmentsRequest&, ExtractAttachmentsResult*)`
- `Status AddPage/DeletePage/ReplacePage(...)`
- `Status ConvertPdfToDocx(const PdfToDocxRequest&, PdfToDocxResult*)`

统一字段：
- `input/output` 路径
- `options`（格式/行为开关）
- `stats`（页数、耗时、warning）

## 4.2 异步接口（GUI）

- `TaskHandle Submit(TaskRequest, ProgressCallback, FinishCallback)`
- `Status Cancel(TaskHandle)`
- `Status Query(TaskHandle, TaskState*)`

说明：GUI 不直接调用具体模块，调用 `TaskRunner` 即可。

## 4.3 命令注册（CLI/GUI 共用）

- `CommandRegistry` 管理 `operation_id -> handler`
- CLI 子命令和 GUI 动作都映射到同一 handler
- 避免 CLI/GUI 各自重复实现业务逻辑

---

## 5. 功能模块分期

## M0：框架启动（基础设施）

产出：
1. 新工程 `cpp_pdftools` 的 CMake + presets
2. `status/types/runtime/task` 基础 API
3. `CommandRegistry + TaskRunner` 骨架

验收：
- 基础编译通过
- 最小单测通过

## M1：迁移 `cpp_pdf2docx`（第一优先）

产出：
1. 将 `cpp_pdf2docx` 的核心代码迁移到 `src/convert/pdf2docx/`
2. 保留现有 CLI 能力：`--dump-ir`、`--no-images`、`--docx-anchored`
3. 兼容旧行为并通过回归测试

验收：
- 新框架下 `pdf2docx` 功能与旧工程一致
- `image-text.pdf` 现有问题修复成果保持（images 提取正常）

## M2：文档编辑能力（合并/加删改页）

产出：
1. `MergePdf`
2. `DeletePage`
3. `ReplacePage`
4. `InsertPage`

验收：
- 端到端测试覆盖页操作
- 输出 PDF 可被标准工具打开

## M3：提取能力（文本/附件）

产出：
1. `ExtractText`（纯文本 + 可选位置信息）
2. `ExtractAttachments`（嵌入文件）

验收：
- 文本提取结果可对齐页数
- 附件提取可导出并校验 hash

## M4：创建能力（图片转 PDF）

产出：
1. `ImagesToPdf`
2. 支持基础排版参数（页面尺寸、边距、缩放策略）

验收：
- 多图输入可正确生成多页 PDF

## M5：统一 CLI

产出：
- 单可执行 `pdftools`，子命令例如：
  - `pdftools merge ...`
  - `pdftools text extract ...`
  - `pdftools image2pdf ...`
  - `pdftools page replace ...`
  - `pdftools convert pdf2docx ...`

验收：
- CLI 帮助与参数规则一致
- 子命令覆盖全部核心能力

## M6：GUI 接入层

产出：
1. `TaskRunner` 的稳定异步 API
2. 进度/日志/取消机制
3. GUI 可直接调用的薄封装（可 C++ API 或 C ABI）

验收：
- GUI 调用不阻塞主线程
- 可取消长任务

---

## 6. 迁移策略（`cpp_pdf2docx` -> `cpp_pdftools`）

1. **先复制后收敛**：先保留一份可工作的 `convert/pdf2docx` 模块。
2. **命名空间重构**：逐步从 `pdf2docx::` 过渡到 `pdftools::convert::pdf2docx::`。
3. **接口适配层**：保留兼容 wrapper，减少一次性重写风险。
4. **测试平移**：先迁移既有 16 个测试，再逐步归类到新目录。

---

## 7. 风险与缓解

1. `thirdparty/podofo` 已有本地修改（ICCBased fallback）
- 缓解：在 `cpp_pdftools` 明确记录 patch 基线与同步策略。

2. 迁移期重复代码导致维护成本上升
- 缓解：设定“迁移完成后删除旧实现”的里程碑和窗口期。

3. CLI/GUI 需求分叉
- 缓解：强制经 `CommandRegistry/TaskRunner` 统一编排。

---

## 8. 完成定义（DoD）

1. `cpp_pdftools` 下完成核心功能模块并可单独构建测试。
2. `pdftools` 单 CLI 覆盖目标功能集合。
3. `pdf2docx` 已合并进新框架，行为不回退。
4. 有可交接文档和可复现命令，支持后续功能持续加入。
