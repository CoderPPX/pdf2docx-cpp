# `pdftools_gui` 预览能力 Roadmap（V1）

- 更新时间：`2026-04-04`
- 目标：在现有 Qt6 GUI 基础上补齐“可视化预览”能力，覆盖你指定场景：
  - 显示当前 PDF
  - 图片转 PDF 预览
  - PDF 提取图片预览
  - PDF 指定页码预览（截取页码前可视化）
  - PDF 转 DOCX 的 IR 渲染预览

---

## M-PREVIEW-01：预览基础服务层（`PreviewService`）

目标：
1. 抽离统一预览服务，避免在页面内重复写解析/渲染逻辑。
2. 复用已有 `pdftools::pdf::GetPdfInfo` 和 `pdf2docx::Converter::ExtractIrFromFile`。

计划实现：
1. `QueryPdfPageCount(input_pdf)`：返回页数与错误信息。
2. `RenderPdfIrPreview(input_pdf, only_page, include_images)`：
   - 解析 IR；
   - 生成可嵌入 GUI 的 HTML 预览字符串（IR 渲染视图）；
   - 返回统计（pages/spans/images）。
3. `ExtractImageThumbnailsFromPdf(input_pdf, only_page)`：
   - 从 IR 图片块构建缩略图，用于“PDF 提取图片预览”。
4. `BuildImageThumbnails(paths)`：
   - 读取本地图片列表生成缩略图，用于“图片转 PDF 预览”。

验收：
1. 新增 `preview_service_test` 通过。
2. 对损坏/不存在文件返回清晰错误，不崩溃。

---

## M-PREVIEW-02：TextExtract 页面“当前 PDF + 指定页预览”

目标：
1. 输入 PDF 后显示页数信息。
2. 可指定页码做预览（对应“截取特定页码”前的可视化确认）。

计划实现：
1. 增加预览区（页码选择 + 刷新按钮 + 预览浏览器）。
2. 输入路径变化后更新页码范围（`page_start/page_end/preview_page`）。
3. 预览由 `PreviewService::RenderPdfIrPreview(..., only_page=n, include_images=false)` 驱动。

验收：
1. 页码范围随 PDF 页数变化。
2. 预览区可显示指定页内容。

---

## M-PREVIEW-03：PageEdit 页面“操作页预览”

目标：
1. 删除/插入/替换前可预览目标页（或来源页）。

计划实现：
1. 增加“刷新页预览”与预览浏览器。
2. 按当前 Tab 自动映射预览源：
   - 删除：预览目标 PDF 的删除页；
   - 插入：预览来源 PDF 的来源页；
   - 替换：预览来源 PDF 的来源页。

验收：
1. 切换 Tab 与页号后，预览能正确跟随。

---

## M-PREVIEW-04：ImageToPdf 页面“图片列表缩略图预览”

目标：
1. 在转换前可直观看到输入图片顺序与可读性。

计划实现：
1. 增加缩略图列表（Icon Mode）。
2. 实时显示：
   - 已加载缩略图数量；
   - 不可读取图片数量。
3. 文件列表变更时自动刷新缩略图。

验收：
1. 调整顺序后缩略图顺序一致。
2. 坏图会计入不可读取计数。

---

## M-PREVIEW-05：Pdf2Docx 页面“IR 渲染 + 提取图片预览”

目标：
1. 提供 PDF->DOCX 前的 IR 可视化预览。
2. 可直接预览 PDF 中提取到的图片缩略图。

计划实现：
1. 新增 IR 预览区（页码选择 + 刷新按钮 + HTML 渲染）。
2. 新增图片预览区（从 IR 图片块渲染缩略图）。
3. 输入 PDF 变更时更新可选页码范围。

验收：
1. IR 预览能显示页内容与基本布局。
2. 图片缩略图区显示提取到的图片及数量。

---

## M-PREVIEW-06：测试与回归

计划实现：
1. 新增测试：
   - `preview_service_test`
   - `text_extract_page_test`
   - `pdf2docx_page_test`
   - `image_to_pdf_page_test`（至少覆盖缩略图刷新与计数）
2. 保持既有测试全绿，避免回归。

验收标准：
1. `cpp_pdftools`：`7/7` 通过（或新增后全通过）。
2. `cpp_pdftools_gui`：新增预览测试后全通过（Debug/Release）。

---

## M-PREVIEW-07：Merge 页面“当前 PDF 预览”

目标：
1. 在合并任务前，可直接查看当前输入列表中的某个 PDF 的指定页内容。
2. 作为“显示当前 PDF”能力的通用入口。

计划实现：
1. 在 `MergePage` 增加预览区：
   - 预览文件下拉框；
   - 预览页码输入；
   - 刷新按钮；
   - IR 预览浏览器。
2. 输入文件列表变化时自动刷新可选预览源。
3. 预览渲染复用 `PreviewService::RenderPdfIrPreview(..., only_page=n)`。

验收：
1. 输入列表包含有效 PDF 时，可预览指定页。
2. 输入列表为空或文件损坏时，不崩溃并给出可读提示。
3. `merge_page_test` 新增预览用例并通过。

---

## 实施顺序（本次执行）

1. 先做 `M-PREVIEW-01`（基础服务）；
2. 并行接入 `M-PREVIEW-04`（图片缩略图）和 `M-PREVIEW-05`（IR 渲染/提图预览）；
3. 再做 `M-PREVIEW-02` 与 `M-PREVIEW-03`（页级预览）；
4. 最后补 `M-PREVIEW-06` 测试并全量回归；
5. 收尾补 `M-PREVIEW-07`，覆盖“当前 PDF 可视化预览”。
