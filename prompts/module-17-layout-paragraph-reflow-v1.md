# Module 17: PDF->DOCX 排版优化（忽略数学公式）V1

- 日期：2026-04-05
- 目标：在不改动公式识别主逻辑前提下，先提升普通文本段落排版可读性（减少“每行一个段落”）。

## 1. 问题复现

使用：`cpp_pdf2docx/build/image-text.pdf`

改造前现象：

1. 正文经常“每个视觉行单独成段”，段落过碎。
2. 同一自然段跨多行时未重组，阅读连贯性差。
3. 标题/数学/普通文本之间缺少统一 flush 顺序，扩展规则时容易相互干扰。

---

## 2. 本轮实现

### 2.1 行到段落重组（非数学）

文件：`cpp_pdf2docx/src/docx/p0_writer.cpp`

新增核心能力：

1. 行级几何统计
- `ComputeLineLeft`
- `ComputeLineRight`
- `ComputeLineMedianHeight`

2. 文本段拼接与空格规则
- `ShouldInsertSpaceBetweenLines`
- `AppendContinuationLine`
- 英文跨行自动补空格；连字符断词（`-`）可无缝拼接。

3. 新段落判定（启发式）
- 基于：行距、缩进变化、居中状态变化、编号/列表起始等。
- 关键策略：
  - 大行距 -> 开新段；
  - 缩进明显变化 -> 开新段；
  - heading/list 行 -> 开新段；
  - 列表 marker 起始 -> 开新段。

4. Flush 流程重构
- 增加 `pending_text_paragraph` 缓冲。
- 顺序统一为：
  - 数学行前先 flush 普通文本；
  - 标题行前先 flush 普通文本；
  - 页末统一 flush：line -> math -> title -> text。

### 2.2 兼容同步

同样改动同步到：

- `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`

保证 `pdftools convert pdf2docx` 路径与 `cpp_pdf2docx` 输出一致。

---

## 3. 新增测试

1. 新增单元测试：
- `cpp_pdf2docx/tests/unit/docx_reflow_test.cpp`

2. 测试覆盖点：
- 两行视觉换行重组为一个段落。
- 缩进/行距触发新段。
- 编号 heading 与正文分段不串段。

3. CMake 接入：
- `cpp_pdf2docx/CMakeLists.txt`
- 新测试名：`docx_reflow_test`

---

## 4. 本地验证结果

### 4.1 `cpp_pdf2docx`

1. `cmake --build --preset linux-debug -j8`：通过
2. `ctest --preset linux-debug --output-on-failure`：`18/18` 通过

### 4.2 `cpp_pdftools`

1. `cmake --build --preset linux-debug -j8`：通过
2. `ctest --preset linux-debug --output-on-failure`：`8/8` 通过

### 4.3 样例回归（`image-text.pdf`）

1. 生成命令：
- `cpp_pdf2docx/build/linux-debug/pdf2docx build/image-text.pdf /tmp/image_text_after.docx --dump-ir /tmp/image_text_after_ir.json`
- `cpp_pdftools/build/linux-debug/pdftools convert pdf2docx --input ../cpp_pdf2docx/build/image-text.pdf --output /tmp/pdftools_image_text_after.docx --dump-ir /tmp/pdftools_image_text_after_ir.json`

2. 结果对比（解析 `word/document.xml`）：
- 改造前文本段落数：`117`
- 改造后文本段落数：`80`

3. 关键改善：
- 多个正文行成功合并为自然段（例如“背景”“任务定义”段落）。
- 编号标题与正文保持分段。

---

## 5. 当前已知限制（后续可继续）

1. 某些“定义列表/指令列表”仍可能被合并过度（例如多条 `xxx指令：` 在同段）。
2. 标题第二行目前通过 `w:br` 保留，纯文本抽取时会显示为拼接（Word可见换行）。
3. 目前仍是启发式重组，尚未引入更强的版面块级语义（段落块/列表块/图注块）。

