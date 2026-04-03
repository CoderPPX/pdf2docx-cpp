# 模块 09：调试工具与可观测性（pdf2docx / pdf2ir / ir2html）

## 1) 模块定位

本模块提供调试闭环：
1. `pdf2ir`：看提取结果（结构化 JSON）。
2. `ir2html`：看坐标映射与视觉对齐。
3. `pdf2docx`：看最终文档写出与统计输出。

---

## 2) 工具职责边界

## 2.1 `pdf2docx`
- 生产链路转换工具。
- 可选同步导出 IR（`--dump-ir`）。

## 2.2 `pdf2ir`
- 提取调试工具。
- 输出可脚本化消费的 JSON 摘要。

## 2.3 `ir2html`
- 布局调试工具。
- 支持单页定位（`--only-page`）。

---

## 3) 当前 CLI 能力

## 3.1 `pdf2docx`
```bash
pdf2docx <input.pdf> <output.docx> \
  [--dump-ir <out.json>] \
  [--no-images] \
  [--disable-font-fallback] \
  [--docx-anchored]
```

## 3.2 `pdf2ir`
```bash
pdf2ir <input.pdf> <output.json>
```

JSON 包含：
- `summary.total_pages`
- `summary.total_spans`
- `summary.total_images`
- 每页 `span_count/image_count`

## 3.3 `ir2html`
```bash
ir2html <input.pdf> <output.html> [--scale 1.25] [--hide-boxes] [--only-page N]
```

---

## 4) 关键实现

1. 新增通用写出模块：
   - `include/pdf2docx/ir_json.hpp`
   - `src/core/ir_json.cpp`
2. `pdf2ir` 统一复用 `WriteIrToJson(...)`，避免多份 JSON 格式逻辑分叉。
3. `ir2html` 新增 `only_page` 参数并在越界时返回 `kInvalidArgument`。

---

## 5) 排障路径（推荐）

1. 先跑 `pdf2ir`：
   - 若 `total_images == 0`，优先查后端提取。
2. 再跑 `ir2html`：
   - 若有图但位置异常，查坐标映射/几何信息。
3. 最后跑 `pdf2docx`：
   - 若 media 有图但显示异常，查 OOXML drawing 映射。

---

## 6) 测试覆盖

1. `tests/unit/ir_json_test.cpp`
2. `tests/unit/ir_html_only_page_test.cpp`
3. `tests/unit/ir_html_image_test.cpp`
4. `tests/unit/converter_test.cpp`
5. `tests/integration/end_to_end_test.cpp`

---

## 7) 后续任务

## P1
1. 为 `pdf2docx --dump-ir` 增加“单次提取复用”优化（避免重复抽取）。
2. 增加结构化日志开关（JSON lines）。

## P2
1. 增加 `--profile` 输出阶段耗时。
2. 增加大文档分页抽样调试脚本。

---

## 8) 最新验证（2026-04-03）

- `ctest --preset linux-debug`：`16/16` 通过。
- `test-image-text.pdf`：
  - `pdf2ir` 产出 summary 正常；
  - `ir2html --only-page 1` 正常渲染单页；
  - `pdf2docx --dump-ir` 产物与统计一致。
