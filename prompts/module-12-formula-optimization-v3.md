# Module 12: 公式识别优化（V3）

- 日期：`2026-04-04`
- 目标：继续以 `cpp_pdftools/build/formula.pdf` 为主样本，重点修复“上标回挂错误”和“多行公式续行断裂”。

## 本轮完成

1. 复合数学 span 拆分（核心）
- 文件：`cpp_pdf2docx/src/docx/p0_writer.cpp`
- 新增逻辑：
  - `ShouldExplodeCompositeMathSpan`
  - `ExplodeCompositeMathSpan`
- 作用：把 `a + b = c` 这类单个大 span 按 token 拆分成近似几何位置的子 span，再与独立上标 token 一起排序。
- 结果：`a+b=c222` 改善为 `a2+b2=c2`（并触发更合理的 `m:sSup` 结构）。

2. 脚本判定细化
- 对单字符括号 token（`(` `)` 等）禁止脚本角色，减少 `lim` 行中括号被误判为上标的情况。

3. 多行公式续行合并
- 在 `AppendPageTextParagraphs` 增加 pending math 缓冲与续行判定：
  - 若下一行以 `=`/`+`/`-` 等运算符开头，或上一行以运算符结尾，则合并到同一公式段。
- 结果：`f(x)=x+...` 与下一行 `=(x+1)^2` 由两段 OMML 合并为一段。

4. 单测增强
- 文件：`cpp_pdf2docx/tests/unit/docx_math_test.cpp`
- 新增场景：
  - 续行公式：`f(x)=x+1` + 下一行 `=2x+1`（验证续行合并）；
  - 大 span + 独立上标：`a + b = c` + 三个 `2` 上标（验证回挂能力）。
- 断言更新：
  - `m:oMathPara` 数量预期从 `3` 调整为 `5`；
  - `m:sSup` 至少 `3` 个。

## 影响文件

- `cpp_pdf2docx/src/docx/p0_writer.cpp`
- `cpp_pdf2docx/tests/unit/docx_math_test.cpp`
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

4. `formula.pdf` 实测（`pdftools convert pdf2docx`）
- OMML 数量：`m:oMathPara=7`（V2 为 8，因“多行公式合并”减少 1）
- 质量提升点：
  - `a+b=c222` -> `a2+b2=c2`
  - `f(x)=x+2x+1` 与 `=(x+1)^2` 合并为一条公式

## 仍待优化（V4 建议）

1. 积分与矩阵仍为“线性 token 重建”，尚未输出专用结构：
- 积分：`m:nary`
- 矩阵：`m:m`
2. 当前 `∫01x2dx=_3^1` 的上下限语义还不准确，需要“符号级几何回挂”。
3. 可新增 `formula.pdf` 对应的集成断言（脚本数量/关键公式子串），防止后续回退。
