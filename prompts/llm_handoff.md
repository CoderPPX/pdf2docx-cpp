# LLM 续作详细交接说明（图片版 PDF -> IR / HTML / DOCX）

- 更新时间：`2026-04-02 22:33:07 EDT (-0400)`
- 项目根目录：`/home/zhj/Projects/pdftools`
- 主工程目录：`/home/zhj/Projects/pdftools/cpp_pdf2docx`
- 当前目标状态：**已可将含图片的 `build/test.pdf` 转为 IR(JSON)、HTML、DOCX（最小可用）**

---

## A. 本次交付概览（给下一位 LLM 的一句话）

已经完成“图片链路最小打通”：PoDoFo 抽取图片进入 IR，IR 可以渲染到 HTML（base64 内嵌），并可写入 DOCX 的 `word/media` + `document.xml.rels` + `w:drawing`，并且 `ctest` 11 项全部通过。

---

## B. 当前可用能力与边界

### B.1 已可用能力
1. **PDF -> IR（文本 + 图片）**
   - 文本：按页抽取 `spans`
   - 图片：按页抽取 `images`，包含二进制数据与位置尺寸
2. **IR -> HTML（绝对定位预览）**
   - 文本绝对定位
   - 图片 `<img src="data:...;base64,...">` 内嵌显示
3. **IR -> DOCX（最小可读）**
   - 输出合法 `.docx` ZIP
   - 图片写入 `word/media/imageN.ext`
   - 图片关系写入 `word/_rels/document.xml.rels`
   - `word/document.xml` 插入 inline drawing
4. **CLI**
   - `pdf2docx`
   - `ir2html`
   - `pdf2ir`（新增）

### B.2 当前边界/限制
1. 图片流当前主要覆盖：
   - `DCTDecode`（JPEG）
   - `JPXDecode`（JP2）
2. `ICCBased` 颜色空间会出现 PoDoFo warning：
   - `Unsupported color space filter ICCBased`
   - 目前不阻塞 JPEG 图片抽取和输出
3. DOCX 图片暂为“最小 inline drawing”，不是高保真 anchored 布局
4. 图片几何使用 CTM 的轴对齐包围盒（AABB），旋转/剪切下非像素级精准

---

## C. 关键代码变更（逐文件）

## C.1 IR 模型扩展
- 文件：`cpp_pdf2docx/include/pdf2docx/ir.hpp`
- 关键新增：
  - `ir::ImageBlock`：`id/mime_type/extension/x/y/width/height/data`
  - `ir::Page::images`
- 说明：
  - `data` 用 `std::vector<uint8_t>`，供 HTML base64、DOCX media 直接消费

## C.2 后端抽图（PoDoFo）
- 文件：
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.hpp`
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
- 关键接口变化：
  - `ExtractToIr(const std::string&, const ConvertOptions&, ir::Document*)`
- 算法主线（实现细节）：
  1. `PdfTextExtractParams` 继续抽文本到 `spans`
  2. 若 `options.extract_images == true`：
     - `PdfContentStreamReader reader(page)`
     - 维护：
       - `current_matrix`
       - `graphics_state_stack`（`q/Q`）
       - `form_context_stack`（Form 嵌套上下文）
  3. `Operator` 分支处理：
     - `q`：压栈
     - `Q`：出栈恢复
     - `cm`：`current_matrix = current_matrix * MatrixFromCmOperands(...)`
  4. `BeginFormXObject`：
     - 保存当前 matrix + 栈深
     - 叠加 form matrix
  5. `EndFormXObject`：
     - 恢复 form 进入前 matrix 与栈深
  6. `DoXObject`：
     - 仅处理 `PdfXObjectType::Image`
     - 尝试从 `stream->GetFilters()` 推断格式：
       - `DCTDecode -> jpg/image/jpeg`
       - `JPXDecode -> jp2/image/jp2`
     - 使用 `stream->GetCopySafe()` 取字节
     - 用 `current_matrix * xobject->GetMatrix()` 计算图片矩阵
     - 取单位矩形 `(0,0)(1,0)(0,1)(1,1)` 变换后的 AABB 作为 `x/y/w/h`
     - 组装 `ImageBlock` 并挂到当前页
- 失败策略：
  - 不支持滤镜 -> 跳过该图（不中断整页）
  - 空 stream -> 跳过该图

## C.3 Converter 统计链路更新
- 文件：`cpp_pdf2docx/src/api/converter.cpp`
- 变更：
  - `ExtractIrFromFile` 把 `options` 传给后端
  - `ConvertStats.image_count` 由 IR 汇总函数 `CountImages` 计算

## C.4 IR -> HTML 图片渲染
- 文件：`cpp_pdf2docx/src/core/ir_html.cpp`
- 变更：
  - 新增 `Base64Encode(const std::vector<uint8_t>&)`
  - 样式新增 `.image` / `.image.debug`
  - 页面空白提示条件改为：`spans` 和 `images` 都为空
  - 对每个 `ImageBlock` 输出：
    - `class="image"`
    - `data-id=...`
    - `style="left/top/width/height"`
    - `src="data:<mime>;base64,<payload>"`
- 坐标映射约定：
  - PDF 以左下为原点
  - HTML 以左上为原点
  - `css_top = page.height_pt - image.y - image.height`

## C.5 IR -> DOCX 图片写出
- 文件：`cpp_pdf2docx/src/docx/p0_writer.cpp`
- 关键新增结构：
  - `DocxImageRef`（关系 ID、part 路径、EMUs、二进制）
- 核心函数：
  1. `CollectDocxImages`：从 IR 按页收集图片，生成 `rIdN`、`imageN.ext`
  2. `BuildDocumentXml(..., images)`：在每页文本后插入图片段落（inline drawing）
  3. `BuildDocumentRelsXml(images)`：生成文档关系
  4. `BuildContentTypesXml(images)`：动态写 `<Default Extension=... ContentType=...>`
  5. `AddZipEntry` 重载为支持二进制写入
- 单位换算：
  - `PtToEmu(pt) = round(pt * 12700)`
- 打包 part：
  - `[Content_Types].xml`
  - `_rels/.rels`
  - `word/document.xml`
  - `word/_rels/document.xml.rels`（有图时）
  - `word/media/imageN.ext`

## C.6 新工具 `pdf2ir`
- 文件：`cpp_pdf2docx/tools/pdf2ir/main.cpp`
- 作用：
  - 将 PDF 提取结果落到 JSON，便于人/LLM 检查 IR
- JSON 含：
  - `pages[]`
  - 每页 `spans[]`
  - 每页 `images[]`（含 `data_size`，不直接输出二进制）

## C.7 CMake 与测试接入
- 文件：`cpp_pdf2docx/CMakeLists.txt`
- CLI 新增 target：
  - `pdf2ir`
- 测试新增 target：
  - `pdf2docx_ir_html_image_test`
  - `pdf2docx_docx_image_test`
- 现有测试总数：`11`

---

## D. 测试与验收状态

## D.1 当前测试清单（`ctest -N`）
1. `status_test`
2. `backend_test`
3. `writer_test`
4. `converter_test`
5. `freetype_test`
6. `pipeline_test`
7. `ir_html_test`
8. `ir_html_image_test`
9. `build_info_test`
10. `docx_image_test`
11. `test_pdf_fixture_test`

## D.2 最新回归结果
- 命令：
  - `cmake --build --preset linux-debug -j4`
  - `ctest --preset linux-debug`
- 结果：
  - `100% tests passed, 0 failed (11/11)`

## D.3 `build/test.pdf` 实跑命令（已执行）
```bash
cd /home/zhj/Projects/pdftools/cpp_pdf2docx
./build/linux-debug/pdf2ir build/test.pdf build/linux-debug/test_ir.json
./build/linux-debug/ir2html build/test.pdf build/linux-debug/test_ir_images.html --scale 1.3
./build/linux-debug/pdf2docx build/test.pdf build/linux-debug/test_images.docx
```

## D.4 产物确认（已执行）
- `test_ir.json`：存在，含 `images`，每页可见 `data_size`
- `test_ir_images.html`：存在，含 `<img class="image ...">` + `data:image/jpeg;base64,...`
- `test_images.docx`：存在，zip 内含：
  - `word/_rels/document.xml.rels`
  - `word/media/image*.jpg`

---

## E. IR / 坐标 / 语义约定（续作必须一致）

1. `ir::Page.width_pt / height_pt` 为 PDF 点单位
2. `TextSpan.y` 和 `ImageBlock.y` 语义均按 PDF 坐标系（左下原点）
3. HTML 渲染阶段负责做 y 轴翻转
4. DOCX 目前不做绝对定位，图片是按流式段落插入
5. `ImageBlock.id` 生成规则：
   - `p<page>_img<index>`
   - 仅用于可读追踪，不做全局唯一约束

---

## F. 已知风险与潜在故障点

1. **PoDoFo 过滤链限制**
   - 非 DCT/JPX 的图片可能被跳过
2. **Const API 适配**
   - 当前对 `GetFilters()` 通过 `const_cast` 读取（PoDoFo API const 不友好）
   - 续作需注意不引入副作用
3. **DOCX 兼容性**
   - 当前 drawing 结构是最小可用
   - 对复杂 Office 版本/渲染器的表现差异未覆盖
4. **AABB 近似**
   - 旋转后的图像尺寸与定位可能偏大/偏小（因为包围盒而非精确矩形）

---

## G. 下一步推荐实施计划（给下一位 LLM）

## G.1 P1（优先，可快速交付）
1. **扩展图片格式**
   - 支持 `FlateDecode` 图片导出为 PNG（可借助 PoDoFo decode 或轻量编码路径）
2. **完善 `pdf2ir` 输出**
   - 增加每页汇总：`span_count/image_count`
   - 增加文档级 `total_span_count/total_image_count`
3. **增强测试稳定性**
   - 用 unzip 读取 ZIP 目录验证 `word/media`，替代 `strings` 检测

### P1 验收标准
- 对 `test.pdf`：`image_count` 与 media 文件数一致
- `ctest` 仍全绿（>=11/11）

## G.2 P2（质量提升）
1. **DOCX anchored 布局**
   - 按页坐标 + EMU 输出锚定位置（而非纯 inline）
2. **矩形精度提升**
   - 保留四角点，减少旋转场景误差
3. **warning 收敛**
   - 把 PoDoFo warning 统计到 `ConvertStats.warning_count`

### P2 验收标准
- 抽样 PDF 与 HTML 位置误差可视化对齐
- Word 中图片位置接近 PDF 页面布局

---

## H. 关键文件索引（快速导航）

### 核心实现
- `cpp_pdf2docx/include/pdf2docx/ir.hpp`
- `cpp_pdf2docx/src/backend/podofo/podofo_backend.hpp`
- `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
- `cpp_pdf2docx/src/api/converter.cpp`
- `cpp_pdf2docx/src/core/ir_html.cpp`
- `cpp_pdf2docx/src/docx/p0_writer.cpp`
- `cpp_pdf2docx/tools/pdf2ir/main.cpp`

### 构建与测试
- `cpp_pdf2docx/CMakeLists.txt`
- `cpp_pdf2docx/CMakePresets.json`
- `cpp_pdf2docx/tests/unit/converter_test.cpp`
- `cpp_pdf2docx/tests/unit/test_pdf_fixture_test.cpp`
- `cpp_pdf2docx/tests/unit/ir_html_image_test.cpp`
- `cpp_pdf2docx/tests/unit/docx_image_test.cpp`

### 过程记录
- `prompts/worklog.md`（含阶段时间线）

---

## I. 复现模板（下一位 LLM 直接可跑）

```bash
cd /home/zhj/Projects/pdftools/cpp_pdf2docx
cmake --preset linux-debug
cmake --build --preset linux-debug -j4
ctest --preset linux-debug

./build/linux-debug/pdf2ir build/test.pdf build/linux-debug/test_ir.json
./build/linux-debug/ir2html build/test.pdf build/linux-debug/test_ir_images.html --scale 1.3
./build/linux-debug/pdf2docx build/test.pdf build/linux-debug/test_images.docx
```

若需快速检查图片是否进入 DOCX：
```bash
strings build/linux-debug/test_images.docx | rg 'word/media/image|document.xml.rels'
```

---

## J. 交接结论

当前分支已具备“含图片 PDF 的最小可用转换链路”，并通过本地回归。下一步建议优先做图片格式覆盖与 DOCX 定位质量提升，以便从“可用”迈向“可交付”。

---

## K. 模块设计文档索引（详细版）

为便于下一位 LLM 按模块接手，已维护如下文档：
- `prompts/module-01-build-system.md`
- `prompts/module-02-backend-podofo.md`
- `prompts/module-03-ir-and-pipeline.md`
- `prompts/module-04-font-system.md`
- `prompts/module-05-docx-writer.md`
- `prompts/module-06-api-and-cli.md`
- `prompts/module-07-test-ci-release.md`
- `prompts/module-08-image-pipeline.md`
- `prompts/module-09-tools-and-debug.md`
