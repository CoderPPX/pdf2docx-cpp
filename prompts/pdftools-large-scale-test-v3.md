# `cpp_pdftools` 联网大规模测试记录（V3，失败修复后）

- 时间：`2026-04-03`
- 测试二进制：`/home/zhj/Projects/pdftools/cpp_pdftools/build/linux-debug/pdftools`
- 数据目录：`/home/zhj/Projects/pdftools/cpp_pdftools/build/large_scale`
- 结果目录：`/home/zhj/Projects/pdftools/cpp_pdftools/build/large_scale/results_v4`

---

## 1. 样本规模

1. PDF：`58`
2. 图片：`25`

---

## 2. 最新批量结果

## 2.1 文本提取（默认 best-effort）

- 结果：`57/58` 成功
- 唯一失败：
1. `Brotli-Prototype-FileA.pdf`（非合法 PDF）

## 2.2 PDF->DOCX

- 输入：30 份通过文本提取的样本
- 结果：`30/30` 成功

## 2.3 页面编辑

1. merge：`20/20`
2. delete：`20/20`
3. insert：`20/20`
4. replace：`20/20`

## 2.4 图片转 PDF

- 批次（5 批）：`5/5` 成功
- 说明：坏图输入已实现“跳过继续”，不再拖垮整批。

## 2.5 附件提取（默认 best-effort）

- 结果：`57/57` 调用成功
- 其中 `parse_failed=yes`：`1`（对应可降级样本）
- 实际提取附件文件：`2`（包含 `foo.txt`）

---

## 3. 与修复前对比

1. 文本提取：`53/58 -> 57/58`
2. 图片批量：`4/5 -> 5/5`
3. 附件提取：`56/57 -> 57/57`

---

## 4. 备注

1. 当前失败/降级样本主要属于格式损坏或非标准输入。
2. CLI 新增 strict 模式：
   - `text extract --strict`
   - `attachments extract --strict`
   可强制按严格解析失败返回错误。

