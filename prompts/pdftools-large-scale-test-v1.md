# `cpp_pdftools` 联网大规模测试记录（V1）

- 测试时间：`2026-04-03`
- 测试目录：`/home/zhj/Projects/pdftools/cpp_pdftools/build/large_scale`
- 使用二进制：`/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools`

---

## 1. 测试集来源（联网下载）

PDF 样本来源（公开仓库）：
1. `mozilla/pdf.js` 测试集目录 API  
   - `https://api.github.com/repos/mozilla/pdf.js/contents/test/pdfs`
2. 实际下载地址为该目录下 `download_url` 的 `.pdf` 文件（选取前 60，成功下载 56）

图片样本来源（公开仓库）：
1. `python-pillow/Pillow` 测试图片目录 API  
   - `https://api.github.com/repos/python-pillow/Pillow/contents/Tests/images`
2. 实际下载地址为该目录下 `download_url` 的 `.png/.jpg/.jpeg`（选取前 30，成功下载 25）

---

## 2. 批量测试项与结果

## 2.1 文本提取压力测试（M3）

命令模式：
- `pdftools text extract --input <pdf> --output <txt>`

统计：
- 输入 PDF：`56`
- 成功：`52`
- 失败：`4`

失败样本：
1. `Pages-tree-refs.pdf`
2. `PDFBOX-4352-0.pdf`
3. `SimFang-variant.pdf`
4. `XiaoBiaoSong.pdf`

结果文件：
- `cpp_pdftools/build/large_scale/results/text_summary.txt`
- `cpp_pdftools/build/large_scale/results/text_pass.list`
- `cpp_pdftools/build/large_scale/results/text_fail.list`

## 2.2 PDF -> DOCX 批量转换（M1）

命令模式：
- `pdftools convert pdf2docx --input <pdf> --output <docx> --dump-ir <json>`

统计（从文本提取成功集挑选前 20）：
- 输入：`20`
- 成功：`20`
- 失败：`0`

产物：
- DOCX：`20`
- IR JSON：`20`

结果文件：
- `cpp_pdftools/build/large_scale/results/docx_summary.txt`
- `cpp_pdftools/build/large_scale/results/docx_pass.list`

## 2.3 页面编辑批量测试（M2）

### 2.3.1 Merge
- 从通过样本中取 `30` 个 PDF，按两两配对进行 `15` 次合并
- 结果：`15/15` 成功

### 2.3.2 Delete / Insert / Replace
- 基于上述 `15` 个 merged PDF 执行：
  - `page delete --page 1`
  - `page insert --at 1 --source <pdf> --source-page 1`
  - `page replace --page 1 --source <pdf> --source-page 1`
- 结果：
  - `delete: 15/15`
  - `insert: 15/15`
  - `replace: 15/15`

结果文件：
- `cpp_pdftools/build/large_scale/results/page_merge_summary.txt`
- `cpp_pdftools/build/large_scale/results/page_ops_summary.txt`

## 2.4 图片转 PDF 批量测试（M4）

命令模式：
- `pdftools image2pdf --output <pdf> --images <img...>`

批次：
- 25 张图片分 5 批（每批 5 张）

结果：
- 成功：`4`
- 失败：`1`

失败批次定位后，单图复测结论：
- 真正失败文件：`broken_data_stream.png`（损坏样本）
- 其余同批图片单图转换成功

结果文件：
- `cpp_pdftools/build/large_scale/results/image2pdf_summary.txt`
- `cpp_pdftools/build/large_scale/results/image2pdf_fail.list`
- `cpp_pdftools/build/large_scale/results/image2pdf_single_fail.list`

## 2.5 附件提取批量测试（M3）

命令模式：
- `pdftools attachments extract --input <pdf> --out-dir <dir>`

统计（52 个文本提取通过 PDF）：
- 成功调用：`52/52`
- 提取到实际附件文件：`1`
- 附件样本来源目录：`attachments/attachment/foo.txt`

结果文件：
- `cpp_pdftools/build/large_scale/results/attachments_summary.txt`

---

## 3. 总体统计

1. 下载 PDF：`56`
2. 下载图片：`25`
3. 文本提取：`52/56`（`92.86%`）
4. PDF->DOCX：`20/20`（`100%`）
5. 页面编辑：
   - merge `15/15`
   - delete `15/15`
   - insert `15/15`
   - replace `15/15`
6. 图片转 PDF：`4/5` 批（失败由损坏图片触发）
7. 附件提取调用成功：`52/52`，其中提取到附件文件 `1`

---

## 4. 结论

1. `cpp_pdftools` 在公开复杂 PDF 集上总体稳定，核心能力可批量运行。
2. 当前主要失败点集中于少数复杂/异常 PDF（文本提取 4 个失败样本）和损坏图片输入（`broken_data_stream.png`）。
3. 后续可针对失败样本做专项兼容修复，并把本文件中的失败列表纳入长期回归集合。

