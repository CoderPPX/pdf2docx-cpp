# Module 11: `formula.pdf` 公式识别与转换优化（V2）

- 日期：`2026-04-04`
- 目标：以 `cpp_pdftools/build/formula.pdf` 为主回归样本，提升 PDF->DOCX 数学公式识别覆盖率，并降低误判。

## 本轮完成内容

1. 强化公式行聚类（`AppendPageTextParagraphs`）
- 引入行包络与横向距离判断：`LineEnvelope` + `HorizontalGapToEnvelope`。
- 引入脚本/数学 token 的分层附着规则，减少公式 token 被拆成多段。
- 基线更新改为按当前行重算（`ComputeLineBaseline`），避免脚本 token 拉偏整行基线。
- 修正 `bbox.height=0` 的小符号（如括号）判定，避免把其当普通正文导致公式断裂。

2. 强化数学 token 识别
- 增加 `LooksMathToken`，并修正函数名误判（`sin` 不再误匹配 `using` 这类普通词）。
- 保持正文文本保守策略，避免“英文句子被误判为公式”。

3. 强化公式解析器稳健性
- 在 `MathExpressionParser::Parse()` 增加“尾部 token 兜底收集”，避免遇到不支持运算符后只输出前缀（如此前 `A ∩ B = ...` 只剩 `A`）。
- 仍保留结构化解析（上/下标、分式等），并在无法完整解析时尽量保留 token。

4. 调整写入门槛
- `AppendMathParagraph` 维持保守门槛，减少误写为 OMML 的普通行。

5. 新增/增强测试
- 扩展 `cpp_pdf2docx/tests/unit/docx_math_test.cpp`：
  - 新增集合表达式 `A ∩ B = {x | x ∈ A}` 用例；
  - 新增正文 `using samples` 用例，防止 trig 关键词误判；
  - 断言 `m:oMathPara` 数量与关键符号输出。

## 关键改动文件

- `cpp_pdf2docx/src/docx/p0_writer.cpp`
- `cpp_pdf2docx/tests/unit/docx_math_test.cpp`
- `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`（同步迁移）

## 测试结果

1. `cpp_pdf2docx`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`17/17 passed`

2. `cpp_pdftools`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`7/7 passed`

3. `cpp_pdftools_gui`
- `ctest --preset linux-debug --output-on-failure`
- 结果：`12/12 passed`

4. `formula.pdf` 实样回归
- 旧结果（V1）：`m:oMathPara = 1`
- 新结果（V2）：`m:oMathPara = 8`
- 说明：复杂公式覆盖率明显提升，逻辑/集合/多行方程基本进入 OMML；积分/矩阵语义仍有结构化精度空间。

## 已知剩余问题

1. 积分、矩阵目前仍偏“线性 token 重建”，未形成高质量的 OMML 专用结构（如 `m:nary`、`m:m`）。
2. `a^2+b^2=c^2` 这类“底座文本 span + 独立上标 span”仍存在配对不完美（会出现 `c222` 一类线性化结果）。
3. 多行公式之间尚未做“语义续行”合并（例如 `f(x)=...` 与下一行 `=(x+1)^2`）。

## 下一步建议（V3）

1. 增加“同一公式块”的跨行聚合（基于垂直间距 + x 覆盖 + 公式符号密度）。
2. 增加积分/求和/矩阵专用解析分支，直接输出 `m:nary`、`m:m`。
3. 针对“独立上标 token”做 x 区间回挂，把 `2` 回挂到最近底座变量而非行尾。
