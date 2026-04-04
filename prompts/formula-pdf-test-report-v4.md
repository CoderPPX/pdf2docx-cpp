# `formula.pdf` 公式转换专项测试报告（V4）

- 时间：`2026-04-04`
- 输入：`cpp_pdftools/build/formula.pdf`

## 1) 测试命令

```bash
/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools \
  convert pdf2docx \
  --input /home/zhj/Projects/pdftools/cpp_pdftools/build/formula.pdf \
  --output /tmp/pdftools_formula_v4.docx \
  --dump-ir /tmp/pdftools_formula_v4_ir.json
```

输出：
- `pdf2docx pages=1 images=0 warnings=0`

## 2) OMML 计数

- `m:oMathPara = 7`
- `m:sSup = 4`
- `m:sSub = 1`
- `m:sSubSup = 1`
- `m:f = 1`

对比：
- V3：`m:f = 0`
- V4：`m:f = 1`（新增分式结构命中）

## 3) 关键观察

1. 积分行右侧分式从“等号挂上下标”改善为分式节点：
- 文本串显示：`∫01x2dx=13`
- 结构上已出现 `m:f`（`1/3`）

2. V3 的改进保持稳定：
- 勾股公式：`a2+b2=c2`（对应 superscript 结构）
- 多行等式：`f(x)=x^2+2x+1=(x+1)^2`（已合并）

## 4) 剩余问题

1. 积分上下限仍未专用结构化（未见 `m:nary`）。
2. 矩阵仍线性字符化（未见 `m:m`）。

## 5) 结论

V4 相比 V3 的主要新增价值是“分式结构恢复”（`m:f` 从 0 提升到 1），说明归一化规则有效。后续应进入符号级结构化阶段（积分/矩阵专用节点）。
