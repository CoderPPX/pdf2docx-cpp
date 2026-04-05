# Module 18: PDF->DOCX 列表/定义条目排版优化（忽略公式）V1

- 日期：2026-04-05
- 目标：在 Module 17 基础上，继续优化“定义条目/指令条目”排版，避免多个条目被并入同一段。

## 1. 背景问题

在 `image-text.pdf` 中，存在这类文本模式：

1. `笔头直径 ...` / `画笔颜⾊ ...`
2. `落笔指令 ...` / `抬笔指令 ...` / `移动指令 ...`

此前重排后仍会出现条目合并过度，阅读不清晰。

---

## 2. 本轮改动

核心文件：`cpp_pdf2docx/src/docx/p0_writer.cpp`

### 2.1 新增行类型识别

1. `IsBracketHeadingLine(...)`
- 识别 `【...】` 类标题行。

2. `IsLikelyDefinitionLabelLine(...)`
- 识别“前缀 + 冒号 + 说明内容”型定义条目行。
- 规则要点：
  - 冒号位置靠前（标签区域）；
  - 标签前不应是完整句末语气；
  - 冒号后必须有正文（避免把仅以冒号结尾的换行误判为标签）。

3. `EndsWithColon(...)`
- 支持 `:` 与 `：`。

### 2.2 段落切分规则增强

在 `should_start_new_text_paragraph(...)` 中增加：

1. 当前行为定义标签行时，默认开新段（除非上一行明确以冒号收尾，属于同标签续写）。
2. 连续两个定义标签行强制分段。
3. `【...】` 标题行纳入 heading/list 逻辑，避免与正文串段。

### 2.3 与 Module 17 协同

保留并继续使用：

1. 行到段的普通重组（行距/缩进/居中判定）。
2. 英文跨行空格补全与连字符拼接。
3. `title/math/text` flush 顺序。

---

## 3. 测试更新

文件：`cpp_pdf2docx/tests/unit/docx_reflow_test.cpp`

新增断言覆盖：

1. `落笔指令：说明A。`
2. `抬笔指令：说明B。`

两行应拆分为两个独立段落，不应被错误合并。

---

## 4. 同步与回归

### 4.1 代码同步

已同步到迁移版本：

- `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`

### 4.2 自动化测试

1. `cpp_pdf2docx`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`18/18 passed`

2. `cpp_pdftools`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`8/8 passed`

### 4.3 样例回归（`image-text.pdf`）

1. `cpp_pdf2docx` 生成：
- `cpp_pdf2docx/build/final_image_text_layout_v3.docx`
- `cpp_pdf2docx/build/final_image_text_layout_v3_ir.json`

2. `cpp_pdftools` 生成：
- `cpp_pdftools/build/final_image_text_layout_v3.docx`
- `cpp_pdftools/build/final_image_text_layout_v3_ir.json`

3. 结果观察：
- 指令条目（落笔/抬笔/移动）已拆分为独立段落。
- 定义条目（笔头直径/画笔颜色）已拆分为独立段落。
- 总体段落比 Module 17 进一步更符合人工阅读。

---

## 5. 仍待优化点

1. `image-text.pdf` 仍存在个别语义断裂来源于上游文本提取（IR 本身缺词/断词），这不完全属于 DOCX 重排层可解问题。
2. 标题行目前是同段 `w:br` 表达，文本提取工具看起来会连成一行，但 Word 中可正常换行显示。

