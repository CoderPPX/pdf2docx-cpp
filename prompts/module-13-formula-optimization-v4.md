# Module 13: 公式识别优化（V4）

- 日期：`2026-04-04`
- 目标：在 V3 基础上进一步提升积分行右侧分式表达，减少 `=_3^1` 这类错误结构。

## 本轮完成

1. 线性表达式归一化（写入前）
- 文件：`cpp_pdf2docx/src/docx/p0_writer.cpp`
- 新增：
  - `ReadLinearScriptToken`
  - `NormalizeLinearMathExpression`
- 规则：
  - 将 `= _a ^b` 或 `= ^b _a` 归一化为 `= b/a`（关系符后“上下标堆叠”解释为分式）。
- 目的：
  - 让解析器进入已有 `m:f` 分式路径，而不是把上下标挂到等号上。

2. 接入点
- 在 `AppendMathParagraph` 中，先对 `linear_expression` 归一化，再 token 化与解析。
- `should_emit_math` 判定改用归一化后表达式。

3. 与 V3 逻辑兼容
- 保留 V3 的：
  - 复合 span 拆分；
  - 多行公式续行合并；
  - 单括号 token 去脚本化。

## 影响文件

- `cpp_pdf2docx/src/docx/p0_writer.cpp`
- `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`（已同步）

## 验证

1. `cpp_pdf2docx`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`17/17 passed`

2. `cpp_pdftools`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`7/7 passed`

3. `cpp_pdftools_gui`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`12/12 passed`

4. `formula.pdf` 回归（`pdftools convert pdf2docx`）
- `m:oMathPara = 7`
- `m:f = 1`（V3 为 0）
- 关键行从 `∫...=_3^1` 改为分式语义（文本串显示为 `=13`，对应 OMML 分式节点）。

## 仍待优化（V5）

1. 积分上下限尚未变成专用 n-ary 结构（`m:nary`），仍是线性近似。
2. 矩阵仍未转 `m:m`，当前是字符线性化（`...`）。
3. 后续建议直接引入“符号块检测 + 结构节点构建”：
- `∫/∑/lim` 专用构造器；
- 矩阵网格检测与 `m:m` 输出。
