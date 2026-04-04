# `formula.pdf` 公式转换专项测试报告（V2）

- 测试时间：`2026-04-04`
- 输入文件：`cpp_pdftools/build/formula.pdf`
- 对比基线：`prompts/formula-pdf-test-report-v1.md`

## 1) 测试命令

```bash
/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools \
  convert pdf2docx \
  --input /home/zhj/Projects/pdftools/cpp_pdftools/build/formula.pdf \
  --output /tmp/pdftools_formula_v2.docx \
  --dump-ir /tmp/pdftools_formula_v2_ir.json
```

输出：
- `pdf2docx pages=1 images=0 warnings=0`

## 2) 结果摘要

## 2.1 OMML 计数（`word/document.xml`）

- `m:oMathPara`: `8`（V1 为 `1`）
- `m:sSup`: `3`
- `m:sSub`: `1`
- `m:sSubSup`: `1`
- `m:f`: `0`

## 2.2 识别到的主要数学段

- `∫01xdx=^2^1_3`
- `n→∞lim^(1+_n^1)^n=e`
- `A=4_75_86_9`
- `a+b=c222`
- `∀x∈R,x^2≥0`
- `A∩B=(x|x∈Aandx∈B)`
- `f(x)=x+2x+1^2`
- `=(x+1)^2`

## 2.3 正文保留情况

- 标题/定理正文/证明正文维持为普通 `w:t` 文本。
- 未再出现此前“`using` 被 trig 关键字误判”的问题（已加单测覆盖）。

## 3) 结论

1. 公式识别覆盖率相对 V1 明显提升（`1 -> 8`）。
2. 对逻辑表达式、集合表达式、多行方程的识别有实质进展。
3. 复杂二维公式仍偏线性重建，语义精度仍需 V3 专项增强（积分/矩阵/独立上标回挂）。
