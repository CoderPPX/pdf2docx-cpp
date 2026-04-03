# 模块 09：调试工具与可观测性设计（pdf2docx / pdf2ir / ir2html）

## 1) 模块定位

本模块定义工具链的职责边界，确保人和 LLM 可以快速定位问题。

当前工具：
- `pdf2docx`：主转换
- `pdf2ir`：结构化调试导出
- `ir2html`：可视化调试导出

---

## 2) 每个工具的职责边界

## 2.1 `pdf2docx`
- 输入：PDF
- 输出：DOCX
- 用于生产路径回归

## 2.2 `pdf2ir`
- 输入：PDF
- 输出：JSON（IR 摘要）
- 用于排查“后端提取是否正确”

## 2.3 `ir2html`
- 输入：PDF
- 输出：HTML（定位预览）
- 用于排查“坐标映射是否合理”

---

## 3) 典型问题排查路径

1. 先跑 `pdf2ir`：
   - 如果图片计数为 0 -> 后端抽图问题
2. 再跑 `ir2html`：
   - 如果有图但位置错 -> 坐标映射问题
3. 最后看 `pdf2docx`：
   - 如果 media 有图但 Word 显示异常 -> OOXML mapping 问题

---

## 4) 建议增强项（可观测性）

## P1
1. `pdf2docx` 增加 `--dump-ir <path>`
2. `pdf2ir` 增加汇总字段：
   - `total_pages`
   - `total_spans`
   - `total_images`
3. `ir2html` 增加 `--only-page N` 便于大文档定位

## P2
1. 增加 `--log-json` 输出结构化日志
2. 增加 `--profile` 输出阶段耗时

---

## 5) 验收建议

1. 三工具对同一 PDF 的页数一致
2. `pdf2ir.total_images == DOCX media 图片数`
3. `ir2html` 视觉结果与 IR 数值一致（抽样验证）
