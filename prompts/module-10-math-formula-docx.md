# 模块 10：PDF 数学公式 -> DOCX OMML 公式

## 1) 模块目标

在 `pdf2docx` 写出阶段，将 PDF 中的数学表达式优先写成 Word 公式（OMML），而不是普通文本 `w:t`。

核心目标：
1. 支持上标/下标（如 `x^2`, `y_1`）。
2. 支持分式（如 `a/b`）。
3. 避免把普通正文误判成公式（尤其中文段落）。

---

## 2) 本次实现

## 2.1 Writer 侧新增公式链路

文件：
- `cpp_pdf2docx/src/docx/p0_writer.cpp`
- 同步迁移到 `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`

新增能力：
1. 行内 span 排序与脚本关系识别（normal/sup/sub）。
2. 数学线性串构建（从几何关系生成 `^` / `_` / 运算符线性表示）。
3. 线性串 token 化 + 表达式解析（含 `+ - * / =`、`^`、`_`、括号）。
4. OMML 节点生成：
   - `m:oMathPara`
   - `m:sSup` / `m:sSub` / `m:sSubSup`
   - `m:f`（分式）
5. 文档根节点新增数学命名空间：
   - `xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math"`

## 2.2 误判控制（重点）

为减少“普通文本被写成公式”：
1. 公式判定不再只靠几何脚本关系，加入字符语义约束。
2. 增加“写入前校验”：
   - 仅当解析结果包含有效结构化数学（分式/上下标）；
   - 或有清晰运算符 + 足够标识符 token；
   才写 `m:oMath`，否则回退普通段落。
3. 行聚类加入“脚本附着”条件，避免跨行误并导致假公式。

---

## 3) 新增测试

新增：
- `cpp_pdf2docx/tests/unit/docx_math_test.cpp`

覆盖点：
1. `x^2 + y_1 = z` 生成 `m:sSup`、`m:sSub`。
2. `a/b` 生成 `m:f`。
3. 普通文本行保留 `w:t`，不强制公式化。

并在 `cpp_pdf2docx/CMakeLists.txt` 新增测试目标：
- `pdf2docx_docx_math_test`

---

## 4) 回归与验证结果

## 4.1 `cpp_pdf2docx`
- Debug：`17/17 passed`
- Release：`17/17 passed`（通过 `ctest --test-dir build/linux-release`）

## 4.2 `cpp_pdftools`
- Debug：`7/7 passed`

## 4.3 `cpp_pdftools_gui`
- Debug：`12/12 passed`

## 4.4 实样回归（`image-text.pdf`）

命令：
- `pdftools convert pdf2docx --input image-text.pdf --output ... --dump-ir ...`

抽检结果：
1. 公式段落数量从误判前的大量（约 20+）收敛到 `3`。
2. 保留的公式为变量/表达式行（如 `c = N`、`EvaluationScore = 100`、`s2+s3+s4+s5`）。
3. 先前明显误判（单中文字符、引号、`/此` 等）已消除。

---

## 5) 当前边界

1. 该版本为“启发式公式识别”，并非完整数学版面重建器。
2. 对复杂二维结构（矩阵、分段函数、多层嵌套根号）仍需后续增强。
3. 若 PDF 文字抽取本身断裂严重，公式还原质量受上游 IR 限制。
