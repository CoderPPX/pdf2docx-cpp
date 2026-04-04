# `formula.pdf` 公式转换专项测试报告（V1）

- 测试时间：`2026-04-04`
- 输入文件：`cpp_pdftools/build/formula.pdf`
- 目标：验证“PDF 数学公式 -> DOCX OMML 公式”在真实公式样本上的效果。

---

## 1) 测试命令

## 1.1 `pdftools` 链路

```bash
/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools \
  convert pdf2docx \
  --input /home/zhj/Projects/pdftools/cpp_pdftools/build/formula.pdf \
  --output /tmp/pdftools_formula_test.docx \
  --dump-ir /tmp/pdftools_formula_test_ir.json
```

输出：
- `pdf2docx pages=1 images=0 warnings=0`

## 1.2 `cpp_pdf2docx` 原生链路交叉验证

```bash
/home/zhj/Projects/pdftools/cpp_pdf2docx/build/linux-debug/pdf2docx \
  /home/zhj/Projects/pdftools/cpp_pdftools/build/formula.pdf \
  /tmp/cpp_pdf2docx_formula_test.docx \
  --dump-ir /tmp/cpp_pdf2docx_formula_test_ir.json
```

输出：
- `P1 convert done ... pages=1 images=0 skipped_images=0 warnings=0`

---

## 2) 结果检查

## 2.1 IR 摘要

- `total_pages=1`
- `total_spans=60`
- `total_images=0`

IR 中包含多类公式相关 token（如 `∫`、`lim`、`n→∞`、`a + b = c`、`f(x)=...`、`≥` 等），但存在明显“碎片化”：
- 积分上下限与主体分离在不同 span/行；
- 上标数字多次独立成单字符 span（`2`）；
- 矩阵括号出现为特殊字符（如 `  `）而非结构化对象。

## 2.2 DOCX 中 OMML 计数

对 `word/document.xml` 检查：
- `m:oMathPara`: `1`
- `m:f`: `0`
- `m:sSup`: `0`
- `m:sSub`: `0`
- `m:sSubSup`: `0`

唯一识别出的公式为：
- `a + b = c`

其余内容（积分、极限、矩阵、多行方程）当前仍以普通 `w:t` 文本形式写出。

---

## 3) 结论

1. 当前实现已具备“基础线性公式”识别能力（例如 `a+b=c`）。
2. 对 `formula.pdf` 这类“复杂二维公式版面”仍不足，主要瓶颈在：
   - 上游 IR 对公式结构抽取碎片化严重；
   - 现有启发式更偏向线性表达式，未覆盖积分/极限/矩阵等结构重建。
3. 该样本可作为后续公式模块迭代的主回归样本。

---

## 4) 下一步建议（按优先级）

1. **行重建增强**：先做“公式区域行聚合”，把积分上下限、主体、等号右侧聚到统一公式候选块。
2. **结构识别增强**：
   - `∫` + 上/下限 -> OMML n-ary/integral 结构；
   - `lim` + 下标 -> `m:limLow`；
   - 括号包围二维数组 -> `m:m`（矩阵）。
3. **规则分层**：
   - 一级：当前轻量线性规则（已完成）；
   - 二级：复杂符号触发的专用公式解析器（积分/极限/求和/矩阵）。
