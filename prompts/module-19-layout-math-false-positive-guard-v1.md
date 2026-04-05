# Module 19: PDF->DOCX 数学误判抑制与排版稳定化（忽略公式）V1

- 日期：2026-04-05
- 目标：继续优化 `image-text.pdf` 的文本排版，重点解决“普通中文说明被误写成 `m:oMathPara` 导致排版错乱”。

## 1. 问题复现

在 Module 18 之后，`image-text.pdf` 仍存在大量误判：

1. 指令解释、评分说明、颜色映射等普通文本被输出为 `m:oMathPara`。
2. 误判后段落会被拆成逐 token 数学 run，阅读体验明显变差。
3. 标题已修复，但正文中“数学误判”仍是主要排版噪声来源。

---

## 2. 本轮改动

核心文件：

1. `cpp_pdf2docx/src/docx/p0_writer.cpp`
2. `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`（同步）

### 2.1 数学行判定新增“自然语言优先”门槛

新增并接入以下 helper：

1. `LooksLikeCjkNaturalLanguageLine(...)`
- 对 CJK 主导文本默认判定为普通自然语言。
- 在“先忽略公式”的阶段，CJK 行不再因为 `+ - * / x y 0.1` 这类符号触发数学段。

2. `ContainsNonAsciiByte(...)`
- 若行内存在非 ASCII 且没有显式数学符号（如 `∫/∑/≤/≥/∈`）或希腊字母特征，则直接不走数学段。

3. `LooksLikeKeyValueMappingLine(...)` + `HasLongAsciiHexRun(...)`
- 识别 `"8FF5":"000000"` 这类 key-value/颜色映射文本，避免被误识别为公式。

### 2.2 列表/定义行续接再修正

在 `should_start_new_text_paragraph(...)` 增加规则：

1. 若上一行是定义标签行且以句末终止符结束，则强制新段。
- 解决了 `抬笔指令：...` 与后续普通行误拼接的问题。

---

## 3. 测试更新

文件：`cpp_pdf2docx/tests/unit/docx_reflow_test.cpp`

新增与强化断言：

1. CJK + 坐标/数值/乘号文本：
- `坐标点（x，y）映射到0.1mm * 0.1mm网格。`
- 断言其为普通 `w:t` 文本段，不出现 `m:oMathPara`。

2. 定义行与后续行分段：
- `抬笔指令：说明B。` 与下一行不应并段。

---

## 4. 回归结果

### 4.1 自动化测试

1. `cpp_pdf2docx`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`18/18 passed`

2. `cpp_pdftools`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`8/8 passed`

### 4.2 样例转换（`image-text.pdf`）

1. `cpp_pdf2docx`：
- `cpp_pdf2docx/build/final_image_text_layout_v9.docx`
- `cpp_pdf2docx/build/final_image_text_layout_v9_ir.json`
- `cpp_pdf2docx/build/final_image_text.docx`（已覆盖刷新）
- `cpp_pdf2docx/build/final_dump_ir.json`（已覆盖刷新）

2. `cpp_pdftools`：
- `cpp_pdftools/build/final_image_text_layout_v9.docx`
- `cpp_pdftools/build/final_image_text_layout_v9_ir.json`

3. 量化变化（`m:oMathPara` 数量）：
- 旧版（v5）约 `16`
- 中间版本（v8）约 `4`
- 本轮（v9）`1`

4. 可见效果：
- 标题双行保持正确；
- 大段中文说明与指令说明已回到普通文本段落；
- 误判噪声显著下降，版面可读性明显提升。

---

## 5. 当前边界

1. 仍保留少量纯 ASCII 公式行（示例：`EvaluationScore=1002`）作为数学段。
2. 本模块目标是“排版稳定化（忽略公式精度）”，并未追求公式结构恢复的召回率。
3. 若后续重启“公式专项优化”，建议在此版本上引入独立开关（strict layout vs math-aware）避免回归。
