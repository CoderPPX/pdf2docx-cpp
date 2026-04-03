# 模块 06：API 与 CLI（库入口、工具入口）

## 1) 模块定位

本模块负责：
1. 暴露稳定 C++ 转换接口（`Converter`）。
2. 提供 CLI 工具（`pdf2docx` / `pdf2ir` / `ir2html`）。
3. 统一统计字段与错误语义。

核心文件：
- `cpp_pdf2docx/include/pdf2docx/converter.hpp`
- `cpp_pdf2docx/include/pdf2docx/types.hpp`
- `cpp_pdf2docx/src/api/converter.cpp`
- `cpp_pdf2docx/tools/cli/main.cpp`
- `cpp_pdf2docx/tools/pdf2ir/main.cpp`
- `cpp_pdf2docx/tools/ir2html/main.cpp`

---

## 2) 当前 API（真实）

## 2.1 `ConvertOptions`
```cpp
struct ConvertOptions {
  bool preserve_page_breaks = true;
  bool extract_images = true;
  bool detect_tables = false;
  bool enable_font_fallback = true;
  bool docx_use_anchored_images = false;
  uint32_t max_threads = 0;
};
```

## 2.2 `ConvertStats`
```cpp
struct ConvertStats {
  uint32_t page_count = 0;
  uint32_t image_count = 0;
  uint32_t warning_count = 0;
  uint32_t extracted_image_count = 0;
  uint32_t skipped_image_count = 0;
  uint32_t skipped_unsupported_filter_count = 0;
  uint32_t skipped_empty_stream_count = 0;
  uint32_t skipped_decode_failed_count = 0;
  uint32_t font_probe_count = 0;
  uint32_t backend_warning_count = 0;
  double elapsed_ms = 0.0;
  std::string backend;
  std::string xml_backend;
};
```

## 2.3 `Converter`
```cpp
Status ExtractIrFromFile(const std::string& input_pdf,
                         const ConvertOptions& options,
                         ir::Document* document,
                         ImageExtractionStats* image_stats = nullptr) const;

Status ConvertFile(const std::string& input_pdf,
                   const std::string& output_docx,
                   const ConvertOptions& options,
                   ConvertStats* stats) const;
```

---

## 3) 当前编排逻辑（`ConvertFile`）

顺序：
1. 参数校验。
2. `ExtractIrFromFile`：字体 probe + PoDoFo 抽取。
3. `pipeline.Execute(&ir_document, &pipeline_stats)`：页内 spans 排序。
4. `P0Writer.WriteFromIr(...)`：写出 DOCX。
5. 汇总 `ConvertStats` 并返回。

---

## 4) CLI 能力（当前）

## 4.1 `pdf2docx`
```bash
pdf2docx <input.pdf> <output.docx> \
  [--dump-ir <out.json>] \
  [--no-images] \
  [--disable-font-fallback] \
  [--docx-anchored]
```

输出包含：`backend/xml/elapsed/pages/images/skipped_images/warnings`。

## 4.2 `pdf2ir`
```bash
pdf2ir <input.pdf> <output.json>
```

输出 JSON：
- 顶层 `summary`：`total_pages/total_spans/total_images`
- 每页：`span_count/image_count`
- 图片：`data_size`（不输出原始字节）

## 4.3 `ir2html`
```bash
ir2html <input.pdf> <output.html> [--scale 1.25] [--hide-boxes] [--only-page N]
```

---

## 5) 边界约束

1. API 不暴露 thirdparty 具体类型。
2. CLI 仅做参数解析与编排，不实现核心算法。
3. JSON/HTML 主要面向调试，不作为稳定外部协议。

---

## 6) 测试覆盖（当前）

1. `tests/unit/converter_test.cpp`：空参数、错误路径、统计字段。
2. `tests/unit/ir_json_test.cpp`：summary 与每页计数字段。
3. `tests/unit/ir_html_only_page_test.cpp`：`--only-page` 行为与越界校验。
4. `tests/integration/end_to_end_test.cpp`：主链路端到端验证。

---

## 7) 后续任务

## P1
1. 增加内存输入接口（`ConvertMemory`）。
2. 提供结构化日志输出（JSON log/callback）。

## P2
1. 增加可取消机制（cancel token）。
2. 减少 `--dump-ir` 的重复提取开销（复用 `ConvertFile` 中间 IR）。

---

## 8) 最新验证（2026-04-03）

- `ctest --preset linux-debug`：`16/16` 通过。
- `test-image-text.pdf` 实跑：
  - `pdf2docx ... --dump-ir` 成功；
  - 输出统计：`pages=7 images=2 skipped_images=5 warnings=5`。
