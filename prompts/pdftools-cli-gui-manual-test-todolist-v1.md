# `pdftools` CLI / GUI 手工测试 TODO List（V1）

- 更新时间：`2026-04-04`
- 适用目录：`/home/zhj/Projects/pdftools`
- 目标：覆盖当前实现的**全部 CLI/GUI tools**，并可复现主要成功/失败路径。

---

## 0. 测试前准备（执行一次）

- [ ] 构建 CLI（Debug）  
  `cmake --build --preset linux-debug -j8`（目录：`cpp_pdftools`）
- [ ] 构建 GUI（Debug）  
  `cmake --build --preset linux-debug -j8`（目录：`cpp_pdftools_gui`）
- [ ] 创建手工测试输出目录  
  `mkdir -p /tmp/pdftools_manual_cli_gui`
- [ ] 确认基础测试素材存在：
  - [ ] `cpp_pdf2docx/build/text.pdf`
  - [ ] `cpp_pdf2docx/build/image.pdf`
  - [ ] `cpp_pdf2docx/build/image-text.pdf`
  - [ ] `thirdparty/tinyxml2/docs/doc.png`
  - [ ] `thirdparty/tinyxml2/docs/doxygen.png`

建议先导出变量（可选）：

```bash
ROOT=/home/zhj/Projects/pdftools
CLI=$ROOT/cpp_pdftools/build/linux-debug/pdftools
GUI=$ROOT/cpp_pdftools_gui/build/linux-debug/pdftools_gui
PDF_TEXT=$ROOT/cpp_pdf2docx/build/text.pdf
PDF_IMAGE=$ROOT/cpp_pdf2docx/build/image.pdf
PDF_MIX=$ROOT/cpp_pdf2docx/build/image-text.pdf
IMG1=$ROOT/thirdparty/tinyxml2/docs/doc.png
IMG2=$ROOT/thirdparty/tinyxml2/docs/doxygen.png
OUT=/tmp/pdftools_manual_cli_gui
```

---

## 1. CLI Tools 手工测试

## CLI-00 `help`

- [ ] 执行 `$CLI --help`
- [ ] 预期：返回码 `0`，输出包含 `merge/text/attachments/pdf/image2pdf/page/convert`

## CLI-01 `merge`

- [ ] 执行：`$CLI merge $OUT/merge_out.pdf $PDF_TEXT $PDF_IMAGE`
- [ ] 预期：输出 `merged pages=...`，返回码 `0`
- [ ] 执行：`$CLI pdf info --input $OUT/merge_out.pdf`
- [ ] 预期：`pages` > 任一输入单文件页数

失败路径：
- [ ] 执行：`$CLI merge $OUT/bad.pdf $PDF_TEXT`
- [ ] 预期：返回码非 `0`，打印帮助或参数错误

## CLI-02 `text extract`（plain）

- [ ] 执行：`$CLI text extract --input $PDF_TEXT --output $OUT/text_plain.txt`
- [ ] 预期：输出 `text extracted pages=... entries=...`
- [ ] 检查：`test -s $OUT/text_plain.txt`

## CLI-03 `text extract`（json + 坐标）

- [ ] 执行：`$CLI text extract --input $PDF_MIX --output $OUT/text_pos.json --json --include-positions`
- [ ] 预期：返回码 `0`
- [ ] 检查：`rg '"entries"|page|x|y' $OUT/text_pos.json`

失败路径：
- [ ] 执行：`$CLI text extract --input $PDF_TEXT`
- [ ] 预期：返回码非 `0`，报 `requires --input and --output`

## CLI-04 `attachments extract`

无附件 PDF 基线（当前仓库可稳定执行）：
- [ ] 执行：`mkdir -p $OUT/attachments && $CLI attachments extract --input $PDF_TEXT --out-dir $OUT/attachments`
- [ ] 预期：输出 `attachments extracted count=0` 或非负计数，返回码 `0`

失败路径：
- [ ] 执行：`$CLI attachments extract --input $OUT/not_found.pdf --out-dir $OUT/attachments`
- [ ] 预期：返回码非 `0`，输出 `Error: ...`

## CLI-05 `pdf info`

- [ ] 执行：`$CLI pdf info --input $PDF_MIX`
- [ ] 预期：输出 `pdf info pages=<N>`，`N>0`

失败路径：
- [ ] 执行：`$CLI pdf info --input $OUT/missing.pdf`
- [ ] 预期：返回码非 `0`

## CLI-06 `image2pdf`（fit）

- [ ] 执行：`$CLI image2pdf --output $OUT/images_fit.pdf --images $IMG1 $IMG2`
- [ ] 预期：输出 `image2pdf pages=2 skipped_images=0`
- [ ] 执行：`$CLI pdf info --input $OUT/images_fit.pdf`
- [ ] 预期：`pages=2`

## CLI-07 `image2pdf`（原始尺寸）

- [ ] 执行：`$CLI image2pdf --output $OUT/images_original.pdf --images $IMG1 $IMG2 --use-image-size --original-size`
- [ ] 预期：返回码 `0`，输出页数为 `2`

失败路径：
- [ ] 执行：`$CLI image2pdf --output $OUT/images_bad.pdf --images $IMG1 $OUT/not_found.png`
- [ ] 预期：成功但 `skipped_images>=1`，或直接错误返回（两者皆可接受，记录实际行为）

## CLI-08 `page delete`

- [ ] 先生成输入：`$CLI merge $OUT/page_src.pdf $PDF_TEXT $PDF_IMAGE`
- [ ] 执行：`$CLI page delete --input $OUT/page_src.pdf --output $OUT/page_deleted.pdf --page 1`
- [ ] 预期：输出 `page delete pages=...`，且输出页数 = 原页数 - 1

失败路径：
- [ ] 执行：`$CLI page delete --input $OUT/page_src.pdf --output $OUT/page_deleted_fail.pdf --page 9999`
- [ ] 预期：返回码非 `0`

## CLI-09 `page insert`

- [ ] 执行：`$CLI page insert --input $PDF_TEXT --output $OUT/page_inserted.pdf --at 1 --source $PDF_IMAGE --source-page 1`
- [ ] 预期：输出 `page insert pages=...`，且输出页数 = 原页数 + 1

失败路径：
- [ ] 执行：`$CLI page insert --input $PDF_TEXT --output $OUT/page_insert_fail.pdf --at x --source $PDF_IMAGE --source-page 1`
- [ ] 预期：返回码非 `0`，报数值参数错误

## CLI-10 `page replace`

- [ ] 执行：`$CLI page replace --input $PDF_MIX --output $OUT/page_replaced.pdf --page 1 --source $PDF_TEXT --source-page 1`
- [ ] 预期：输出 `page replace pages=...`，页数不变

失败路径：
- [ ] 执行：`$CLI page replace --input $PDF_MIX --output $OUT/page_replace_fail.pdf --page 0 --source $PDF_TEXT --source-page 1`
- [ ] 预期：返回码非 `0`

## CLI-11 `convert pdf2docx`（默认）

- [ ] 执行：`$CLI convert pdf2docx --input $PDF_MIX --output $OUT/mix_default.docx`
- [ ] 预期：输出 `pdf2docx pages=... images=... warnings=...`
- [ ] 检查：`test -s $OUT/mix_default.docx`
- [ ] 检查：`unzip -l $OUT/mix_default.docx | rg 'word/document.xml'`

## CLI-12 `convert pdf2docx`（dump-ir + no-images + anchored）

- [ ] 执行：`$CLI convert pdf2docx --input $PDF_MIX --output $OUT/mix_anchored.docx --dump-ir $OUT/mix_ir.json --no-images --docx-anchored`
- [ ] 预期：返回码 `0`，`$OUT/mix_ir.json` 存在
- [ ] 检查：`rg '"pages"|spans|images' $OUT/mix_ir.json`

---

## 2. GUI Tools 手工测试（Qt6）

启动：
- [ ] 执行：`$GUI`
- [ ] 预期：主窗口包含左侧导航（PDF 合并 / 文本提取 / 附件提取 / 图片转 PDF / 页面编辑 / PDF 转 DOCX / 设置）

## GUI-01 页面：PDF 合并（含“当前 PDF 预览”）

- [ ] 导入 `$PDF_TEXT` 与 `$PDF_IMAGE`
- [ ] 检查“输入摘要”显示文件数与预计总页数
- [ ] 在“当前 PDF 预览”中选择文件，设置页码，点击“刷新预览”
- [ ] 预期：出现“预览完成”且预览区显示页面内容
- [ ] 设置输出 `$OUT/gui_merge.pdf`，点击“开始合并”
- [ ] 预期：任务中心出现成功任务，输出文件存在

## GUI-02 页面：文本提取（含指定页预览）

- [ ] 输入 `$PDF_MIX` 后，检查“输入 PDF 信息”显示页数
- [ ] 调整“起始页/结束页”，并在预览区选择页码点击“刷新预览”
- [ ] 预期：预览摘要显示“预览完成”
- [ ] 输出到 `$OUT/gui_text.json`，格式选择 JSON，勾选“包含位置信息”
- [ ] 点击“开始提取”，预期任务成功且输出文件非空

## GUI-03 页面：附件提取

- [ ] 输入 `$PDF_TEXT`，输出目录 `$OUT/gui_attachments`
- [ ] 点击“开始提取附件”
- [ ] 预期：任务完成；无附件时应成功返回 `0` 数量而不是崩溃
- [ ] 失败路径：输入不存在的 PDF，预期弹出参数错误

## GUI-04 页面：图片转 PDF（含缩略图预览）

- [ ] 选择 `$IMG1`、`$IMG2`
- [ ] 预期：预览区显示 2 张可读图片；若加入不存在路径，显示“无法读取 N 张”
- [ ] 输出 `$OUT/gui_images_fit.pdf`，默认策略提交任务
- [ ] 勾选“页面尺寸跟随图片原始尺寸”再执行一次到 `$OUT/gui_images_origin.pdf`
- [ ] 两次任务都应成功，产物可通过 `pdf info` 查看页数

## GUI-05 页面：页面编辑（删除/插入/替换 + 预览）

删除：
- [ ] 输入 `$PDF_MIX`，删除页设为 `1`，刷新预览并确认成功
- [ ] 输出 `$OUT/gui_delete.pdf`，执行后页数减少 1

插入：
- [ ] 输入 `$PDF_TEXT`，来源 `$PDF_IMAGE`，来源页 `1`，插入位置 `1`
- [ ] 刷新预览（来源页），输出 `$OUT/gui_insert.pdf`
- [ ] 预期：任务成功且页数 +1

替换：
- [ ] 输入 `$PDF_MIX`，来源 `$PDF_TEXT`，替换页 `1`，来源页 `1`
- [ ] 刷新预览（来源页），输出 `$OUT/gui_replace.pdf`
- [ ] 预期：任务成功且页数不变

## GUI-06 页面：PDF 转 DOCX（IR 预览 + 提图预览）

- [ ] 输入 `$PDF_MIX` 后检查页数标签
- [ ] 点击“刷新 IR 预览”
- [ ] 预期：摘要显示总页数/渲染页数/文本块/图片计数；提图预览列表非崩溃
- [ ] 输出 `$OUT/gui_mix.docx`，勾选“同时导出 IR JSON”，IR 输出 `$OUT/gui_mix_ir.json`
- [ ] 分别测试：
  - [ ] 默认（提取图片开启）
  - [ ] 勾选“DOCX 使用 anchored 图片”
  - [ ] 关闭“提取图片”
- [ ] 预期：任务均可完成，docx/json 产物存在

## GUI-07 页面：设置

- [ ] 修改“完成任务保留上限”，点击 Apply，重启 GUI 后值保持
- [ ] 修改“最大并发任务数”，Apply 后新任务按并发上限调度
- [ ] 设置“任务报告目录”为 `$OUT/gui_reports`，Apply
- [ ] 点击“清空最近参数历史”，预期各页面历史建议被清空
- [ ] 点击“清空任务历史记录”，任务中心终态任务即时清除

## GUI-08 任务中心（全局）

- [ ] 观察任务状态流转：`Queued -> Running -> Succeeded/Failed/Canceled`
- [ ] 验证状态过滤：`All / Queued / Running / Succeeded / Failed / Canceled`
- [ ] 关键字搜索：输入摘要中的关键词，列表被正确过滤
- [ ] `Open Output`：成功任务可打开输出文件或父目录
- [ ] `Export Report`：导出文本报告并检查含 `TaskId/Status/Summary/Detail`
- [ ] `Cancel Task`：对排队任务可取消；非排队任务按钮应禁用

---

## 3. 完成判定（Exit Criteria）

- [ ] CLI 12 个测试项全部通过（含成功/失败路径）
- [ ] GUI 8 个页面/模块测试项全部通过
- [ ] 无崩溃、无卡死、无“任务显示成功但文件缺失”
- [ ] 所有产物统一留存在 `/tmp/pdftools_manual_cli_gui` 便于复盘

---

## 4. 缺陷记录模板（建议）

每个失败项记录以下字段：

- [ ] Tool 名称（CLI 命令或 GUI 页面）
- [ ] 输入参数/步骤
- [ ] 实际行为（含报错文本）
- [ ] 期望行为
- [ ] 复现概率（必现/偶现）
- [ ] 环境信息（Debug/Release、系统版本）
