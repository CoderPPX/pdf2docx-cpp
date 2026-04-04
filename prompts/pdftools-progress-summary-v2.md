# pdftools 进度总结（V2）

时间：2026-04-04

## 1. 已完成范围

- `cpp_pdftools` 主库框架已搭建并可用（merge / text extract / attachments / image2pdf / page edit / pdf2docx）。
- CLI 已实现并通过单测。
- Qt6 GUI 已实现主流程页面与任务中心，并通过单测。
- `cpp_pdf2docx` 迁移到 `cpp_pdftools` legacy 通路已完成。
- PDF->DOCX 重点优化已做：
  - 公式识别（上/下标、分式、续行合并、部分积分近似结构）；
  - `image-text.pdf` 首页标题双行样式修复（单段+换行+标题字号）。

## 2. 最近测试结果

- `cpp_pdf2docx`: 17/17 passed
- `cpp_pdftools`: 7/7 passed
- `cpp_pdftools_gui`: 12/12 passed

专项样本：
- `cpp_pdftools/build/formula.pdf` -> 当前统计约为：
  - `m:oMathPara=7`
  - `m:f=1`
  - `m:sSup=9`
  - `m:sSub=4`
  - `m:sSubSup=1`

## 3. 可人工检查的关键输出

- `cpp_pdf2docx/build/final_image_text.docx`
- `cpp_pdf2docx/build/final_anchored.docx`
- `/tmp/image_text_title_fix.docx`
- `/tmp/pdftools_formula_v5.docx`
- `/tmp/formula_cpp_pdf2docx_v15.docx`

## 4. 结论：测试是否“完全正确”

结论：**不是完全正确**。

说明：
- 自动化测试目前全绿，说明“当前规则集下的回归稳定”。
- 但 PDF->DOCX 仍存在已知未完成项：
  - 积分未使用专用 `m:nary` 结构；
  - 矩阵未使用 `m:m` 结构；
  - 个别复杂公式/复杂版面仍可能出现语义近似而非完全等价。

因此当前状态应定义为：**可用并持续改进中，不是数学与版式100%保真完成态**。
