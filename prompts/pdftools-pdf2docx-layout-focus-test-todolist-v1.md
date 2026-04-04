# PDF->DOCX 排版正确性专项测试（重点 Tool + 测试方法）

- 更新时间：`2026-04-04`
- 目标：你关心的是“**排版是否正确**”，本清单只聚焦这个目标，不做泛功能覆盖。

---

## 1. 需要重点测试的 Tools（按优先级）

## P0-1 `pdftools` CLI：`convert pdf2docx`

为什么最重要：
- 实际交付链路（库到 CLI）的标准入口。
- 最容易做批量回归与参数组合测试。

重点测什么：
- 标题/正文换行与段落顺序是否正确。
- 图片是否缺失、位置是否漂移、是否覆盖文字。
- `--no-images / --docx-anchored / --disable-font-fallback` 对排版的影响。

---

## P0-2 `pdftools_gui`：`PDF 转 DOCX` 页面 + `IR 预览`

为什么重要：
- 最终用户入口。
- 能直接看 IR 预览与提图预览，便于定位“源数据 vs DOCX”差异。

重点测什么：
- GUI 生成结果与 CLI 一致性。
- 预览中有图，但 DOCX 无图/错位时的定位能力。
- 指定页预览与最终 DOCX 该页一致性（尤其是 title 位置）。

---

## P0-3 `ir2html`（`cpp_pdf2docx`）

为什么重要：
- 当 DOCX 排版错时，用来判断是“IR 抽取错”还是“DOCX 渲染错”。

重点测什么：
- 问题页（例如 title 乱、图片丢失页）在 IR 预览里是否已错。
- 若 IR 正常但 DOCX 异常，问题大概率在 writer/布局映射阶段。

---

## P1-辅助 `pdftools pdf info` / `text extract`

用途：
- `pdf info`：校验页数一致。
- `text extract --json --include-positions`：辅助判断文本顺序与坐标是否异常。

---

## 2. 测试素材（建议固定）

- `cpp_pdf2docx/build/image-text.pdf`（主样本，历史上出现 title 乱/图片异常）
- `cpp_pdf2docx/build/text.pdf`（纯文本基线）
- `cpp_pdf2docx/build/image.pdf`（图片密集基线）

输出目录建议：

```bash
mkdir -p /tmp/pdftools_layout_focus
```

---

## 3. 每个 Tool 的手工测试 TODO

## Tool-A：`pdftools convert pdf2docx`（主测试）

二进制：
- `cpp_pdftools/build/linux-debug/pdftools`

### A1 基线转换
- [ ] `pdftools convert pdf2docx --input image-text.pdf --output a_default.docx --dump-ir a_default_ir.json`
- [ ] 预期：命令成功，`docx/json` 均存在。

### A2 参数组合回归
- [ ] `--no-images`：检查文字排版是否受影响（不应出现 title 乱序/异常换行扩大）
- [ ] `--docx-anchored`：检查图片锚定后是否与文字重叠或漂移
- [ ] `--disable-font-fallback`：检查 CJK/特殊符号是否乱码或行高异常

### A3 排版检查点（人工开 Word/WPS）
- [ ] 标题：字号、换行、是否被拆成多段错位
- [ ] 正文：段落顺序与阅读顺序一致
- [ ] 图片：是否都加载成功；是否有空白占位
- [ ] 图文关系：图片上下文文本是否仍在正确位置
- [ ] 跨页：段落是否被错误挤压到上一页/下一页

### A4 快速结构检查（命令行）
- [ ] `unzip -l a_default.docx | rg 'word/document.xml|word/media/'`
- [ ] 比较不同参数的 `word/media` 文件数量（no-images 应显著减少）
- [ ] `rg '"pages"|spans|images' a_default_ir.json`

---

## Tool-B：`pdftools_gui` PDF->DOCX 页面（用户链路）

二进制：
- `cpp_pdftools_gui/build/linux-debug/pdftools_gui`

### B1 基线路径
- [ ] 在“PDF 转 DOCX”页输入 `image-text.pdf`
- [ ] 点击“刷新 IR 预览”，记录问题页页码（title 异常页）
- [ ] 执行转换，导出 DOCX + IR JSON

### B2 一致性验证（GUI vs CLI）
- [ ] 用同一输入/参数做一次 CLI 转换
- [ ] 对比两份 DOCX：
  - [ ] 页数一致
  - [ ] title 样式与换行一致
  - [ ] 图片数量与可见性一致

### B3 GUI 预览定位
- [ ] 若 DOCX 有错但 IR 预览正常，记录为“writer/render 侧问题”
- [ ] 若 IR 预览已错，记录为“解析/IR 生成侧问题”

---

## Tool-C：`ir2html`（IR 排障）

二进制：
- `cpp_pdf2docx/build/linux-debug/ir2html`

### C1 全文 IR 预览
- [ ] `ir2html image-text.pdf ir_full.html --scale 1.2`
- [ ] 人工查看 title 页、图片页布局。

### C2 问题页聚焦
- [ ] 结合 GUI 预览页码，重点检查问题页对应 IR 是否正常。
- [ ] 若可复现“line45/93 image empty”现象，记录页码 + 图片块计数。

---

## Tool-D：`pdftools pdf info` + `text extract`（辅助）

### D1 页数一致性
- [ ] `pdftools pdf info --input image-text.pdf`
- [ ] DOCX 打开后页数应在合理范围（允许轻微分页差异，但不能大量偏移）。

### D2 文本顺序辅助
- [ ] `pdftools text extract --input image-text.pdf --output text_pos.json --json --include-positions`
- [ ] 对 title 所在页检查文本顺序与坐标是否出现异常跳跃。

---

## 4. 重点缺陷判定标准（用于你关心的“排版正确”）

判定为“高优先级失败”的任一条件：
- [ ] title 被拆分错位，视觉上明显不符合原 PDF 阅读顺序。
- [ ] 任意关键图片未显示（空白/损坏图标/无法加载）。
- [ ] 图片与正文发生覆盖，导致可读性严重下降。
- [ ] 页内对象顺序错乱（先出现下方段落再出现上方标题）。

判定为“可接受偏差”（记录但可先不阻塞）：
- [ ] 微小字体差异（< 1 级视觉感知）。
- [ ] 行高/字距小幅变化但不影响阅读顺序。
- [ ] 页末 1-2 行跨页变化但内容顺序正确。

---

## 5. 建议执行顺序（最省时间）

1. 先跑 Tool-A（CLI）做参数组合，快速找到“最稳定排版配置”。
2. 用 Tool-B（GUI）复现同样配置，验证用户链路一致。
3. 发现问题后立刻用 Tool-C（ir2html）分层定位。
4. 用 Tool-D 做页数/文本顺序辅助确认，形成缺陷证据。

---

## 6. 缺陷记录最小模板

- 输入 PDF：
- 执行 Tool/命令：
- 参数：
- 异常页码：
- 现象（title/图片/段落顺序）：
- IR 预览是否同样异常（是/否）：
- 结论归因（IR 抽取 / DOCX 渲染 / 字体回退）：
