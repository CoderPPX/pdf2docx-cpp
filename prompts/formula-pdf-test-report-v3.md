# `formula.pdf` 公式转换专项测试报告（V3）

- 时间：`2026-04-04`
- 输入：`cpp_pdftools/build/formula.pdf`
- 命令：

```bash
/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools \
  convert pdf2docx \
  --input /home/zhj/Projects/pdftools/cpp_pdftools/build/formula.pdf \
  --output /tmp/pdftools_formula_v3.docx \
  --dump-ir /tmp/pdftools_formula_v3_ir.json
```

输出：
- `pdf2docx pages=1 images=0 warnings=0`

## 1) OMML 计数

- `m:oMathPara = 7`
- `m:sSup = 4`
- `m:sSub = 1`
- `m:sSubSup = 1`
- `m:f = 0`

对比：
- V2: `m:oMathPara = 8`
- V3: `m:oMathPara = 7`
- 说明：数量减少是因为多行等式被合并为同一公式段，不是识别退化。

## 2) 关键观察

1. 勾股公式由 V2 的 `a+b=c222` 改善为 `a2+b2=c2`（上标回挂更合理）。
2. 多行公式由两段合并为一段：
- `f(x)=x^2+2x+1=(x+1)^2`
3. 极限行结构比 V2 更自然：
- `n→∞lim(1+_n^1)^n=e`

## 3) 仍存在的问题

1. 积分表达式仍未结构化为专用 OMML（`m:nary`），上下限语义仍有偏差：
- 当前：`∫01x2dx=_3^1`
2. 矩阵仍为线性字符近似：
- 当前：`A=4_75_86_9`
3. 因此 V3 主要提升“行级/token 级重建”，还未完成“二维公式语义建模”。

## 4) 下一步（V4）

1. 实现积分符号驱动的上下限绑定（输出 `m:nary`）。
2. 实现矩阵候选检测（方括号/三行数字阵列）并输出 `m:m`。
3. 对 `formula.pdf` 加入回归断言：
- 勾股公式需包含 3 个独立 superscript；
- 多行等式保持单段输出；
- 积分/矩阵先以“不会降级”为保底。
