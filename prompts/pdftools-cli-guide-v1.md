# `pdftools` CLI 使用手册（中文，V1）

- 更新时间：`2026-04-03`
- 可执行文件：`/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools`
- 适用代码：`cpp_pdftools/src/app/cli_app.cpp`

---

## 1. 快速查看帮助

命令：

```bash
pdftools --help
```

示例输出（节选）：

```text
Usage:
  pdftools merge <output.pdf> <input1.pdf> <input2.pdf> [inputN.pdf...]
  pdftools text extract --input <in.pdf> --output <out.txt|out.json> [--json] [--strict]
  pdftools attachments extract --input <in.pdf> --out-dir <dir> [--strict]
  pdftools image2pdf --output <out.pdf> --images <img1> <img2> [imgN]
  ...
```

---

## 2. 退出码约定

1. `0`：执行成功
2. `1`：参数错误/命令错误（例如缺少必填参数、未知命令）
3. `2`：运行时失败（例如解析失败、I/O 错误、非法输入文件）

说明：
1. 当内部错误码为 `kInvalidArgument` 时，CLI 返回 `1`
2. 其他运行错误通常返回 `2`

---

## 3. 全局参数

1. `--help` / `-h`
- 含义：打印帮助并退出
- 是否必填：否

---

## 4. 子命令详解

## 4.1 `merge`

语法：

```bash
pdftools merge <output.pdf> <input1.pdf> <input2.pdf> [inputN.pdf...]
```

参数说明：
1. `<output.pdf>`
- 含义：合并后的输出 PDF 路径
- 必填：是

2. `<input1.pdf> <input2.pdf> ...`
- 含义：待合并输入 PDF，按给定顺序拼接
- 必填：至少 2 个

示例：

```bash
pdftools merge merged.pdf a.pdf b.pdf
```

示例输出：

```text
merged pages=10
```

---

## 4.2 `text extract`

语法：

```bash
pdftools text extract --input <in.pdf> --output <out.txt|out.json> [--json] [--strict]
```

参数说明：
1. `--input <in.pdf>`
- 含义：输入 PDF 路径
- 必填：是

2. `--output <out.txt|out.json>`
- 含义：输出文本文件路径
- 必填：是

3. `--json`
- 含义：输出 JSON 格式（否则为纯文本）
- 必填：否

4. `--strict`
- 含义：严格模式，仅使用 PoDoFo 提取；失败即报错
- 默认：不开启（默认 best-effort，允许回退到 `pdftotext`）

5. `--include-positions`
- 含义：坐标信息开关（当前版本预留参数）
- 备注：当前实现里不改变输出结构（JSON 本身已带坐标字段）

成功输出字段说明：
1. `pages`：输出页数
2. `entries`：提取条目数
3. `extractor`：实际使用的提取器（`podofo` 或 `pdftotext`）
4. `fallback`：是否发生回退（`yes/no`）

示例 1（普通模式）：

```bash
pdftools text extract --input text.pdf --output text.txt
```

示例输出：

```text
text extracted pages=3 entries=86 extractor=podofo fallback=no
```

示例 2（JSON + 严格）：

```bash
pdftools text extract --input text.pdf --output text.json --json --strict
```

示例输出：

```text
text extracted pages=3 entries=86 extractor=podofo fallback=no
```

示例 3（严格模式失败）：

```bash
pdftools text extract --input not_pdf.bin --output out.txt --strict
```

示例输出（stderr）：

```text
Error: eager_and_skip_xref_recovery: PdfErrorCode::InvalidPDF, This is not a PDF file.
...
```

---

## 4.3 `attachments extract`

语法：

```bash
pdftools attachments extract --input <in.pdf> --out-dir <dir> [--strict]
```

参数说明：
1. `--input <in.pdf>`
- 含义：输入 PDF 路径
- 必填：是

2. `--out-dir <dir>`
- 含义：附件导出目录
- 必填：是

3. `--strict`
- 含义：严格模式；解析失败直接报错
- 默认：不开启（best-effort，解析失败返回空结果并标记）

成功输出字段说明：
1. `count`：提取出的附件数量
2. `parser`：使用的解析器（当前 `podofo`，解析失败时为 `none`）
3. `parse_failed`：是否发生解析失败但被容错处理（`yes/no`）

示例 1（正常提取）：

```bash
pdftools attachments extract --input attachment.pdf --out-dir ./attachments
```

示例输出：

```text
attachments extracted count=1 parser=podofo parse_failed=no
```

示例 2（best-effort 容错）：

```bash
pdftools attachments extract --input not_pdf.bin --out-dir ./attachments_invalid
```

示例输出：

```text
attachments extracted count=0 parser=none parse_failed=yes
```

示例 3（strict 失败）：

```bash
pdftools attachments extract --input not_pdf.bin --out-dir ./attachments_invalid --strict
```

示例输出（stderr）：

```text
Error: eager_and_skip_xref_recovery: PdfErrorCode::InvalidPDF, This is not a PDF file.
...
```

---

## 4.4 `image2pdf`

语法：

```bash
pdftools image2pdf --output <out.pdf> --images <img1> <img2> [imgN]
```

参数说明：
1. `--output <out.pdf>`
- 含义：输出 PDF 路径
- 必填：是

2. `--images <img1> <img2> ...`
- 含义：输入图片列表
- 必填：是（至少 1 张）

3. `--use-image-size`
- 含义：每页使用原图尺寸作为页面尺寸
- 必填：否

4. `--original-size`
- 含义：不做 fit 缩放（按原始尺寸绘制）
- 必填：否（默认 `fit`）

返回字段说明：
1. `pages`：成功写入的页面数
2. `skipped_images`：解码失败被跳过的图片数

示例 1（全成功）：

```bash
pdftools image2pdf --output images_ok.pdf --use-image-size --images 7x13.png a_fli.png
```

示例输出：

```text
image2pdf pages=2 skipped_images=0
```

示例 2（混合坏图，任务仍成功）：

```bash
pdftools image2pdf --output images_mixed.pdf --use-image-size --images 7x13.png broken_data_stream.png a_fli.png
```

示例输出：

```text
image2pdf pages=2 skipped_images=1
```

示例 3（全部坏图，任务失败）：

```bash
pdftools image2pdf --output images_bad_only.pdf --images broken_data_stream.png
```

示例输出（stderr）：

```text
Error: no valid images were decoded
```

注意：
1. 建议把 `--images` 放在参数最后，后续都作为图片路径传入。

---

## 4.5 `page delete`

语法：

```bash
pdftools page delete --input <in.pdf> --output <out.pdf> --page <n>
```

参数说明：
1. `--input`：输入 PDF
2. `--output`：输出 PDF
3. `--page`：要删除的页（1-based）

示例：

```bash
pdftools page delete --input merged.pdf --output deleted.pdf --page 1
```

示例输出：

```text
page delete pages=9
```

---

## 4.6 `page insert`

语法：

```bash
pdftools page insert --input <in.pdf> --output <out.pdf> --at <n> --source <src.pdf> --source-page <m>
```

参数说明：
1. `--input <in.pdf>`：目标 PDF
2. `--output <out.pdf>`：输出 PDF
3. `--at <n>`：插入位置（1-based）
4. `--source <src.pdf>`：来源 PDF
5. `--source-page <m>`：来源页（1-based）

示例：

```bash
pdftools page insert --input text.pdf --output page_insert_ok.pdf --at 1 --source image.pdf --source-page 1
```

示例输出：

```text
page insert pages=4
```

---

## 4.7 `page replace`

语法：

```bash
pdftools page replace --input <in.pdf> --output <out.pdf> --page <n> --source <src.pdf> --source-page <m>
```

参数说明：
1. `--input <in.pdf>`：目标 PDF
2. `--output <out.pdf>`：输出 PDF
3. `--page <n>`：被替换页（1-based）
4. `--source <src.pdf>`：来源 PDF
5. `--source-page <m>`：来源页（1-based）

示例：

```bash
pdftools page replace --input text.pdf --output page_replace_ok.pdf --page 1 --source image.pdf --source-page 1
```

示例输出：

```text
page replace pages=3
```

---

## 4.8 `convert pdf2docx`

语法：

```bash
pdftools convert pdf2docx --input <in.pdf> --output <out.docx> [--dump-ir <ir.json>] [--no-images] [--docx-anchored] [--disable-font-fallback]
```

参数说明：
1. `--input <in.pdf>`
- 含义：输入 PDF
- 必填：是

2. `--output <out.docx>`
- 含义：输出 DOCX
- 必填：是

3. `--dump-ir <ir.json>`
- 含义：额外导出 IR JSON（调试/分析用）
- 必填：否

4. `--no-images`
- 含义：不抽取图片，仅文本链路
- 必填：否

5. `--docx-anchored`
- 含义：使用 anchored 图片写入模式
- 必填：否

6. `--disable-font-fallback`
- 含义：关闭字体回退探测
- 必填：否

成功输出字段说明：
1. `pages`：输出页数
2. `images`：写入图片数量
3. `warnings`：告警数

示例 1（带 IR 导出）：

```bash
pdftools convert pdf2docx --input image-text.pdf --output converted.docx --dump-ir converted_ir.json
```

示例输出：

```text
pdf2docx pages=7 images=7 warnings=7
```

示例 2（禁图 + anchored）：

```bash
pdftools convert pdf2docx --input image-text.pdf --output converted_noimg_anchored.docx --no-images --docx-anchored
```

示例输出：

```text
pdf2docx pages=7 images=0 warnings=0
```

---

## 5. 常见错误与排查

1. `Unknown command`
- 原因：子命令拼写错误或层级错误
- 处理：先执行 `pdftools --help`

2. `... requires --input and --output`
- 原因：缺少必填参数
- 处理：补齐必填参数

3. `Error: output path already exists`
- 原因：当前 CLI 未暴露 `--overwrite`，输出文件已存在
- 处理：先删除目标文件后重试

4. `Error: ... InvalidPDF ...`
- 原因：输入非合法 PDF 或损坏严重
- 处理：
1. `text extract` 可不加 `--strict` 让其尝试回退
2. 其他命令建议先用外部工具检查输入有效性

