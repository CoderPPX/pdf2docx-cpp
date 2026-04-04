# Module 15: `image-text.pdf` 标题排版修复（V1）

- 日期：`2026-04-04`
- 问题：`image-text.pdf` 首页标题在 IR 中两行正确，但 DOCX 呈现不理想（未作为标题样式、两行关系不明显）。

## 本轮修复

1. 新增标题识别逻辑（保守启发式）
- 文件：`cpp_pdf2docx/src/docx/p0_writer.cpp`
- 条件（第一页顶部，且为前两行）：
  - 行基线在页面上方区域；
  - 非编号小节（排除 `1.` 等）；
  - 英文字符足够且符合 title-case 特征。

2. 新增标题段落写出
- 新增 `AppendTitleParagraph(...)`：
  - 同一段落内用 `w:br` 保留标题换行；
  - 增加标题 run 样式（加粗、字号 `w:sz=36`）。

3. 写出流程调整
- 在 `AppendPageTextParagraphs(...)` 中增加 `pending_title_lines` 缓冲：
  - 连续标题行合并写出；
  - 与数学段落/普通段落 flush 顺序协调，避免串段。

4. 兼容同步
- 同步到：`cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`

## 验证

1. 自动化测试
- `cpp_pdf2docx`: `17/17 passed`
- `cpp_pdftools`: `7/7 passed`
- `cpp_pdftools_gui`: `12/12 passed`

2. 目标样本回归
- 重新生成：
  - `cpp_pdf2docx/build/final_image_text.docx`
  - `/tmp/image_text_title_fix.docx`
- `document.xml` 首段已变为“单段双行标题”（包含 `w:br`）：
  - `Auto Painter: From RGB Image to Pen Plot`
  - `Trajectories`

## 交接备注

1. 当前标题识别是启发式，仅针对首页顶部英文标题场景。
2. 若后续要做通用版，建议引入“字体大小/字重/块级语义”到 IR，再做稳定样式映射。
