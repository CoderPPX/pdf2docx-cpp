# `formula.pdf` 专项测试报告（V5 最终快照）

- 时间：`2026-04-04`
- 输入：`cpp_pdftools/build/formula.pdf`
- 说明：本报告对应“暂停公式继续开发”前的最新稳定状态。

## 1) 测试命令

```bash
/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools \
  convert pdf2docx \
  --input /home/zhj/Projects/pdftools/cpp_pdftools/build/formula.pdf \
  --output /tmp/pdftools_formula_v5.docx \
  --dump-ir /tmp/pdftools_formula_v5_ir.json
```

输出：
- `pdf2docx pages=1 images=0 warnings=0`

## 2) OMML 统计

- `m:oMathPara = 7`
- `m:f = 1`
- `m:sSup = 9`
- `m:sSub = 4`
- `m:sSubSup = 1`
- `m:nary = 0`

## 3) 关键质量点

1. 积分首行已形成结构化组合：
- `∫` 的上下限（`sSubSup`）
- `x^2`（`sSup`）
- 等号右侧 `1/3`（`m:f`）

2. 勾股公式保持 superscript 结构。
3. 多行方程 `f(x)=...=(x+1)^2` 保持合并输出。

## 4) 当前缺口

1. 未产出 `m:nary`（积分仍是近似结构）。
2. 未产出 `m:m`（矩阵仍线性字符化）。
3. `lim` 等复杂结构仍需专门语义解析器。

## 5) 结论

V5 相比早期版本，已从“基本线性识别”提升到“部分结构化公式输出（含分式、上下标、续行合并）”。
后续应进入专用结构节点阶段（`m:nary` / `m:m`）。
