# `cpp_pdftools` 执行清单（V1）

- 目标：按步骤落地统一 `pdftools` 框架并并入 `pdf2docx`。

---

## G-01 初始化骨架

命令（示例）：
```bash
mkdir -p /home/zhj/Projects/pdftools/cpp_pdftools
# 创建 CMakeLists.txt / CMakePresets.json / cmake/Dependencies.cmake
```

验收：
- `cmake --preset linux-debug` 可执行。

---

## G-02 建核心接口层

改动：
- `include/pdftools/status.hpp`
- `include/pdftools/types.hpp`
- `include/pdftools/task.hpp`
- `include/pdftools/runtime.hpp`

验收：
- 基础单测通过。

---

## M1-01 迁移 pdf2docx 核心

改动：
- 平移 `backend/pipeline/docx/api` 代码到 `src/convert/pdf2docx/`
- 建立 `pdftools::convert::pdf2docx` 入口

验收：
- `pdftools convert pdf2docx` 可跑样本。

---

## M1-02 迁移测试

改动：
- 平移 `cpp_pdf2docx/tests` 到 `cpp_pdftools/tests`
- 修复 fixture 路径

验收：
- 迁移后测试全绿。

---

## M2-01 PDF 合并

接口：
- `MergePdfRequest/Result`

验收：
- 多输入 PDF 合并产物可打开。

## M2-02 页面增删改

接口：
- `InsertPage/DeletePage/ReplacePage`

验收：
- 页数与页内容符合预期。

---

## M3-01 文本提取

接口：
- `ExtractTextRequest/Result`

验收：
- 文本与页码映射正确，支持 UTF-8 输出。

## M3-02 附件提取

接口：
- `ExtractAttachmentsRequest/Result`

验收：
- 提取出的附件 hash 可验证。

---

## M4-01 图片转 PDF

接口：
- `ImagesToPdfRequest/Result`

验收：
- 多图生成多页 PDF，页面尺寸可配置。

---

## M5-01 单 CLI 汇总

目标：
- `pdftools` 单入口 + 子命令

验收：
- `pdftools --help` 展示全命令树。

---

## M6-01 GUI 接入层

目标：
- `TaskRunner` 支持异步、进度、取消

验收：
- GUI 调用任务不阻塞主线程。

---

## FINAL-01 回归

命令：
```bash
cmake --preset linux-debug
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```

验收：
- 全绿。

## FINAL-02 文档

必更：
- `prompts/pdftools-roadmap-v1.md`
- `prompts/pdftools-migration-plan-v1.md`
- `prompts/pdftools-tasks-checklist-v1.md`

验收：
- 可供后续 LLM 直接接手。
