# `cpp_pdftools` 联网大规模测试记录（V2）

- 测试时间：`2026-04-03`
- 工程路径：`/home/zhj/Projects/pdftools/cpp_pdftools`
- 结果目录：`/home/zhj/Projects/pdftools/cpp_pdftools/build/large_scale/results_v2`

---

## 1. 测试集来源

PDF 集（公开）：
1. `https://api.github.com/repos/mozilla/pdf.js/contents/test/pdfs`
2. 从目录 `download_url` 下载样本，当前本地可用 `58` 份 PDF

图片集（公开）：
1. `https://api.github.com/repos/python-pillow/Pillow/contents/Tests/images`
2. 从目录 `download_url` 下载样本，当前本地可用 `25` 张图片

---

## 2. 批量测试结果（V2）

## 2.1 文本提取（M3）

- 命令：`pdftools text extract --input <pdf> --output <txt>`
- 结果：`53/58` 成功（`91.38%`）
- 失败样本：
1. `Brotli-Prototype-FileA.pdf`
2. `Pages-tree-refs.pdf`
3. `PDFBOX-4352-0.pdf`
4. `SimFang-variant.pdf`
5. `XiaoBiaoSong.pdf`

文件：
- `text_summary.txt`
- `text_pass.list`
- `text_fail.list`

## 2.2 PDF->DOCX（M1）

- 命令：`pdftools convert pdf2docx --input <pdf> --output <docx> --dump-ir <json>`
- 输入：通过文本提取的前 `30` 份 PDF
- 结果：`30/30` 成功（`100%`）

文件：
- `docx_summary.txt`
- `docx_pass.list`
- `docx_fail.list`

## 2.3 页面操作（M2）

### Merge
- 取通过集前 `40` 份 PDF，两两配对，共 `20` 次
- 结果：`20/20` 成功

### Delete / Insert / Replace
- 对每个 merged PDF 依次执行：
  - `page delete --page 1`
  - `page insert --at 1 --source ... --source-page 1`
  - `page replace --page 1 --source ... --source-page 1`
- 结果：
  - `delete: 20/20`
  - `insert: 20/20`
  - `replace: 20/20`

文件：
- `page_merge_summary.txt`
- `page_ops_summary.txt`

## 2.4 图片转 PDF（M4）

### 批次测试（每批 5 张）
- `batch_total=5`
- `batch_pass=4`
- `batch_fail=1`

### 单图测试（25 张逐一）
- `single_total=25`
- `single_pass=24`
- `single_fail=1`
- 失败文件：`broken_data_stream.png`（损坏输入）

文件：
- `image2pdf_batch_summary.txt`
- `image2pdf_single_summary.txt`
- `image2pdf_single_fail.list`

## 2.5 附件提取（M3）

- 命令：`pdftools attachments extract --input <pdf> --out-dir <dir>`
- 输入：文本提取通过集 `53` 份
- 调用结果：`53/53` 成功
- 实际提取到附件：`1` 个
- 附件路径：`attachments/attachment/foo.txt`

文件：
- `attachments_summary.txt`

---

## 3. 总结结论

1. 在联网公开测试集上，`cpp_pdftools` 主要能力稳定可批量运行。
2. 页面编辑能力在 V2 压测中保持 `100%` 成功。
3. `pdf2docx` 在 30 份复杂样本上 `100%` 成功。
4. 当前可归因失败主要来自异常/损坏输入（复杂异常 PDF 与损坏 PNG）。

