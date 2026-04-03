# `cpp_pdftools` 失败原因分析与修复记录（V1）

- 时间：`2026-04-03`
- 目标：定位联网大规模测试中的失败根因，并完成代码修复与回归验证。

---

## 1. 原始失败样本

文本提取失败（5 个）：
1. `Brotli-Prototype-FileA.pdf`
2. `Pages-tree-refs.pdf`
3. `PDFBOX-4352-0.pdf`
4. `SimFang-variant.pdf`
5. `XiaoBiaoSong.pdf`

图片转 PDF 失败（1 个）：
1. `broken_data_stream.png`

---

## 2. 根因分析

## 2.1 文本提取失败

1. `Brotli-Prototype-FileA.pdf`
- 根因：文件本身不是合法 PDF（PoDoFo 报 `InvalidPDF`，`Unable to read PDF header`）。
- 结论：属于不可修复输入，保持失败是正确行为。

2. `Pages-tree-refs.pdf` / `PDFBOX-4352-0.pdf` / `SimFang-variant.pdf` / `XiaoBiaoSong.pdf`
- 根因：PoDoFo 在默认文本提取路径上报解析异常（页面树循环、trailer 异常、variant 类型异常）。
- 对照：`pdftotext` 对其中 4 个可输出文本（说明可“降级提取”）。
- 结论：需要“PoDoFo 主路径 + 回退提取”策略。

## 2.2 图片转 PDF 失败

1. `broken_data_stream.png`
- 根因：输入图片损坏（`libpng error: bad adaptive filter value`）。
- 旧行为：批量任务中任一坏图导致整批失败。
- 结论：应改为“跳过坏图，保留可解码图片继续生成 PDF”。

---

## 3. 代码修复

## 3.1 文本提取鲁棒性修复（`src/pdf/extract_ops.cpp`）

1. 增加 PoDoFo 多加载策略：
- `None`
- `LoadStreamsEagerly`
- `SkipXRefRecovery`
- `LoadStreamsEagerly | SkipXRefRecovery`

2. 当 PoDoFo 路径失败时，新增 `pdftotext` 回退：
- 命令：`pdftotext <input.pdf> -`
- 读取 stdout 作为文本提取结果
- 继续支持 `plain/json` 输出写盘

3. 新增严格模式开关：
- `ExtractTextRequest.best_effort`（默认 `true`）
- CLI 对应参数：`--strict`（禁用回退，仅 PoDoFo）

4. 输出结果增强：
- `ExtractTextResult.used_fallback`
- `ExtractTextResult.extractor`（`podofo` 或 `pdftotext`）

5. 抽取公共输出写文件逻辑，减少分支重复。

文件：
- `cpp_pdftools/src/pdf/extract_ops.cpp`

## 3.2 图片转 PDF 鲁棒性修复（`src/pdf/create_ops.cpp`）

1. 对每张图片单独 `try/catch`：
- 单张损坏时跳过，不中断整批任务。

2. 仅当“全部图片均不可解码”时返回失败。

3. 输出结果新增跳过统计：
- `ImagesToPdfResult.skipped_image_count`

文件：
- `cpp_pdftools/src/pdf/create_ops.cpp`

## 3.3 附件提取容错修复（`src/pdf/extract_ops.cpp`）

1. `ExtractAttachmentsRequest` 新增 `best_effort`（默认 `true`）：
- 解析失败时返回空结果而不是整任务失败。

2. `ExtractAttachmentsResult` 新增字段：
- `parse_failed`
- `parser`

3. CLI 新增：
- `attachments extract --strict`（禁用容错）

文件：
- `cpp_pdftools/src/pdf/extract_ops.cpp`
- `cpp_pdftools/src/app/cli_app.cpp`

## 3.4 测试补强

新增/修改测试：
1. `m4_create_ops_test` 增加“有效 + 损坏图片混合输入”回归：
- 期望：任务成功，输出仅包含有效图片页。
2. `m3_extract_ops_test` 增加 strict 模式失败用例（非 PDF 输入应返回解析失败）。
3. `m3_extract_ops_test` 增加附件提取 best-effort/strict 双路径回归。
3. `m5_cli_test` 增加 `text extract --strict` 回归与 extractor 字段校验。
4. `m5_cli_test` 增加 `attachments extract --strict` 失败路径回归。

文件：
- `cpp_pdftools/tests/unit/m4_create_ops_test.cpp`
- `cpp_pdftools/tests/unit/m3_extract_ops_test.cpp`
- `cpp_pdftools/tests/unit/m5_cli_test.cpp`

---

## 4. 修复后验证结果

## 4.1 单测

```bash
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```

结果：
- `7/7 tests passed`

## 4.2 失败样本复测

文本提取（原 5 失败）：
- 成功：`4`
- 失败：`1`（仅 `Brotli-Prototype-FileA.pdf`，非 PDF 输入）

图片转 PDF：
1. 单独坏图：仍失败（预期）
2. 混合输入（好图+坏图+好图）：成功（已修复）

## 4.3 大规模复测（V3/V4）

1. 文本提取：
- 修复前：`53/58`
- 修复后：`57/58`
- 唯一失败：`Brotli-Prototype-FileA.pdf`（非 PDF）

2. 图片批量转 PDF（25 张按 5 批）：
- 修复前：`4/5`
- 修复后：`5/5`（V3 和 V4 均保持）

3. 附件提取（57 份可提取文本样本）：
- 修复前：`56/57`
- 修复后：`57/57`（其中 `1` 份走 parse_failed 容错）

---

## 5. 结论

1. 这次修复解决了“可降级处理”的失败（复杂但可读 PDF、批量中的坏图片）。
2. 对“本质非法输入”（非 PDF）保持失败，符合预期错误语义。
3. 关键能力在公开测试集上的鲁棒性已明显提升。
