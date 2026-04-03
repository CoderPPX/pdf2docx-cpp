# `cpp_pdftools` 模块执行记录（M0~M6）

- 更新时间：`2026-04-03`
- 工程目录：`/home/zhj/Projects/pdftools/cpp_pdftools`
- 状态：`M0~M6 全部已落地并通过测试`

---

## M0 基础框架（Core + Runtime）

完成内容：
1. 建立 `cpp_pdftools` 独立工程（`CMakeLists.txt` + `CMakePresets.json`）。
2. 实现基础状态模型：`pdftools::Status`、`pdftools::ErrorCode`。
3. 实现运行时编排基础能力：
   - `CommandRegistry`
   - `TaskRunner`
   - `RuntimeContext`
   - `ICommandHandler` / `ILogger` / `IProgressSink`

新增测试：
- `tests/unit/m0_runtime_test.cpp`

验证结果：
- `ctest --preset linux-debug` -> `pdftools_m0_runtime_test: Passed`

---

## M1 `pdf2docx` 迁移并入

完成内容：
1. 将 `cpp_pdf2docx` 已有核心源码迁移到 `cpp_pdftools/src/legacy_pdf2docx/`。
2. 将旧头文件迁移到 `cpp_pdftools/include/pdf2docx/`。
3. 在新框架对外提供统一入口：
   - `include/pdftools/convert/pdf2docx.hpp`
   - `src/convert/pdf2docx/pdf2docx.cpp`
4. 支持选项：
   - `--no-images`（`extract_images=false`）
   - `--docx-anchored`
   - `--dump-ir <path>`

新增测试：
- `tests/unit/m1_pdf2docx_test.cpp`

验证结果：
- 使用 `build/image-text.pdf` fixture 执行转换与 IR 导出
- `ctest --preset linux-debug` -> `pdftools_m1_pdf2docx_test: Passed`

---

## M2 文档编辑（合并/删页/插页/替页）

完成内容：
1. 实现 API：
   - `MergePdf`
   - `DeletePage`
   - `InsertPage`
   - `ReplacePage`
2. 基于 PoDoFo 页面集合接口实现：
   - `AppendDocumentPages`
   - `RemovePageAt`
   - `InsertDocumentPageAt`
3. 增加输出路径、页码范围校验和 overwrite 策略。

新增测试：
- `tests/unit/m2_document_ops_test.cpp`

验证结果：
- 合并后页数校验、删除页后页数校验、插页/替页后页数一致性校验
- `ctest --preset linux-debug` -> `pdftools_m2_document_ops_test: Passed`

---

## M3 提取（文本/附件）

完成内容：
1. 实现 `ExtractText`：
   - 按页提取文本
   - 支持 `plain text/json` 输出
2. 实现 `ExtractAttachments`：
   - 从 EmbeddedFiles name tree 导出附件
   - 支持输出目录与重名处理
3. 增加 JSON 转义、输出文件路径检查等稳定性逻辑。

新增测试：
- `tests/unit/m3_extract_ops_test.cpp`

验证结果：
- 文本提取：对 fixture PDF 提取并落盘
- 附件提取：测试中动态生成带嵌入文件 PDF，再执行提取并校验内容
- `ctest --preset linux-debug` -> `pdftools_m3_extract_ops_test: Passed`

---

## M4 创建（图片转 PDF）

完成内容：
1. 实现 `ImagesToPdf`：
   - 多图输入生成多页 PDF
   - 支持 `use_image_size_as_page`
   - 支持 `fit/original` 缩放模式
2. 使用 `PdfImage + PdfPainter` 完成页面构建。

新增测试：
- `tests/unit/m4_create_ops_test.cpp`

验证结果：
- 测试内生成 1x1 PNG 样本，执行双图转 PDF，校验输出页数=2
- `ctest --preset linux-debug` -> `pdftools_m4_create_ops_test: Passed`

---

## M5 统一 CLI

完成内容：
1. 新增统一 CLI 入口 `pdftools`：
   - `merge`
   - `text extract`
   - `attachments extract`
   - `image2pdf`
   - `page delete/insert/replace`
   - `convert pdf2docx`
2. CLI 实现位于：
   - `include/pdftools/cli.hpp`
   - `src/app/cli_app.cpp`
   - `tools/cli/main.cpp`

新增测试：
- `tests/unit/m5_cli_test.cpp`

验证结果：
- `--help`、`text extract`、`convert pdf2docx` 命令流程校验通过
- `ctest --preset linux-debug` -> `pdftools_m5_cli_test: Passed`

---

## M6 GUI 接入层（任务 API）

完成内容：
1. 通过 `TaskRunner` 提供异步提交、取消、查询基础能力。
2. 任务状态模型：
   - `queued/running/succeeded/failed/cancelled`
3. 通过 `ProgressCallback`、`FinishCallback` 提供 UI 可订阅事件。

新增测试：
- `tests/unit/m6_task_runner_test.cpp`

验证结果：
- 慢任务取消路径验证（提交 -> 取消 -> 状态进入 cancelled）
- `ctest --preset linux-debug` -> `pdftools_m6_task_runner_test: Passed`

---

## 总体验证命令

```bash
cd /home/zhj/Projects/pdftools/cpp_pdftools
cmake --preset linux-debug
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```

结果：
- `7/7 tests passed`

