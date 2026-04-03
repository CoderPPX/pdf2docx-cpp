# 模块 05：DOCX 写出（tinyxml2 + minizip-ng）

## 1) 模块定位

本模块负责将 `ir::Document` 写成最小可读 `.docx`，当前已支持：
1. 文本段落写出。
2. 页面分页符写出。
3. 图片媒体写出与关系绑定。
4. inline 与 anchored 两种图片 drawing 路径（开关控制）。
5. 最小 `styles.xml` 写出。

核心文件：
- `cpp_pdf2docx/src/docx/p0_writer.hpp`
- `cpp_pdf2docx/src/docx/p0_writer.cpp`

---

## 2) 当前接口（真实）

```cpp
struct DocxWriteOptions {
  bool use_anchored_images = false;
};

class P0Writer {
 public:
  Status WriteFromIr(const ir::Document& document,
                     const std::string& output_docx,
                     const ConvertStats& stats,
                     const DocxWriteOptions& options = {}) const;
};
```

说明：
- 默认 `use_anchored_images=false`，兼容原 `wp:inline`。
- `Converter` 会把 `ConvertOptions.docx_use_anchored_images` 传入 writer。

---

## 3) 当前 DOCX part 结构

固定写入：
1. `[Content_Types].xml`
2. `_rels/.rels`
3. `word/document.xml`
4. `word/styles.xml`
5. `word/_rels/document.xml.rels`
6. `word/media/imageN.ext`（有图时）

---

## 4) 文本与分页映射

1. 每个 `TextSpan` -> 一个 `w:p/w:r/w:t`。
2. 每页末尾追加 `w:br w:type="page"`。
3. 仍未做 run 合并与语义段落建模（由后续 pipeline 负责）。

---

## 5) 图片映射

## 5.1 媒体收集
- 读取 `ir::Page.images`。
- 生成 `rIdN`、`word/media/imageN.ext`。
- `pt -> emu`：`1pt = 12700 emu`。

## 5.2 inline 路径（默认）
- `w:drawing/wp:inline`。
- `wp:extent` 写尺寸。
- `a:blip r:embed="rIdN"` 绑定关系。

## 5.3 anchored 路径（可选）
- `w:drawing/wp:anchor`。
- 写 `wp:positionH/wp:positionV`（相对 page）。
- 当前是基于 IR 坐标的近似定位实现，非最终高保真布局。

---

## 6) styles.xml（当前）

已输出最小样式模板：
- `w:docDefaults`
- 默认字体 `Calibri`
- 默认字号 `w:sz=22`

并在：
- `[Content_Types].xml` 增加 `styles.xml` override；
- `word/_rels/document.xml.rels` 增加 styles relationship（`rIdStyles`）。

---

## 7) ZIP 写出实现

使用 minizip compat API：
- `zipOpen64`
- `zipOpenNewFileInZip`
- `zipWriteInFileInZip`
- `zipCloseFileInZip`
- `zipClose`

内部封装：
- `AddZipEntry(zipFile, const std::string&)`
- `AddZipEntry(zipFile, const void*, size_t)`

---

## 8) 测试覆盖（当前）

1. `tests/unit/writer_test.cpp`
   - 核验产物存在与容器合法性。
2. `tests/unit/docx_image_test.cpp`
   - 通过 ZIP 目录解析验证 `word/media/*` 与 `word/styles.xml`。
3. `tests/unit/docx_anchor_test.cpp`
   - 解压 `word/document.xml` 并断言 `<wp:anchor>` 等锚定节点。
4. `tests/integration/end_to_end_test.cpp`
   - 端到端验证含图 PDF 写出结果。

---

## 9) 已知限制

1. anchored 仍是近似映射，复杂旋转/裁剪场景未精确覆盖。
2. 样式体系仍是最小模板（无 numbering/theme/header/footer）。
3. 文字布局仍依赖基础 span 顺序，未完成语义段落重建。

---

## 10) 后续任务

## P1（优先）
1. anchored 精度提升（旋转、叠放、环绕策略）。
2. 样式复用与段落样式抽象，减少 XML 冗余。

## P2
1. 增加 `numbering.xml`、页眉页脚等 part。
2. 增加 Office 版本兼容性抽检矩阵。

---

## 11) 最新验证（2026-04-03）

- `ctest --preset linux-debug`：`16/16` 通过。
- `test-image-text.pdf` 实跑产物：
  - `final_image_text.docx` 可生成，且图片与样式 part 存在。

---

## 12) 增量进展（2026-04-03, M12 第一阶段）

已落地 anchored 几何精度改进：
1. `CollectDocxImages` 在图片有 `quad` 时，优先用四角点求边界并映射锚定位置。
2. 从 `quad.p0 -> quad.p1` 估算旋转角，写入 `a:xfrm rot`（单位 1/60000 度）。
3. `docx_anchor_test` 新增断言：
   - `wp:positionH/wp:positionV` 的 `posOffset` 与 quad 推导一致；
   - `document.xml` 中存在 `rot=\"...\"` 属性。

当前回归：`ctest --preset linux-debug` 通过（`16/16`）。

---

## 13) 增量进展（2026-04-03, 文本排版热修）

针对文本密集 PDF 在 Word 中“段落过于稀疏”的问题，已在文本段落写出时添加紧凑段落属性：
1. `w:spacing w:before=\"0\" w:after=\"0\"`
2. `w:line=\"240\" w:lineRule=\"auto\"`

效果：
- 保持现有 `span -> p/r/t` 映射不变；
- 显著减少默认段前后距导致的纵向拉散。
