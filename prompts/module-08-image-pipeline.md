# 模块 08：图片链路专项（PDF 图片 -> IR -> HTML/DOCX）

## 1) 模块定位

该专项跨后端、IR、可视化和 DOCX 写出，目标是让图片在链路中可提取、可观测、可写出。

涉及文件：
- `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
- `cpp_pdf2docx/include/pdf2docx/ir.hpp`
- `cpp_pdf2docx/src/core/ir_html.cpp`
- `cpp_pdf2docx/src/docx/p0_writer.cpp`
- `cpp_pdf2docx/include/pdf2docx/types.hpp`

---

## 2) 当前数据流

1. PoDoFo 内容流扫描命中图片 XObject。
2. 提取图片编码字节与几何信息，写入 `ir::ImageBlock`。
3. `ir2html`：base64 `<img>` 预览 + quad overlay（debug）。
4. `pdf2docx`：写入 `word/media/*` 并在 `document.xml` 引用。

---

## 3) 当前格式与统计支持

## 3.1 图片格式
- 已支持：
  - `DCTDecode`（JPEG）
  - `JPXDecode`（JP2）
  - `FlateDecode`（decode 后转 PNG）
- 未完全覆盖：
  - `JBIG2Decode` 等复杂格式

## 3.2 统计字段
- `ImageExtractionStats`：
  - `extracted_count`
  - `skipped_count`
  - `skipped_unsupported_filter_count`
  - `skipped_empty_stream_count`
  - `skipped_decode_failed_count`
  - `warning_count`
- 映射到 `ConvertStats`：
  - `warning_count`
  - `backend_warning_count`
  - 各类 `skipped_*` 计数

---

## 4) 几何表示（当前）

`ImageBlock` 同时保存：
1. AABB：`x/y/width/height`
2. 四角点：`has_quad + quad(p0,p1,p2,p3)`

用途：
- AABB：兼容既有渲染/写出路径。
- quad：用于旋转/错切调试与后续 anchored 精度提升。

---

## 5) 可观测性

1. `ir2html` 在 debug 模式下可绘制图片 quad polygon。
2. `pdf2docx` 输出统计可见 `images/skipped_images/warnings`。
3. `pdf2ir` JSON 可见每页 `image_count` 与图片 `data_size`。

---

## 6) 测试覆盖

1. `tests/unit/ir_html_image_test.cpp`
   - 图片渲染与 quad overlay 标记。
2. `tests/unit/test_pdf_fixture_test.cpp`
   - fixture 至少存在一张 `has_quad` 图片。
3. `tests/unit/converter_test.cpp`
   - 图片计数与 skip reason 统计一致性。
4. `tests/integration/end_to_end_test.cpp`
   - 图文 PDF 端到端写出验证。

---

## 7) 已知限制

1. `ICCBased` 颜色空间仍可能产生 warning。
2. DOCX anchored 当前仍为近似布局，并非严格图形变换映射。
3. 复杂滤镜/颜色空间尚未全部覆盖。

---

## 8) 后续任务

## P1
1. 扩展更多图像滤镜与颜色空间支持。
2. 把 quad 更深入用于 DOCX 锚定定位计算。

## P2
1. 增加图片链路专用样本集与 golden 统计。
2. 提供更细阶段耗时（抽图/编码/写出）。

---

## 9) 最新验证（2026-04-03）

- `ctest --preset linux-debug`：`16/16` 通过。
- `test-image-text.pdf` 实跑：`images=2 skipped_images=5 warnings=5`。
