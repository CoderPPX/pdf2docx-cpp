# 模块 05：DOCX 写出（tinyxml2 + minizip-ng）详细设计

## 1) 模块定位

本模块把 `ir::Document` 写出为最小合法 `.docx`，当前已支持：
- 文本段落
- 页面分隔符
- 图片媒体与最小 drawing 引用

核心文件：
- `cpp_pdf2docx/src/docx/p0_writer.hpp`
- `cpp_pdf2docx/src/docx/p0_writer.cpp`

---

## 2) 当前接口与输入输出

```cpp
class P0Writer {
 public:
  Status WriteFromIr(const ir::Document& document,
                     const std::string& output_docx,
                     const ConvertStats& stats) const;
};
```

输入：
- `ir::Document`
- `output_docx`
- `ConvertStats`（当前主要用于降级文本输出）

输出：
- 成功：合法 `.docx` 文件
- 失败：`Status::Error(...)`

---

## 3) 当前 DOCX 结构（已实现）

固定写入 part：
1. `[Content_Types].xml`
2. `_rels/.rels`
3. `word/document.xml`
4. `word/_rels/document.xml.rels`（有图片时）
5. `word/media/imageN.ext`（每张图片）

---

## 4) 文本映射规则（现状）

1. 每个 `TextSpan` -> 一个 `w:p/w:r/w:t`
2. 每页末尾追加一个分页段落：
   - `w:br w:type="page"`
3. 目前不做 run 合并、样式抽取、段落级语义映射

---

## 5) 图片映射规则（现状）

## 5.1 图片收集
- 从 `ir::Page.images` 顺序遍历
- 生成：
  - `relationship_id = rIdN`
  - `file_name = imageN.ext`
  - `part_name = word/media/imageN.ext`

## 5.2 MIME 与扩展名
- 优先使用 IR 携带值
- 扩展名标准化（去 `.`、转小写）
- 未给 mime 时按扩展名推断

## 5.3 尺寸换算
- `pt -> emu`：`1pt = 12700 emu`
- 写入 `wp:extent cx/cy`

## 5.4 document.xml 中图片节点
- 当前写最小 `w:drawing/wp:inline` 结构
- `a:blip r:embed="rIdN"` 引用关系

## 5.5 document.xml.rels
- 每图一条：
  - `Type=image`
  - `Target=media/imageN.ext`

---

## 6) ZIP 写入实现细节

使用 `minizip` compat API：
- `zipOpen64`
- `zipOpenNewFileInZip`
- `zipWriteInFileInZip`
- `zipCloseFileInZip`
- `zipClose`

当前封装：
- `AddZipEntry(zipFile, entry, std::string)`
- `AddZipEntry(zipFile, entry, void*, size_t)`（二进制）

---

## 7) 降级路径（无 tinyxml2 或无 minizip）

当 `PDF2DOCX_HAS_TINYXML2 && PDF2DOCX_HAS_MINIZIP` 不满足：
- 写出占位文本文件（不是 docx zip）
- 包含基础统计字段与说明

备注：
- `writer_test`/`converter_test` 已按宏区分断言

---

## 8) 测试覆盖（当前）

1. `tests/unit/writer_test.cpp`
   - 核验产物存在
   - 核验 zip 魔数/降级首字符
2. `tests/unit/docx_image_test.cpp`
   - 构造含图片 IR
   - 断言 docx 内含 `word/media/image1.jpg` 与 `word/_rels/document.xml.rels`
3. `tests/unit/converter_test.cpp`
   - 端到端转换并核验 `stats.image_count`

---

## 9) 已知限制

1. 图片当前按文档流插入，不是按 PDF 绝对坐标锚定
2. 未输出 `styles.xml`/`numbering.xml`
3. 未覆盖复杂 run 样式（加粗、字体等）
4. `document.xml` 里图片与文本可能顺序正确但位置不高保真

---

## 10) 后续详细任务卡

## P1（推荐先做）
1. 新增 `styles.xml` 最小模板，给文本 run 加基本字体/字号。
2. 增加 docx unzip 级别测试（实际读取 central directory，而非 `strings`）。
3. `pdf2docx` CLI 增加 `--docx-minimal` / `--docx-anchored` 模式开关（anchored 先占位）。

## P2（质量提升）
1. 实现 anchored drawing（按坐标布局）。
2. 处理图片旋转/裁剪到 Word 变换模型。
3. 引入段落/样式复用，减少 XML 体积。

---

## 11) 验收标准

1. 现有样本 DOCX 在 Word/LibreOffice 可打开。
2. 增加新能力后，现有 11 项测试不回归。
3. 对 `build/test.pdf`，`word/media/` 文件数与 IR 图片计数一致。
