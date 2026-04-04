# Module 14: 数学公式模块阶段性交接（V5）

- 日期：`2026-04-04`
- 状态：按用户要求，先暂停数学公式继续开发；本文件用于后续 LLM 直接接手。

## 1) 本阶段已完成内容（代码层）

1. 公式行聚类与脚本附着增强
- 新增行包络判定与脚本附着窗口，减少数学 token 被拆散。
- 修复 `bbox.height=0` 小符号（括号等）导致的误判。

2. 复合 span 拆分（关键）
- 对 `a + b = c` 这类单 span 公式做 token 级拆分并重排。
- 结果：独立上标回挂改善，`a+b=c222` 改善为 `a^2+b^2=c^2` 的结构化输出。

3. 多行公式续行合并
- 支持把下一行以 `=` 等运算符开头的公式续行合并到上一条公式。
- 结果：`f(x)=...` 与下一行 `=(x+1)^2` 合并为单条公式段。

4. 线性表达式归一化
- 规则：`= _a ^b` / `= ^b _a` 归一化为 `= b/a`，用于恢复分式结构。
- 规则：`∫` 后两字符归一化为上下限（`∫_0^1...`）。

5. 解析器增强
- `MathExpressionParser::ParseTerm` 支持隐式乘法/相邻项继续解析，避免在无显式 `*` 时过早截断。

## 2) 主要变更文件

- `cpp_pdf2docx/src/docx/p0_writer.cpp`
- `cpp_pdf2docx/tests/unit/docx_math_test.cpp`
- `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`（已同步）

## 3) 测试与验证

1. 单元/集成测试
- `cpp_pdf2docx`：`17/17 passed`
- `cpp_pdftools`：`7/7 passed`
- `cpp_pdftools_gui`：`12/12 passed`

2. `formula.pdf`（核心样本）
- 命令：`pdftools convert pdf2docx --input cpp_pdftools/build/formula.pdf ...`
- 当前统计：
  - `m:oMathPara = 7`
  - `m:f = 1`
  - `m:sSup = 9`
  - `m:sSub = 4`
  - `m:sSubSup = 1`

3. 首条积分公式的当前 OMML（关键）
- 已形成：
  - `∫` 的 `m:sSubSup`（下标 0，上标 1）
  - `x` 的 `m:sSup`（上标 2）
  - 右侧 `1/3` 的 `m:f`

## 4) 已知问题（待后续 LLM）

1. 仍未实现专用 `m:nary`（积分/求和）节点输出；当前用 `sSubSup` 近似。
2. 矩阵仍未输出 `m:m`，目前是线性字符化（如 `...`）。
3. 部分复杂表达（如 `lim` 行）仍为半结构化，需做符号级语义建模。

## 5) 下一步建议（从这里继续）

1. 积分专用构造器
- 识别 `∫` + 上下限 + 被积函数 + `dx`，直接产出 `m:nary`。

2. 矩阵专用构造器
- 识别方括号包围 + 行列网格，产出 `m:m`。

3. 针对 `formula.pdf` 增加更强回归断言
- 断言首条积分公式必须含 `m:nary`（未来目标）。
- 断言矩阵行必须含 `m:m`（未来目标）。

