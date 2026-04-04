# 执行工作说明（P0 落地记录）

## 时间

- 记录时间：`2026-04-02 12:35:37 EDT (-0400)`

## 依赖检查与修正

- 已检查 `thirdparty/` 下依赖是否完备、是否来自 Gitee、是否具备 CMake 入口。
- 发现原 `thirdparty/freetype` 是 Go 版仓库（非 C/C++ FreeType）。
- 已保留该仓库为 `thirdparty/freetype-go-backup`，并替换为 `freetype2`（Gitee）到 `thirdparty/freetype`。

### 当前依赖复核结果

- `thirdparty/zlib`
  - origin: `https://gitee.com/mirrors/zlib.git`
  - `CMakeLists.txt`: `OK`
- `thirdparty/tinyxml2`
  - origin: `https://gitee.com/mirrors/tinyxml2.git`
  - `CMakeLists.txt`: `OK`
- `thirdparty/minizip-ng`
  - origin: `https://gitee.com/wshikh/minizip-ng.git`
  - `CMakeLists.txt`: `OK`
- `thirdparty/freetype`
  - origin: `https://gitee.com/mirrors/freetype2.git`
  - `CMakeLists.txt`: `OK`
- `thirdparty/podofo`
  - origin: `https://gitee.com/mycn027b/podofo.git`
  - `CMakeLists.txt`: `OK`

## P0 Framework 产出

- 新建工程目录：`cpp_pdf2docx`
- 构建与依赖：
  - `cpp_pdf2docx/CMakeLists.txt`
  - `cpp_pdf2docx/CMakePresets.json`
  - `cpp_pdf2docx/cmake/Dependencies.cmake`
- 对外 API：
  - `cpp_pdf2docx/include/pdf2docx/status.hpp`
  - `cpp_pdf2docx/include/pdf2docx/types.hpp`
  - `cpp_pdf2docx/include/pdf2docx/converter.hpp`
- 模块骨架：
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
  - `cpp_pdf2docx/src/pipeline/pipeline.cpp`
  - `cpp_pdf2docx/src/docx/p0_writer.cpp`
  - `cpp_pdf2docx/src/api/converter.cpp`
- CLI 与测试：
  - `cpp_pdf2docx/tools/cli/main.cpp`
  - `cpp_pdf2docx/tests/unit/status_test.cpp`
- 项目说明：`cpp_pdf2docx/README.md`

## 本地验证结果

- `cmake --preset linux-debug`：通过
- `cmake --build --preset linux-debug`：通过
- `ctest --preset linux-debug`：通过（`1/1`）

## 当前边界说明

- 已完成跨平台 CMake 骨架与模块接口打通。
- 当前 `Converter` 走的是 P0 占位链路（可运行、可测试）。
- 下一阶段需实现真实 `PoDoFo` 提取 + `tinyxml2/minizip-ng` 合法 DOCX 输出。

---

## P1 继续推进记录（thirdparty 导入 + 测试补齐）

- 记录时间：`2026-04-02 12:51:37 EDT (-0400)`

### 1) thirdparty 真实导入（CMake）

- 重构依赖导入层：`cpp_pdf2docx/cmake/Dependencies.cmake`
  - 支持 `find_package + add_subdirectory` 混合策略。
  - 自动识别 `thirdparty/freetype2`（兼容旧 `freetype` 路径）。
  - 对 `minizip-ng` 关闭测试与重依赖开关，避免引入不必要组件。
  - 对 `podofo` 使用 `LIB_ONLY` 模式，避免工具/示例构建。
- 顶层构建接入：`cpp_pdf2docx/CMakeLists.txt`
  - 统一调用 `pdf2docx_import_dependencies(...)`。
  - 将依赖可用性导出为编译宏：
    - `PDF2DOCX_HAS_ZLIB`
    - `PDF2DOCX_HAS_TINYXML2`
    - `PDF2DOCX_HAS_MINIZIP`
    - `PDF2DOCX_HAS_FREETYPE`
    - `PDF2DOCX_HAS_PODOFO`
  - 将解析出的第三方目标真实链接到 `pdf2docx_core`。
- 为避免静态第三方链接到共享库时的 PIC 冲突，默认改为静态核心库：
  - `PDF2DOCX_BUILD_SHARED=OFF`（`cpp_pdf2docx/CMakeLists.txt`）

### 2) P1 最小功能推进

- 新增构建信息头：`cpp_pdf2docx/include/pdf2docx/build_info.hpp`
- 新增字体探测 API：
  - 头文件：`cpp_pdf2docx/include/pdf2docx/font.hpp`
  - 实现：`cpp_pdf2docx/src/font/freetype_probe.cpp`
- 后端增强：`cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
  - 增加 PDF 头校验（`%PDF-`）。
- 转换流程增强：`cpp_pdf2docx/src/api/converter.cpp`
  - 在 `enable_font_fallback=true` 时执行 FreeType 探测。
- 写出增强：`cpp_pdf2docx/src/docx/p0_writer.cpp`
  - 若 `tinyxml2 + minizip` 可用：生成最小合法 DOCX（ZIP 容器，含 `document.xml` 等核心 part）。
  - 若依赖不可用：自动降级为原占位输出。

### 3) 测试用例补齐

新增测试：
- `cpp_pdf2docx/tests/unit/backend_test.cpp`
- `cpp_pdf2docx/tests/unit/writer_test.cpp`
- `cpp_pdf2docx/tests/unit/converter_test.cpp`
- `cpp_pdf2docx/tests/unit/freetype_test.cpp`
- `cpp_pdf2docx/tests/unit/pipeline_test.cpp`

并在 `cpp_pdf2docx/CMakeLists.txt` 中接入 CTest。当前合计测试：`6` 个。

### 4) 本地验证结果

- `cmake --preset linux-debug --fresh`：通过
- 依赖解析摘要：`zlib=1, tinyxml2=1, minizip=1, freetype=1, podofo=1`
- `cmake --build --preset linux-debug -j4`：通过
- `ctest --preset linux-debug`：通过（`6/6`）

---

## P1 持续推进记录（`build/test.pdf` + IR to HTML）

- 记录时间：`2026-04-02 22:07:54 EDT (-0400)`

### 1) 基于 PoDoFo 的最小文本提取 -> IR

- 新增 IR 结构：`cpp_pdf2docx/include/pdf2docx/ir.hpp`
- `PoDoFoBackend` 新增 IR 提取接口：
  - `ExtractToIr(...)`（`cpp_pdf2docx/src/backend/podofo/podofo_backend.hpp`）
  - 实现按页提取 `PdfTextEntry` 到 `ir::Document`（`cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`）
- `Converter` 新增：
  - `ExtractIrFromFile(...)`（`cpp_pdf2docx/include/pdf2docx/converter.hpp`）
  - `ConvertFile(...)` 改为基于 IR 再写出 DOCX（`cpp_pdf2docx/src/api/converter.cpp`）

### 2) 最小 IR -> DOCX 改造

- `P0Writer` 改造为 `WriteFromIr(...)`：
  - 声明：`cpp_pdf2docx/src/docx/p0_writer.hpp`
  - 实现：`cpp_pdf2docx/src/docx/p0_writer.cpp`
- 当 `tinyxml2 + minizip` 可用时：
  - 生成最小 OOXML `document.xml`（由 IR 文本生成段落）
  - 打包为合法 ZIP 结构 `.docx`

### 3) 新增 IR -> HTML 工具

- 新增 HTML 写出 API：
  - `cpp_pdf2docx/include/pdf2docx/ir_html.hpp`
  - `cpp_pdf2docx/src/core/ir_html.cpp`
- 新增工具：`cpp_pdf2docx/tools/ir2html/main.cpp`
  - 用法：`ir2html <input.pdf> <output.html>`
  - 流程：`PDF -> IR -> HTML`

### 4) 构建与测试补充

- CMake 接入：
  - 新增 `ir2html` 可执行文件
  - 新增 `ir_html.cpp` 到核心库
  - 新增 `PDF2DOCX_TEST_PDF_PATH` 用于 fixture 测试
- 测试新增：
  - `cpp_pdf2docx/tests/unit/ir_html_test.cpp`
  - `cpp_pdf2docx/tests/unit/build_info_test.cpp`
  - `cpp_pdf2docx/tests/unit/test_pdf_fixture_test.cpp`
- 当前测试总数：`9`

### 5) 以 `build/test.pdf` 实测结果

- `./build/linux-debug/pdf2docx build/test.pdf build/linux-debug/test_out.docx`：通过
- `./build/linux-debug/ir2html build/test.pdf build/linux-debug/test_ir.html`：通过
- 产物检查：
  - `test_out.docx` 存在且 ZIP 魔数为 `PK`
  - `test_ir.html` 存在且包含 `Page 1`
- `ctest --preset linux-debug`：通过（`9/9`）

---

## P1 持续推进记录（IR 绝对定位 HTML 预览增强）

- 记录时间：`2026-04-02 22:12:24 EDT (-0400)`

### 1) `ir2html` 渲染增强

- 新增配置结构：`IrHtmlOptions`（`scale`, `show_boxes`）
  - 文件：`cpp_pdf2docx/include/pdf2docx/ir_html.hpp`
- `WriteIrToHtml(...)` 改造为支持选项参数
  - 文件：`cpp_pdf2docx/src/core/ir_html.cpp`
- HTML 输出改为“页面画布 + 绝对定位文本 span”模式：
  - 页面容器：`page-canvas`
  - 每个文本块按坐标计算 `left/top`，并按缩放系数渲染
  - 支持可选 debug 轮廓框

### 2) `ir2html` CLI 参数增强

- 工具：`cpp_pdf2docx/tools/ir2html/main.cpp`
- 新增参数：
  - `--scale <float>`：控制预览缩放
  - `--hide-boxes`：关闭文本框可视化边框
- 输出信息补充：`scale` 与 `boxes` 状态

### 3) 测试增强

- 更新：`cpp_pdf2docx/tests/unit/ir_html_test.cpp`
  - 校验 HTML 转义
  - 校验 `page-canvas` 与绝对定位样式
- 更新：`cpp_pdf2docx/tests/unit/test_pdf_fixture_test.cpp`
  - 增加对 `page-canvas` 的结构断言

### 4) 本地验证

- `cmake --build --preset linux-debug -j4`：通过
- `ctest --preset linux-debug`：通过（`9/9`）
- `build/test.pdf` 实测：
  - `./build/linux-debug/ir2html build/test.pdf build/linux-debug/test_ir_abs.html --scale 1.60 --hide-boxes`
  - 输出文件存在，且包含 `page-canvas`、`scale=1.60`、`position:absolute`

---

## P1 持续推进记录（图片 PDF -> IR / HTML / DOCX 打通）

- 记录时间：`2026-04-02 22:27:05 EDT (-0400)`

### 1) IR 模型扩展（图片块）

- 更新 `cpp_pdf2docx/include/pdf2docx/ir.hpp`
  - 新增 `ir::ImageBlock`：
    - `id`
    - `mime_type`
    - `extension`
    - `x/y/width/height`
    - `data`（二进制字节）
  - `ir::Page` 新增 `images` 容器

### 2) PoDoFo 后端图片抽取

- 更新：
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.hpp`
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
- 能力：
  - 使用 `PdfContentStreamReader` 遍历内容流，跟踪 `q/Q/cm` 图形状态矩阵
  - 处理 `DoXObject`，识别 `PdfXObjectType::Image`
  - 当前支持从滤镜推断并提取：
    - `DCTDecode` -> `image/jpeg` (`jpg`)
    - `JPXDecode` -> `image/jp2` (`jp2`)
  - 计算图片在页面坐标系中的轴对齐包围盒，写入 IR

### 3) IR -> HTML 图片渲染

- 更新 `cpp_pdf2docx/src/core/ir_html.cpp`
- 能力：
  - 新增 base64 编码
  - 通过 `<img src="data:mime;base64,...">` 内嵌渲染图片
  - 维持“绝对定位”预览风格（与文本统一坐标映射）

### 4) IR -> DOCX 图片写出

- 更新 `cpp_pdf2docx/src/docx/p0_writer.cpp`
- 能力：
  - 将图片写入 `word/media/imageN.ext`
  - 生成 `word/_rels/document.xml.rels` 图片关系
  - 在 `word/document.xml` 插入最小 `w:drawing/wp:inline` 节点并通过 `r:embed` 引用图片
  - 更新 `[Content_Types].xml` 的图片扩展名 ContentType

### 5) 新增 `pdf2ir` 工具

- 新增 `cpp_pdf2docx/tools/pdf2ir/main.cpp`
- CMake 接入：
  - `cpp_pdf2docx/CMakeLists.txt`
- 用法：
  - `./build/linux-debug/pdf2ir input.pdf output_ir.json`
- 输出说明：
  - JSON 包含页面、文本块、图片块（含坐标与 `data_size`）

### 6) 测试增强

- 更新测试：
  - `cpp_pdf2docx/tests/unit/converter_test.cpp`
  - `cpp_pdf2docx/tests/unit/test_pdf_fixture_test.cpp`
- 新增测试：
  - `cpp_pdf2docx/tests/unit/ir_html_image_test.cpp`
  - `cpp_pdf2docx/tests/unit/docx_image_test.cpp`
- CTest 接入：
  - `cpp_pdf2docx/CMakeLists.txt`
- 当前测试总数：`11`

### 7) 本地验证结果

- 构建与回归：
  - `cmake --build --preset linux-debug -j4`：通过
  - `ctest --preset linux-debug`：通过（`11/11`）
- `build/test.pdf` 实跑：
  - `./build/linux-debug/pdf2ir build/test.pdf build/linux-debug/test_ir.json`：通过
  - `./build/linux-debug/ir2html build/test.pdf build/linux-debug/test_ir_images.html --scale 1.3`：通过
  - `./build/linux-debug/pdf2docx build/test.pdf build/linux-debug/test_images.docx`：通过
- 产物校验：
  - `test_ir.json` 含多页 `images` 节点，且每页可见 `data_size`
  - `test_ir_images.html` 含 `class="image"` 与 `data:image/jpeg;base64,...`
  - `test_images.docx` ZIP 中包含 `word/_rels/document.xml.rels` 与 `word/media/image*.jpg`

### 8) 当前限制（已明确）

- 对图片流当前主要覆盖 `DCTDecode/JPXDecode`（便于无额外 toolchain 依赖地稳定导出）。
- 在处理 `build/test.pdf` 时，PoDoFo 会输出 `Unsupported color space filter ICCBased` 警告（不影响当前 JPEG 图片抽取与输出）。

---

## 模块 01 持续推进记录（依赖版本日志 + 严格依赖 + Windows test preset）

- 记录时间：`2026-04-03 06:52:20 EDT (-0400)`

### 1) 依赖版本与来源日志增强

- 更新文件：`cpp_pdf2docx/cmake/Dependencies.cmake`
- 改动：
  - 新增依赖级日志函数，配置阶段输出：
    - `source`（thirdparty 路径或 system）
    - `target`（最终链接 target）
    - `version`（优先 target version，其次 package version）
  - 当前可见示例（linux-debug）：
    - `zlib=1.3`
    - `tinyxml2=10.0.0`
    - `minizip-ng=4.0.10`
    - `freetype2=2.13.2`
    - `podofo=1.1.0`

### 2) 严格依赖开关

- 更新文件：
  - `cpp_pdf2docx/CMakeLists.txt`
  - `cpp_pdf2docx/cmake/Dependencies.cmake`
  - `cpp_pdf2docx/CMakePresets.json`
- 改动：
  - 新增 `PDF2DOCX_STRICT_DEPS`（默认 `OFF`）
  - 开启后若任一关键依赖缺失（`zlib/tinyxml2/minizip-ng/freetype2/podofo`）则 `FATAL_ERROR` fail-fast
  - 在 presets 基线变量中显式加入该开关（默认 `OFF`）

### 3) Windows test presets 补全

- 更新文件：`cpp_pdf2docx/CMakePresets.json`
- 新增 test preset：
  - `windows-msvc-debug`
  - `windows-mingw-debug`
- 保留既有：
  - `linux-debug`
  - `windows-msvc-release`

### 4) 本地验证

- 执行：
  - `cmake --preset linux-debug`
  - `cmake --preset linux-debug -DPDF2DOCX_STRICT_DEPS=ON`
  - `cmake --build --preset linux-debug -j4`
  - `ctest --preset linux-debug`
- 结果：
  - configure（OFF/ON）均通过
  - `ctest` 全绿（`11/11`）

---

## 模块 02 + 08 持续推进记录（抽图统计 + Flate 路径 + 四角点 + warning 映射）

- 记录时间：`2026-04-03 07:14:14 EDT (-0400)`

### 1) 统计与接口扩展

- 更新文件：
  - `cpp_pdf2docx/include/pdf2docx/types.hpp`
  - `cpp_pdf2docx/include/pdf2docx/converter.hpp`
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.hpp`
  - `cpp_pdf2docx/src/api/converter.cpp`
- 新增：
  - `ImageExtractionStats`（`extracted/skipped/skip_reason/warning`）
  - `ConvertStats` 扩展字段：
    - `extracted_image_count`
    - `skipped_image_count`
    - `skipped_unsupported_filter_count`
    - `skipped_empty_stream_count`
    - `skipped_decode_failed_count`
    - `backend_warning_count`
    - `font_probe_count`
- `Converter::ExtractIrFromFile(...)` 与 `PoDoFoBackend::ExtractToIr(...)` 新增可选统计输出参数。

### 2) FlateDecode -> PNG 导出路径（P1）

- 更新文件：`cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
- 实现：
  - 检测 `FlateDecode` 时走 `PdfImage::DecodeTo(...RGBA)`；
  - 本地封装最小 PNG（`IHDR/IDAT/IEND` + zlib 压缩 + CRC32）；
  - IR 图片类型写为 `image/png` / `png`。
- 鲁棒性：
  - 对单图 decode 异常改为“单图跳过 + 统计”，不再中断整份 PDF。

### 3) 四角点几何（为 anchored 做准备）

- 更新文件：
  - `cpp_pdf2docx/include/pdf2docx/ir.hpp`
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
  - `cpp_pdf2docx/src/core/ir_html.cpp`
- IR 扩展：
  - `Point` / `Quad`
  - `ImageBlock.has_quad + quad`
- 后端：
  - 在 CTM 下保留单位矩形四角点；
  - 同时继续输出 AABB（`x/y/width/height`）。
- HTML：
  - debug 模式新增 `svg polygon` 叠加显示图片四角点轮廓。

### 4) warning 映射

- 在后端把以下事件映射到 `warning_count`：
  - 不支持滤镜跳过
  - 空流跳过
  - decode 失败跳过
  - 几何尺寸 fallback
- 在 converter 中将后端 warning 同步到 `ConvertStats.warning_count/backend_warning_count`。

### 5) 测试更新

- 更新：
  - `tests/unit/converter_test.cpp`（统计字段一致性）
  - `tests/unit/ir_html_image_test.cpp`（quad overlay）
  - `tests/unit/test_pdf_fixture_test.cpp`（至少存在 `has_quad`）
- 结果：
  - `cmake --build --preset linux-debug -j4` 通过
  - `ctest --preset linux-debug` 通过（阶段内 `12/12`）

---

## 模块 03 持续推进记录（Pipeline spans 排序阶段）

- 记录时间：`2026-04-03 07:14:14 EDT (-0400)`

### 1) Pipeline 从占位改为真实排序

- 更新文件：
  - `cpp_pdf2docx/src/pipeline/pipeline.hpp`
  - `cpp_pdf2docx/src/pipeline/pipeline.cpp`
  - `cpp_pdf2docx/src/api/converter.cpp`
- 实现：
  - `Pipeline::Execute(ir::Document*, PipelineStats*)`
  - 页内排序规则：按阅读顺序稳定排序（顶部优先：`y` 降序，再 `x` 升序）
  - 增加 `PipelineStats`：`page_count/reordered_page_count/reordered_span_count`

### 2) 单测补齐

- 更新文件：`cpp_pdf2docx/tests/unit/pipeline_test.cpp`
- 断言：
  - 空指针入参返回 `kInvalidArgument`
  - 构造乱序 spans 后排序结果符合预期
  - `PipelineStats` 统计有效

### 3) 本地验证

- `ctest --preset linux-debug` 通过（阶段内 `12/12`）

---

## 模块 04 持续推进记录（字体探测扩展 + 最小 FontResolver）

- 记录时间：`2026-04-03 07:14:14 EDT (-0400)`

### 1) FreeType probe 扩展输出

- 更新文件：
  - `cpp_pdf2docx/include/pdf2docx/font.hpp`
  - `cpp_pdf2docx/src/font/freetype_probe.cpp`
  - `cpp_pdf2docx/tests/unit/freetype_test.cpp`
- 新增 `FreeTypeProbeInfo`：
  - `available`
  - `major/minor/patch`
  - `version`
  - `has_truetype_module/has_cff_module`

### 2) 最小 FontResolver（P2 基础）

- 新增文件：
  - `cpp_pdf2docx/include/pdf2docx/font_resolver.hpp`
  - `cpp_pdf2docx/src/font/font_resolver.cpp`
  - `cpp_pdf2docx/tests/unit/font_resolver_test.cpp`
- 能力：
  - alias 映射
  - fallback family
  - family->file_path 最小映射

### 3) CMake 接入

- 更新：`cpp_pdf2docx/CMakeLists.txt`
  - core 新增 `font_resolver.cpp`
  - 新增测试 `font_resolver_test`

### 4) 本地验证

- `ctest --preset linux-debug` 通过（阶段内 `13/13`）

---

## 模块 05 持续推进记录（DOCX 媒体校验 + styles.xml + anchored 开关）

- 记录时间：`2026-04-03 07:14:14 EDT (-0400)`

### 1) Writer 选项扩展（anchored 预研）

- 更新文件：
  - `cpp_pdf2docx/src/docx/p0_writer.hpp`
  - `cpp_pdf2docx/src/docx/p0_writer.cpp`
  - `cpp_pdf2docx/src/api/converter.cpp`
  - `cpp_pdf2docx/include/pdf2docx/types.hpp`
- 新增：
  - `DocxWriteOptions{ use_anchored_images }`
  - `ConvertOptions.docx_use_anchored_images`
- 实现：
  - 默认仍 `wp:inline`
  - 开关开启时写 `wp:anchor` + `positionH/positionV`（基于 PDF 坐标近似映射）

### 2) styles.xml 最小模板

- `P0Writer` 现在固定写入：
  - `word/styles.xml`
  - `[Content_Types].xml` 增加 styles override
  - `word/_rels/document.xml.rels` 固定存在，并含 styles relationship

### 3) 媒体文件计数验证（替代 strings）

- 更新文件：
  - `cpp_pdf2docx/tests/unit/docx_image_test.cpp`
  - `cpp_pdf2docx/tests/unit/docx_anchor_test.cpp`（新增）
  - `cpp_pdf2docx/CMakeLists.txt`（新增 `docx_anchor_test`）
- 验证方式：
  - 改为解析 ZIP central directory / local header（二进制解析），统计 entry，不依赖 `strings`
  - `docx_anchor_test` 解压并断言 `word/document.xml` 中出现 `<wp:anchor>`

### 4) 本地验证

- `ctest --preset linux-debug` 通过（阶段内 `15/15`）

---

## 模块 06 + 09 持续推进记录（CLI 参数增强 + JSON summary + only-page）

- 记录时间：`2026-04-03 07:14:14 EDT (-0400)`

### 1) 通用 IR JSON 写出模块

- 新增：
  - `cpp_pdf2docx/include/pdf2docx/ir_json.hpp`
  - `cpp_pdf2docx/src/core/ir_json.cpp`
  - `cpp_pdf2docx/tests/unit/ir_json_test.cpp`
- 能力：
  - 顶层 `summary`（`total_pages/total_spans/total_images`）
  - 每页 `span_count/image_count`
  - 图片可选输出 `quad`

### 2) `pdf2ir` 升级

- 更新：`cpp_pdf2docx/tools/pdf2ir/main.cpp`
- 改为复用 `WriteIrToJson(...)`
- CLI 输出现在包含 `pages/spans/images` 汇总。

### 3) `ir2html --only-page`

- 更新：
  - `cpp_pdf2docx/include/pdf2docx/ir_html.hpp`
  - `cpp_pdf2docx/src/core/ir_html.cpp`
  - `cpp_pdf2docx/tools/ir2html/main.cpp`
  - `cpp_pdf2docx/tests/unit/ir_html_only_page_test.cpp`（新增）
- 行为：
  - `only_page=0` 渲染全部；
  - `only_page=N` 仅渲染第 N 页；
  - 越界参数返回 `kInvalidArgument`。

### 4) `pdf2docx` CLI 参数增强

- 更新：`cpp_pdf2docx/tools/cli/main.cpp`
- 新增参数：
  - `--dump-ir <path>`
  - `--no-images`
  - `--disable-font-fallback`
  - `--docx-anchored`
- 转换结果打印补充：
  - `pages/images/skipped_images/warnings`

### 5) 实跑验证（已执行）

- `pdf2ir` summary 验证：
  - `./build/linux-debug/pdf2ir build/test.pdf build/linux-debug/final_ir_mod09.json`
  - JSON 顶层出现 `summary.total_images=14`
- `ir2html only-page` 验证：
  - `./build/linux-debug/ir2html build/test.pdf build/linux-debug/final_ir_only_page3.html --scale 1.3 --only-page 3`
  - 输出仅包含 `Page 3`，meta 含 `only_page=3`
- `pdf2docx --no-images --dump-ir` 验证：
  - `./build/linux-debug/pdf2docx build/test.pdf build/linux-debug/final_no_images.docx --no-images --dump-ir build/linux-debug/final_no_images_ir.json`
  - `final_no_images_ir.json` 中 `total_images=0`

---

## 模块 07 持续推进记录（integration 测试 + CI workflow）

- 记录时间：`2026-04-03 07:14:14 EDT (-0400)`

### 1) integration 目录与首个用例

- 新增：
  - `cpp_pdf2docx/tests/integration/end_to_end_test.cpp`
  - `CMakeLists.txt` 接入 `integration_e2e_test`
- 覆盖：
  - `test-image-text.pdf -> docx` 端到端
  - 校验 ZIP 关键 part（`word/media/*`, `word/styles.xml`）
  - `--no-images` 等价路径（API 级）确认 `media` 数量为 0

### 2) CI workflow 草案

- 新增：`.github/workflows/ci.yml`
- 矩阵：
  - `ubuntu-latest / linux-debug`
  - `macos-latest / macos-debug`
  - `windows-latest / windows-msvc-debug`
  - `windows-latest / windows-mingw-debug`（MSYS2）

### 3) 本地验证

- `ctest --preset linux-debug` 通过（当前 `16/16`）

---

## FINAL 收尾记录（文档同步 + 抽检）

- 记录时间：`2026-04-03 07:20:00 EDT (-0400)`

### 1) 完整回归复跑

- 执行：
  - `cmake --preset linux-debug`
  - `cmake --build --preset linux-debug -j4`
  - `ctest --preset linux-debug`
- 结果：
  - 全部通过（`16/16`）。

### 2) 产物抽检（`test-image-text.pdf`）

- 执行：
  - `./build/linux-debug/pdf2ir build/test-image-text.pdf build/linux-debug/final_ir_image_text.json`
  - `./build/linux-debug/ir2html build/test-image-text.pdf build/linux-debug/final_ir_image_text.html --scale 1.3 --only-page 1`
  - `./build/linux-debug/pdf2docx build/test-image-text.pdf build/linux-debug/final_image_text.docx --dump-ir build/linux-debug/final_dump_ir.json`
- 结果：
  - 产物均生成：
    - `final_ir_image_text.json`
    - `final_ir_image_text.html`
    - `final_image_text.docx`
    - `final_dump_ir.json`
  - JSON 顶层包含 `summary.total_pages=7`、`summary.total_images=2`。
  - `pdf2docx` 输出统计：`pages=7 images=2 skipped_images=5 warnings=5`。

### 3) 文档同步（交接必需）

- 更新：
  - `prompts/module-05-docx-writer.md`
  - `prompts/module-06-api-and-cli.md`
  - `prompts/module-07-test-ci-release.md`
  - `prompts/module-08-image-pipeline.md`
  - `prompts/module-09-tools-and-debug.md`
  - `prompts/module-tasks-checklist.md`（完成状态）
  - `prompts/llm_handoff.md`（重写为 16 测试版本）
  - `cpp_pdf2docx/README.md`（CLI 与测试数量同步）

### 4) 当前结论

- roadmap 对应 checklist 条目（M01~M09 + FINAL）已落地完成并有可复现命令。

---

## M10 持续推进记录（`--dump-ir` 单次提取复用）

- 记录时间：`2026-04-03 07:34:00 EDT (-0400)`

### 1) API 扩展

- 更新：
  - `cpp_pdf2docx/include/pdf2docx/converter.hpp`
  - `cpp_pdf2docx/src/api/converter.cpp`
- 新增重载：
  - `ConvertFile(..., ConvertStats* stats, ir::Document* out_document)`
- 行为：
  - 转换链路中可直接返回已提取并经过 pipeline 的 IR；
  - 旧 4 参数接口保持兼容，内部转调新重载。

### 2) CLI 去重提取

- 更新：
  - `cpp_pdf2docx/tools/cli/main.cpp`
- 行为：
  - `--dump-ir` 改为复用 `ConvertFile` 返回 IR；
  - 删除原先“转换后再次 `ExtractIrFromFile`”的重复路径。

### 3) 测试补充

- 更新：
  - `cpp_pdf2docx/tests/unit/converter_test.cpp`
- 新增断言：
  - 返回 IR 的 `page_count` 与 `stats.page_count` 一致；
  - 返回 IR 图片总数与 `stats.image_count` 一致。

### 4) 验证

- `cmake --build --preset linux-debug -j4`：通过
- `ctest --preset linux-debug`：通过（`16/16`）
- `pdf2docx --dump-ir` 实跑：成功产出 docx + json。

---

## M11 持续推进记录（Pipeline 行内合并）

- 记录时间：`2026-04-03 07:37:00 EDT (-0400)`

### 1) Pipeline 算法增强

- 更新：
  - `cpp_pdf2docx/src/pipeline/pipeline.hpp`
  - `cpp_pdf2docx/src/pipeline/pipeline.cpp`
- 新增：
  - `PipelineStats.merged_span_count`
- 算法：
  - 排序后执行同一行近邻 span 合并；
  - 通过间距阈值、重叠容差、标点空格规则避免误合并。

### 2) 测试

- 更新：
  - `cpp_pdf2docx/tests/unit/pipeline_test.cpp`
- 新增场景：
  - `Hello` + `world` 合并为 `Hello world`；
  - 验证 `merged_span_count > 0`。

### 3) 验证

- `ctest --preset linux-debug`：通过（`16/16`）。

---

## M12 持续推进记录（anchored 几何第一阶段）

- 记录时间：`2026-04-03 07:40:00 EDT (-0400)`

### 1) quad 驱动锚定边界

- 更新：
  - `cpp_pdf2docx/src/docx/p0_writer.cpp`
- 行为：
  - 图片存在 `quad` 时，优先按四角点求边界；
  - anchored 位置偏移基于该边界计算。

### 2) 旋转属性写出

- 更新：
  - `cpp_pdf2docx/src/docx/p0_writer.cpp`
- 行为：
  - 从 `quad.p0 -> quad.p1` 估算角度；
  - 写入 `a:xfrm rot="..."`（1/60000 度单位）。

### 3) 测试增强

- 更新：
  - `cpp_pdf2docx/tests/unit/docx_anchor_test.cpp`
- 新增断言：
  - 锚定 `posOffset` 与 quad 推导值一致；
  - XML 中存在 `rot` 属性。

### 4) 验证

- `ctest --preset linux-debug`：通过（`16/16`）。
- `pdf2docx --docx-anchored --dump-ir`（`test-image-text.pdf`）实跑成功。

---

## M13 持续推进记录（阶段耗时可观测性）

- 记录时间：`2026-04-03 07:45:00 EDT (-0400)`

### 1) 统计结构扩展

- 更新：
  - `cpp_pdf2docx/include/pdf2docx/types.hpp`
- 新增字段：
  - `extract_elapsed_ms`
  - `pipeline_elapsed_ms`
  - `write_elapsed_ms`

### 2) 转换流程计时

- 更新：
  - `cpp_pdf2docx/src/api/converter.cpp`
- 行为：
  - 对 extraction/pipeline/writer 三阶段分别计时并写入 `ConvertStats`。

### 3) CLI 输出增强

- 更新：
  - `cpp_pdf2docx/tools/cli/main.cpp`
- 输出新增：
  - `extract_ms`
  - `pipeline_ms`
  - `write_ms`

### 4) 测试补充

- 更新：
  - `cpp_pdf2docx/tests/unit/converter_test.cpp`
- 新增断言：
  - 分段耗时非负；
  - 分段耗时不超过总耗时。

### 5) 验证

- `ctest --preset linux-debug`：通过（`16/16`）。
- `pdf2docx --dump-ir` 实跑输出已带三段耗时。

---

## Hotfix 记录（`final_image_text.docx` 质量问题排查与修复）

- 记录时间：`2026-04-03 08:06:00 EDT (-0400)`

### 1) 问题复现

- 用户反馈：
  - `build/final_anchored.docx` 正常；
  - `build/final_image_text.docx` 文字排版差，且部分图片无法加载。
- 排查结论：
  1. 文本问题：旧产物来自“每词独立段落 + 默认段距”阶段，版面会被拉散。
  2. 图片问题：`image-text.pdf` 抽取阶段仍有 `skipped_images=5`（`ICCBased` 相关），不是关系表丢失。

### 2) 已做修复

1. DOCX 文本段落增加紧凑段落属性（`before/after=0`，固定行距规则）。
2. 后端图片回退增强：
   - 未知滤镜下尝试原始字节魔数识别；
   - PNG 回退支持 `RGBA/RGB24/Grayscale` 多解码路径。
   - 未知滤镜场景优先尝试 `GetCopy(true)` 读取原始流字节，再回落 `GetCopySafe()`。
3. 测试 fixture 路径改为自动回退，避免 `test*.pdf` 缺失导致假失败。

### 3) 验证结果

- `cmake --preset linux-debug && cmake --build --preset linux-debug -j4 && ctest --preset linux-debug`：通过（`16/16`）。
- 重新生成：
  - `build/final_image_text.docx`
  - `build/final_anchored.docx`
  - `build/final_dump_ir.json`
- 当前 `image-text.pdf` 统计仍为：
  - `images=2 skipped_images=5 warnings=5`
  - 说明 `ICCBased` 相关图片仍待后续专项修复。

### 4) 针对 `final_dump_ir.json` line45/93 的定位结论

1. `line45` 与 `line93` 分别位于 page1/page2 的 `images` 数组，当前确实为空（`image_count=0`）。
2. 使用 `pdfimages -list build/image-text.pdf` 对照发现：
   - page1/page2 存在 `icc` 颜色空间图像对象（`enc image`）；
   - 当前后端在这些对象上触发 `Unsupported color space filter ICCBased`，最终进入 skip。
3. 因此这不是 JSON 写出错误，而是后端对该 PDF 某些 ICCBased 图像解码失败导致的“提取前缺失”。

### 5) ICCBased 专项修复（已落地）

1. 在 PoDoFo 侧增加 `ICCBased` 颜色空间 fallback（优先 `Alternate`，其次按 `N` 推断，最后回落 `DeviceRGB`）。
2. 后端抽图在未知滤镜场景继续保留：
   - 原始流魔数识别；
   - 多像素格式 decode-to-PNG（RGBA/RGB24/Gray）。
3. 对异常几何（超大负坐标）增加 page 归一化与边界裁剪，使图片可见区域落在页面内。

修复后结果（`build/image-text.pdf`）：
- `images=7`
- `skipped_images=0`
- page1/page2 的 `images` 不再为空。

### 6) 标题排版问题根因与修复

1. 根因：
   - PoDoFo 提取标题为同一行多个离散 span（如 `Auto` / `Painter:` / `From` ...）；
   - 旧 writer 把每个 span 单独写成一个 `w:p`，导致标题在 Word 中看起来“散行/乱排”。
2. 修复：
   - 在 `p0_writer.cpp` 增加“按同一行 y 聚合 span -> 单段落”逻辑；
   - 同时保持紧凑段落间距设置。
3. 结果：
   - 新版 `build/final_image_text.docx` 标题区域排版明显改善；
   - 且图片抽取问题已同步修复（见上一节）。

---

## `cpp_pdftools` 大框架规划记录（roadmap 输出）

- 记录时间：`2026-04-03 08:50:00 EDT (-0400)`

已新增全新规划文档（文件名前缀 `pdftools-`，与 `cpp_pdf2docx` 旧文档区分）：
1. `prompts/pdftools-roadmap-v1.md`
2. `prompts/pdftools-migration-plan-v1.md`
3. `prompts/pdftools-tasks-checklist-v1.md`

内容覆盖：
- `cpp_pdftools` 分层架构、接口规范、CLI/GUI 共用编排模型；
- `cpp_pdf2docx` 并入新框架的迁移策略与目录映射；
- 可执行的阶段清单与验收标准（M0~M6 + FINAL）。

## `cpp_pdftools` 模块级详细设计补充（module spec 输出）

- 记录时间：`2026-04-03 09:05:00 EDT (-0400)`

已新增：
- `prompts/pdftools-module-spec-v1.md`

本次文档聚焦“每个模块到底要做什么”，包含：
1. 模块职责拆解（M0~M6 + 横切能力）
2. 每个模块的核心类清单
3. 每个模块的接口定义与功能说明
4. 每个接口的实现思路（自然语言）
5. 模块级完成标准（DoD）与推荐落地顺序

## `cpp_pdftools` M0~M6 实施完成记录

- 记录时间：`2026-04-03 10:05:00 EDT (-0400)`

已新增：
- `prompts/pdftools-module-execution-v1.md`

本次执行结果：
1. `cpp_pdftools` 工程完成搭建并可独立构建。
2. `cpp_pdf2docx` 核心已迁入并通过新框架适配接口调用。
3. M2/M3/M4 能力完成（文档编辑、文本/附件提取、图片转PDF）。
4. M5 单入口 CLI `pdftools` 完成并接通全部主能力。
5. M6 异步任务 API（`TaskRunner`）完成并支持取消与查询。
6. 验证通过：
   - `cmake --preset linux-debug`
   - `cmake --build --preset linux-debug -j4`
   - `ctest --preset linux-debug`（`7/7 passed`）

## 联网大规模测试记录（public datasets）

- 记录时间：`2026-04-03 10:40:00 EDT (-0400)`

已新增：
- `prompts/pdftools-large-scale-test-v1.md`

本轮执行摘要：
1. 从 `mozilla/pdf.js`、`python-pillow/Pillow` 联网下载公开测试集（PDF + 图片）。
2. 对 `pdftools` 执行批量测试：
   - `text extract`
   - `convert pdf2docx`
   - `merge/page delete/page insert/page replace`
   - `image2pdf`
   - `attachments extract`
3. 关键结果：
   - 文本提取：`52/56`
   - pdf2docx：`20/20`
   - 页面编辑：全部 `15/15`
   - image2pdf：`4/5`（失败批次为损坏图片）
   - 附件提取调用：`52/52`，提取到附件文件 `1`

## 联网大规模测试（V2 继续扩容）

- 记录时间：`2026-04-03 10:50:00 EDT (-0400)`

已新增：
- `prompts/pdftools-large-scale-test-v2.md`

V2 结果摘要：
1. 样本规模：PDF `58`，图片 `25`。
2. 文本提取：`53/58`（失败 5 个异常 PDF）。
3. pdf2docx：`30/30`。
4. 页面编辑：
   - merge `20/20`
   - delete `20/20`
   - insert `20/20`
   - replace `20/20`
5. image2pdf：
   - batch `4/5`
   - single `24/25`（失败文件 `broken_data_stream.png`）
6. 附件提取：
   - 调用成功 `53/53`
   - 提取到附件文件 `1`

## 失败根因修复（文本提取 + image2pdf）

- 记录时间：`2026-04-03 11:05:00 EDT (-0400)`

已新增：
- `prompts/pdftools-failure-analysis-fix-v1.md`

本次修复内容：
1. `ExtractText` 增加 PoDoFo 多加载策略与 `pdftotext` 回退路径。
2. `ImagesToPdf` 改为“坏图跳过、整批继续”，仅在全部图片失效时失败。
3. 增加混合坏图回归测试并通过。

修复后关键结果：
1. 大规模文本提取从 `53/58` 提升到 `57/58`（仅剩 1 个非 PDF 输入失败）。
2. 图片批量转 PDF 从 `4/5` 提升到 `5/5`。
3. `ctest --preset linux-debug`：`7/7 passed`。

## 容错模式接口化（strict/best-effort）继续迭代

- 记录时间：`2026-04-03 11:15:00 EDT (-0400)`

本次继续完成：
1. `text extract` 增加 `best_effort` 请求字段与 CLI `--strict` 参数。
2. `ExtractTextResult` 增加 `used_fallback/extractor` 字段，便于调用方判断是否降级。
3. `ImagesToPdfResult` 增加 `skipped_image_count` 字段。
4. 补充测试：
   - `m3_extract_ops_test` strict 场景
   - `m4_create_ops_test` 跳过坏图统计
   - `m5_cli_test` strict 参数路径
5. 复测结果：
   - `ctest --preset linux-debug`：`7/7 passed`
   - 大规模快速复测（V4）：
     - 文本提取 `57/58`
     - 图片批量 `5/5`

## 附件提取容错模式补全 + 全链路复测

- 记录时间：`2026-04-03 11:25:00 EDT (-0400)`

已新增：
- `prompts/pdftools-large-scale-test-v3.md`

本次继续完成：
1. `ExtractAttachmentsRequest` 增加 `best_effort`（默认 true）与 CLI `attachments extract --strict`。
2. `ExtractAttachmentsResult` 增加 `parse_failed/parser` 字段。
3. 单测补强并保持全绿：`ctest --preset linux-debug -> 7/7 passed`。

全链路复测（V4）：
1. 文本提取：`57/58`
2. pdf2docx：`30/30`
3. 页面编辑（merge/delete/insert/replace）：全部 `20/20`
4. image2pdf 批量：`5/5`
5. 附件提取：`57/57`（`parse_failed=1`，实际提取文件 `2`）

## CLI 中文使用手册输出

- 记录时间：`2026-04-03 11:35:00 EDT (-0400)`

已新增：
- `prompts/pdftools-cli-guide-v1.md`

内容包括：
1. 全部子命令语法与参数定义（逐项解释）
2. 退出码说明与常见错误排查
3. 每个命令的可复现示例（含示例输出）
4. strict/best-effort 模式说明与注意事项

## Qt6 GUI 方案设计输出

- 记录时间：`2026-04-03 11:55:00 EDT (-0400)`

已新增：
- `prompts/pdftools-gui-qt6-design-v1.md`

内容覆盖：
1. GUI 功能范围（P0/P1/P2）
2. 界面信息架构与每个页面字段级设计
3. 如何以 In-process 方式调用 `pdftools` 库
4. Qt6 类设计、任务线程模型、strict/best-effort 策略映射
5. 测试策略与分阶段落地计划

## Qt6 GUI 实施完成（P0）

- 记录时间：`2026-04-03 13:05:00 EDT (-0400)`

已新增：
- `prompts/pdftools-gui-qt6-implementation-v1.md`

本次完成：
1. 新建 `cpp_pdftools_gui`，实现 Qt6 GUI 全架构（主窗口 + 任务中心 + 6 页面）。
2. 进程内接入 `pdftools::core`（不走 CLI 子进程）。
3. 新增服务层/任务层/模型层/通用控件层，支持后台异步执行。
4. 为支持子工程复用，修正 `cpp_pdftools` CMake 路径变量（`CMAKE_CURRENT_SOURCE_DIR/BINARY_DIR`）。
5. 新增 GUI 测试并通过：`2/2`。
6. 回归 `cpp_pdftools` 独立构建测试：`7/7` 通过。

验证命令：
1. `cd cpp_pdftools_gui && cmake --preset linux-debug && cmake --build --preset linux-debug -j8 && ctest --preset linux-debug`
2. `cd cpp_pdftools && cmake --preset linux-debug && cmake --build --preset linux-debug -j8 && ctest --preset linux-debug`

## Qt6 GUI 继续自检修复（错误自修 + 测试扩展）

- 记录时间：`2026-04-03 13:30:00 EDT (-0400)`

本轮继续完成：
1. 扩展 GUI 自动化测试覆盖范围：
   - `pdftools_service_test` 覆盖 `ExtractText / Merge / DeletePage / InsertPage / ReplacePage / ExtractAttachments / ImagesToPdf / PdfToDocx`。
   - 新增 `task_manager_test`，验证异步提交与完成信号。
2. 补充 GUI CMake 测试 fixture：
   - 增加 `PDFTOOLS_GUI_TEST_ALT_PDF_PATH`。
   - 增加 `PDFTOOLS_GUI_TEST_IMAGE_PATH`（用于 `image2pdf` 用例）。
3. 持续修正编译与集成细节：
   - 补全 `TaskCenterWidget` 对 `std::optional` 头文件依赖。

验证结果：
1. `cpp_pdftools_gui` Debug：`ctest --preset linux-debug` -> `3/3 passed`。
2. `cpp_pdftools_gui` Release：`ctest --preset linux-release` -> `3/3 passed`。
3. `cpp_pdftools` 回归：仍保持 `7/7 passed`。

## Qt6 GUI 稳定性修复（TaskManager 内存增长风险）

- 记录时间：`2026-04-03 13:45:00 EDT (-0400)`

本次继续修复：
1. `TaskManager` 增加已完成任务保留上限（默认 `200`），完成任务会按 FIFO 清理旧记录，避免长期运行任务记录无限增长。
2. `TaskManager` 构造时若传入空 `service`，自动创建默认 `PdfToolsService` 兜底，防止空指针调用。
3. 新增/增强测试：
   - `task_manager_test` 增加空 service 场景覆盖；
   - `service_test` 维持全链路操作覆盖。

验证结果：
1. `cpp_pdftools_gui` Debug：`3/3 passed`。
2. `cpp_pdftools_gui` Release：`3/3 passed`。

## Qt6 GUI 继续迭代（最近参数复用 + 设置层补测）

- 记录时间：`2026-04-03 14:05:00 EDT (-0400)`

本轮继续完成：
1. `PathPicker` 新增 history completer（suggestions）能力，支持最近路径快速复用。
2. 6 个业务页面接入最近参数恢复：
   - 启动时回填最近 input/output 与关键参数；
   - 提交时持久化当前参数（strict/overwrite/scale/tab 等）。
3. `SettingsService` 新增 `RecentValue()` 快捷读取接口。
4. `TaskCenterWidget` 增加 sourceModel 判空保护，避免异常时序下空指针。
5. 新增测试：`settings_service_test`，验证 recent 去重/上限逻辑。

验证结果：
1. `cpp_pdftools_gui` Debug：`4/4 passed`。
2. `cpp_pdftools_gui` Release：`4/4 passed`。

## Qt6 GUI 继续迭代（设置页 + 任务保留策略可配置）

- 记录时间：`2026-04-03 14:25:00 EDT (-0400)`

本轮继续完成：
1. 新增 `SettingsPage`，接入主导航“设置”页面。
2. 设置页支持：
   - 动态配置 `TaskManager` 完成任务保留上限；
   - 清空 recent 历史；
   - 重置窗口布局状态。
3. `TaskManager` 新增接口：
   - `SetMaxCompletedTasks` / `MaxCompletedTasks` / `RetainedTaskCount`。
4. `SettingsService` 新增：
   - `ClearByPrefix` / `AllKeys` / `RemoveKey`。
5. 扩充测试：
   - `task_manager_test` 增加保留上限约束场景；
   - `settings_service_test` 增加前缀清理场景。

验证结果：
1. `cpp_pdftools_gui` Debug：`4/4 passed`。
2. `cpp_pdftools_gui` Release：`4/4 passed`。

## Qt6 GUI 继续迭代（任务中心详情面板）

- 记录时间：`2026-04-03 14:40:00 EDT (-0400)`

本轮继续完成：
1. `TaskCenterWidget` 新增详情面板，展示当前任务的 `summary/detail/output path`。
2. 列表选择变化时自动刷新详情；任务状态更新（dataChanged）时同步刷新详情。
3. 任务中心交互排障效率提升，失败任务可在 GUI 内直接查看细节。

验证结果：
1. `cpp_pdftools_gui` Debug：`4/4 passed`。
2. `cpp_pdftools_gui` Release：`4/4 passed`。

## Qt6 GUI 继续迭代（UI 交互自动化测试扩展）

- 记录时间：`2026-04-03 14:55:00 EDT (-0400)`

本轮继续完成：
1. 新增 `task_center_widget_test`，验证任务详情面板随选择更新。
2. 新增 `settings_page_test`，验证设置页参数应用与 recent 清理。
3. CTest 统一注入 `QT_QPA_PLATFORM=offscreen`，确保无头 CI 稳定。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。

## Qt6 GUI 继续迭代（Open Output 回退增强）

- 记录时间：`2026-04-03 15:05:00 EDT (-0400)`

本轮继续完成：
1. 任务中心 `Open Output` 行为增强：
   - 若输出目标不存在但父目录存在，自动回退打开父目录；
   - 降低失败任务时“无法打开路径”的阻塞感。
2. 修复该改动引入的编译问题（补充 `QDir` 头文件）。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。

## Qt6 GUI 继续迭代（任务并发队列 + 错误报告导出）

- 记录时间：`2026-04-04 04:32:18 EDT (-0400)`

本轮继续完成：
1. `TaskManager` 并发队列能力补齐：
   - 提交任务初始状态为 `Queued`；
   - 由 `QFutureWatcher::started` 切换为 `Running`；
   - 执行改为 `QtConcurrent::run(&thread_pool_, ...)`，支持线程池并发上限控制。
2. 并发设置接入：
   - `TaskManager` 增加 `SetMaxConcurrentTasks/MaxConcurrentTasks`；
   - 设置页新增“最大并发任务数”，并持久化 `settings/max_concurrent_tasks`；
   - 主窗口启动时读取并发配置。
3. 任务中心新增“错误报告导出”：
   - 增加 `Export Report` 按钮；
   - 支持导出选中任务的 `TaskId/Operation/Status/Summary/Detail/OutputPath` 等关键信息；
   - 默认输出到应用数据目录下 `task_reports`，可配置导出目录；
   - 增加 `LastExportedReportPath` 用于测试和后续扩展。
4. 新增/增强测试：
   - `task_manager_test`：补充 queued 状态与并发上限配置测试；
   - `settings_page_test`：补充并发上限持久化验证；
   - `task_center_widget_test`：新增报告导出落盘与内容断言。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（任务历史持久化）

- 记录时间：`2026-04-04 04:44:18 EDT (-0400)`

本轮继续完成：
1. `SettingsService` 增加任务历史序列化接口：
   - `WriteTaskHistory(tasks, max_items)`；
   - `ReadTaskHistory(max_items)`；
   - `ClearTaskHistory()`。
2. `TaskItemModel` 增加：
   - `SnapshotTasks()`；
   - `ReplaceAllTasks(...)`；
   支持主窗口在启动/退出时做任务历史恢复与保存。
3. `MainWindow` 生命周期接入历史持久化：
   - 启动后调用 `RestoreTaskHistory()`；
   - 关闭时调用 `SaveTaskHistory()`；
   - 仅持久化最终态任务（成功/失败），避免重启后出现“卡在运行中”的历史假象。
4. 增强测试：
   - `settings_service_test`：覆盖任务历史 round-trip、上限裁剪、清理逻辑；
   - `task_item_model_test`：覆盖 snapshot + replace 顺序一致性。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（历史管理闭环）

- 记录时间：`2026-04-04 04:49:09 EDT (-0400)`

本轮继续完成：
1. `SettingsPage` 新增“清空任务历史记录”按钮（`clear_task_history_button`）。
2. 设置页功能从“清空 recent”扩展为：
   - 清空最近参数/文件历史；
   - 清空持久化任务历史；
   - 重置窗口布局状态。
3. 与任务历史持久化能力组合后形成闭环：
   - 启动自动恢复；
   - 退出自动保存；
   - 用户可随时手动清空。
4. 扩展 `settings_page_test`：
   - 验证清空任务历史后 `ReadTaskHistory(...)` 返回空集。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（任务中心队列状态过滤）

- 记录时间：`2026-04-04 04:53:29 EDT (-0400)`

本轮继续完成：
1. 任务中心过滤器新增 `Queued` 状态项。
2. 过滤器项顺序统一为：
   - `All` / `Queued` / `Running` / `Succeeded` / `Failed`。
3. 增强 `task_center_widget_test`：
   - 验证切换到 `Queued` 过滤后仅显示排队任务。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（排队任务取消）

- 记录时间：`2026-04-04 05:30:08 EDT (-0400)`

本轮继续完成：
1. `TaskManager` 新增 `CancelTask(task_id)`：
   - 仅允许取消 `Queued` 任务；
   - 取消后立即切换任务状态为 `Canceled`；
   - 发出 `TaskChanged/TaskCompleted` 事件并进入完成队列。
2. `TaskState` 新增 `Canceled` 终态，任务中心可展示可过滤。
3. 任务中心新增 `Cancel Task` 按钮：
   - 选中任务后触发取消；
   - 对非队列任务给出“仅支持取消排队任务”提示。
4. `task_manager_test` 增加排队取消测试，验证终态与信号行为。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（任务历史即时清理）

- 记录时间：`2026-04-04 05:38:44 EDT (-0400)`

本轮继续完成：
1. `TaskManager` 新增 `ClearRetainedTasks()`，可清理内存中的终态任务记录。
2. `TaskItemModel` 新增 `RemoveFinalTasks()`，支持任务中心即时移除终态记录。
3. `SettingsPage` 清空任务历史升级为“持久化 + 内存 + UI”三层联动：
   - 清空 `QSettings` 中历史；
   - 调用 `TaskManager::ClearRetainedTasks()`；
   - 发射 `TaskHistoryCleared` 信号。
4. `MainWindow` 监听 `TaskHistoryCleared`，触发任务中心即时刷新，无需重启。
5. 新增/增强测试：
   - `task_item_model_test`：终态任务清理后仅保留活跃任务；
   - `task_manager_test`：`ClearRetainedTasks` 清理行为；
   - `settings_page_test`：`TaskHistoryCleared` 信号触发。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（任务中心按钮状态联动）

- 记录时间：`2026-04-04 05:44:30 EDT (-0400)`

本轮继续完成：
1. 任务中心新增动作状态联动策略：
   - 无选中任务时 `Cancel/Export/Open` 全禁用；
   - 有选中任务时 `Export/Open` 启用；
   - 仅 `Queued` 任务启用 `Cancel`。
2. 在任务选择变化与模型状态变化（`dataChanged`）时自动刷新动作状态。
3. 增强 `task_center_widget_test`：
   - 验证 failed 任务不可取消；
   - 验证 queued 任务可取消；
   - 验证无选择时全部动作禁用。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（Canceled 过滤回归覆盖）

- 记录时间：`2026-04-04 05:47:51 EDT (-0400)`

本轮继续完成：
1. `task_center_widget_test` 新增 `Canceled` 状态过滤用例。
2. 覆盖路径：
   - 写入 `Canceled + Running` 混合任务；
   - 切换过滤到 `Canceled`；
   - 断言仅展示取消任务。
3. 进一步巩固“排队取消 -> Canceled 可筛选”完整链路。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（报告目录设置化）

- 记录时间：`2026-04-04 05:53:50 EDT (-0400)`

本轮继续完成：
1. 设置页新增“任务报告目录”输入控件（目录模式 `PathPicker`）。
2. 应用设置时新增：
   - 持久化 `settings/report_directory`；
   - 发出 `ReportDirectoryChanged(path)` 信号。
3. 主窗口接入：
   - 启动时读取报告目录配置并应用到任务中心；
   - 监听设置页信号，运行时热更新任务中心报告导出目录。
4. 顺带修复：
   - 主窗口任务历史保存的“终态判定”补入 `Canceled`，避免取消任务漏持久化。
5. 测试补强：
   - `settings_page_test` 增加报告目录持久化与信号断言。

验证结果：
1. `cpp_pdftools_gui` Debug：`6/6 passed`。
2. `cpp_pdftools_gui` Release：`6/6 passed`。
3. `cpp_pdftools` 回归：`7/7 passed`。

## Qt6 GUI 继续迭代（页面编辑页数预览）

- 记录时间：`2026-04-04 06:31:55 EDT (-0400)`

本轮继续完成：
1. `cpp_pdftools` 增加 `GetPdfInfo(request, result)`：
   - 输入：`input_pdf`；
   - 输出：`page_count`；
   - 失败场景返回可区分状态（例如不存在文件返回 `kNotFound`）。
2. `PageEditPage` 增加页数信息可视化：
   - 输入 PDF 信息标签；
   - 插入来源 PDF 信息标签；
   - 替换来源 PDF 信息标签。
3. 交互行为：
   - 路径变化时自动刷新信息；
   - 成功显示 `页数：N`；
   - 失败显示 `无法读取：...`。
4. 新增 GUI 测试 `page_edit_page_test`，覆盖有效/无效路径与来源 PDF 信息刷新。
5. 扩展 `m2_document_ops_test` 覆盖 `GetPdfInfo` 成功与 NotFound 失败分支。

验证结果：
1. `cpp_pdftools` Debug：`7/7 passed`。
2. `cpp_pdftools_gui` Debug：`7/7 passed`。
3. `cpp_pdftools_gui` Release：`7/7 passed`。

## Qt6 GUI 继续迭代（PDF Info CLI 接入）

- 记录时间：`2026-04-04 06:35:38 EDT (-0400)`

本轮继续完成：
1. `cpp_pdftools` CLI 新增：
   - `pdftools pdf info --input <in.pdf>`。
2. 命令输出新增：
   - `pdf info pages=<N>`。
3. `m5_cli_test` 增加 `pdf info` 调用与输出断言，确保命令可用。

验证结果：
1. `cpp_pdftools` Debug：`7/7 passed`。
2. `cpp_pdftools_gui` Debug：`7/7 passed`。
3. `cpp_pdftools_gui` Release：`7/7 passed`。

## Qt6 GUI 继续迭代（页号输入范围联动）

- 记录时间：`2026-04-04 06:41:00 EDT (-0400)`

本轮继续完成：
1. 页面编辑页新增“页号范围联动”：
   - 目标 PDF 解析成功后，动态限制删除/替换页号和插入位置上限；
   - 来源 PDF 解析成功后，动态限制来源页号上限。
2. 无效路径或未选择文件时，自动回退默认上限，保持可编辑。
3. `page_edit_page_test` 增加范围断言：
   - `delete/replace <= page_count`；
   - `insert_at <= page_count + 1`；
   - `source_page <= source_page_count`。

验证结果：
1. `cpp_pdftools` Debug：`7/7 passed`。
2. `cpp_pdftools_gui` Debug：`7/7 passed`。
3. `cpp_pdftools_gui` Release：`7/7 passed`。

## Qt6 GUI 继续迭代（合并页总页数预估）

- 记录时间：`2026-04-04 06:47:50 EDT (-0400)`

本轮继续完成：
1. `MergePage` 增加输入摘要标签，实时展示：
   - 当前选择文件数；
   - 预计总页数；
   - 无法读取文件计数（若存在）。
2. 摘要刷新触发：
   - 初始加载最近输入后；
   - 文件列表变更（增删/排序/清空）后。
3. 新增 `merge_page_test`：
   - 验证可读 PDF 的总页数聚合；
   - 验证混入无效路径时“无法读取 N 个”提示。

验证结果：
1. `cpp_pdftools` Debug：`7/7 passed`。
2. `cpp_pdftools_gui` Debug：`8/8 passed`。
3. `cpp_pdftools_gui` Release：`8/8 passed`。

## Qt6 GUI 继续迭代（任务中心关键字搜索）

- 记录时间：`2026-04-04 06:53:34 EDT (-0400)`

本轮继续完成：
1. 任务中心新增关键字搜索框（匹配 `summary + detail`，大小写不敏感）。
2. 支持和状态过滤联动叠加（例如 `Failed + timeout`）。
3. 修复过滤边界：`All` 状态下关键字过滤也可生效。
4. 增强 `task_center_widget_test`：
   - 关键字匹配 `summary`；
   - 关键字匹配 `detail`；
   - 清空关键字恢复全量。

验证结果：
1. `cpp_pdftools` Debug：`7/7 passed`。
2. `cpp_pdftools_gui` Debug：`8/8 passed`。
3. `cpp_pdftools_gui` Release：`8/8 passed`。

## Qt6 GUI 继续迭代（预览能力补齐 + 当前 PDF 预览）

- 记录时间：`2026-04-04 07:39:36 EDT (-0400)`

本轮继续完成：
1. 新增统一预览服务 `PreviewService`（`cpp_pdftools_gui/services`）：
   - `QueryPdfPageCount`：读取 PDF 页数；
   - `RenderPdfIrPreview`：PDF -> IR -> HTML 预览；
   - `BuildImageThumbnails`：本地图像缩略图；
   - `ExtractImageThumbnailsFromPdf`：PDF 图片块缩略图提取。
2. `TextExtractPage` 接入“当前 PDF + 指定页预览”：
   - 路径变化后自动更新页码范围；
   - 支持刷新指定页 IR 预览。
3. `PageEditPage` 接入“删除/插入/替换来源页预览”：
   - 按当前 Tab 自动选用预览源和页码。
4. `ImageToPdfPage` 接入“图片转 PDF 前缩略图预览”：
   - 展示可读图片数；
   - 展示不可读图片计数。
5. `Pdf2DocxPage` 接入“IR 预览 + 提图预览”：
   - 支持按页预览 IR；
   - 支持提取图片块并缩略图展示。
6. `MergePage` 新增“当前 PDF 预览”：
   - 新增预览文件下拉、页码输入、刷新按钮、预览浏览器；
   - 输入列表变化时自动刷新预览候选文件与页码范围。
7. 修复/规避点：
   - 预览相关类在页面头文件中显式包含，避免 `unique_ptr` + 前置声明在 MOC 场景下的完整类型编译问题。

新增/增强测试：
1. `preview_service_test`：覆盖页数查询、IR HTML 预览、PDF 提图缩略图。
2. `text_extract_page_test`：覆盖页数信息与指定页预览。
3. `pdf2docx_page_test`：覆盖 IR 预览和提图预览联动。
4. `image_to_pdf_page_test`：覆盖可读/不可读图片统计。
5. `merge_page_test`：新增当前 PDF 预览刷新断言。

验证结果：
1. `cpp_pdftools_gui` Debug：`12/12 passed`。
2. `cpp_pdftools_gui` Release：`12/12 passed`。
3. `cpp_pdftools` Debug 回归：`7/7 passed`。

## 手工测试清单输出（CLI + GUI 全工具）

- 记录时间：`2026-04-04 07:46:10 EDT (-0400)`

本轮完成：
1. 新增手工测试待办文档：
   - `prompts/pdftools-cli-gui-manual-test-todolist-v1.md`
2. 覆盖范围：
   - 全部 CLI 命令（`merge/text/attachments/pdf info/image2pdf/page delete|insert|replace/convert pdf2docx`）；
   - 全部 GUI 页面（6 业务页 + 设置页 + 任务中心）；
   - 各预览能力（当前 PDF 预览、IR 预览、提图预览、图片缩略图预览）。
3. 清单内容包含：
   - 前置准备；
   - 每个 tool 的可执行步骤；
   - 成功/失败路径；
   - 产物校验；
   - 统一完成判定与缺陷记录模板。

## PDF->DOCX 排版正确性专项测试清单

- 记录时间：`2026-04-04 07:50:07 EDT (-0400)`

本轮完成：
1. 输出 PDF->DOCX 排版专项测试文档：
   - `prompts/pdftools-pdf2docx-layout-focus-test-todolist-v1.md`
2. 文档重点：
   - 明确 P0/P1 测试工具优先级（`pdftools convert pdf2docx`、`pdftools_gui`、`ir2html`）；
   - 为每个工具给出具体排版检查步骤；
   - 定义高优先级排版缺陷判定标准（title 错位、图片缺失、图文覆盖、对象顺序错乱）；
   - 给出最短执行路径和缺陷归因模板。

## PDF->DOCX 数学公式转换能力实现

- 记录时间：`2026-04-04 08:31:07 EDT (-0400)`

本轮完成：
1. 新增“数学公式写出”能力（OMML）：
   - 在 `cpp_pdf2docx/src/docx/p0_writer.cpp` 实现公式识别、表达式解析与 OMML 生成；
   - 支持 `m:sSup/m:sSub/m:sSubSup/m:f`；
   - 文档根节点新增 `xmlns:m` 数学命名空间。
2. 为保证 `pdftools` CLI/GUI 生效，同步迁移相同实现到：
   - `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`。
3. 误判抑制（重点）：
   - 增加公式判定语义约束（不再仅依赖几何脚本）；
   - 增加“写入前校验”，仅对有效结构化公式写 `m:oMath`，否则回退普通文本；
   - 优化行聚类中的脚本附着条件，减少跨行误并。
4. 新增测试：
   - `cpp_pdf2docx/tests/unit/docx_math_test.cpp`；
   - `cpp_pdf2docx/CMakeLists.txt` 增加 `docx_math_test` 目标。
5. 新模块文档：
   - `prompts/module-10-math-formula-docx.md`

验证结果：
1. `cpp_pdf2docx` Debug：`17/17 passed`。
2. `cpp_pdf2docx` Release：`17/17 passed`（`ctest --test-dir build/linux-release`）。
3. `cpp_pdftools` Debug：`7/7 passed`。
4. `cpp_pdftools_gui` Debug：`12/12 passed`。
5. 实样 `image-text.pdf` 抽检：
   - `m:oMathPara` 从误判前的 20+ 收敛到 `3`；
   - 保留为表达式类内容（如 `c=N`、`EvaluationScore=100`、`s2+s3+s4+s5`）。

## `formula.pdf` 公式样本测试

- 记录时间：`2026-04-04 08:38:40 EDT (-0400)`

本轮完成：
1. 对样本 `cpp_pdftools/build/formula.pdf` 执行专项测试（`pdftools` + `cpp_pdf2docx` 双链路）。
2. 测试命令与结果明细已整理到：
   - `prompts/formula-pdf-test-report-v1.md`

关键结果：
1. 转换链路成功，页数 `1`，无图片、无警告。
2. IR 统计：`total_spans=60`，包含积分/极限/矩阵等公式相关 token。
3. DOCX OMML 命中：`m:oMathPara=1`（识别 `a+b=c`），其余复杂公式仍为普通文本。

当前结论：
1. 线性公式能力已生效；
2. `formula.pdf` 暴露出复杂二维公式（积分、极限、矩阵）结构化重建不足；
3. 该文件已纳入后续公式模块主回归样本。

## `formula.pdf` 公式识别优化（V2）

- 记录时间：`2026-04-04 09:23:00 EDT (-0400)`

本轮完成：
1. 在 `cpp_pdf2docx/src/docx/p0_writer.cpp` 完成公式识别优化：
   - 行聚类新增包络距离与脚本附着规则；
   - 修复 `bbox.height=0` 小符号的脚本判定；
   - `MathExpressionParser` 增加尾部 token 兜底，避免集合公式被截断；
   - 修复 trig 关键词误判（`sin` 不再命中普通词 `using`）。
2. 强化单测：
   - 更新 `cpp_pdf2docx/tests/unit/docx_math_test.cpp`；
   - 新增集合表达式与误判回归断言。
3. 同步迁移到 `pdftools`：
   - `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`。
4. 新增交接文档：
   - `prompts/module-11-formula-optimization-v2.md`
   - `prompts/formula-pdf-test-report-v2.md`

验证结果：
1. `cpp_pdf2docx`：`17/17 passed`。
2. `cpp_pdftools`：`7/7 passed`。
3. `cpp_pdftools_gui`：`12/12 passed`。
4. `formula.pdf`：`m:oMathPara` 从 V1 的 `1` 提升到 V2 的 `8`。

## `formula.pdf` 公式识别优化（V3）

- 记录时间：`2026-04-04 09:40:30 EDT (-0400)`

本轮完成：
1. 在 `cpp_pdf2docx/src/docx/p0_writer.cpp` 实现 V3 优化：
   - 复合数学 span 拆分（大 span token 化）以改进独立上标回挂；
   - 脚本判定排除单括号 token；
   - 多行公式续行合并（`=` 开头等继续行并入上一公式）。
2. 增强单测：
   - `cpp_pdf2docx/tests/unit/docx_math_test.cpp` 增加续行公式与“`a+b=c` + 独立上标”场景。
3. 同步迁移到 `pdftools` legacy writer：
   - `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`。
4. 新增交接文档：
   - `prompts/module-12-formula-optimization-v3.md`
   - `prompts/formula-pdf-test-report-v3.md`

验证结果：
1. `cpp_pdf2docx`: `17/17 passed`
2. `cpp_pdftools`: `7/7 passed`
3. `cpp_pdftools_gui`: `12/12 passed`
4. `formula.pdf`：
   - `m:oMathPara=7`（V2 为 8，因续行合并）；
   - 关键改进：`a+b=c222` -> `a2+b2=c2`；
   - 多行等式已合并为单条数学段。

## `formula.pdf` 公式识别优化（V4）

- 记录时间：`2026-04-04 09:49:20 EDT (-0400)`

本轮完成：
1. 在 `cpp_pdf2docx/src/docx/p0_writer.cpp` 增加线性表达式归一化：
   - 将 `= _a ^b` / `= ^b _a` 归一化为 `= b/a`；
   - 接入 `AppendMathParagraph` 解析前流程。
2. 同步到 `pdftools` legacy writer：
   - `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`。
3. 新增交接文档：
   - `prompts/module-13-formula-optimization-v4.md`
   - `prompts/formula-pdf-test-report-v4.md`

验证结果：
1. `cpp_pdf2docx`: `17/17 passed`
2. `cpp_pdftools`: `7/7 passed`
3. `cpp_pdftools_gui`: `12/12 passed`
4. `formula.pdf`：
   - `m:oMathPara=7`（与 V3 一致）
   - `m:f=1`（V3 为 0，新增分式结构命中）

## 数学公式阶段性交接（暂停点）

- 记录时间：`2026-04-04 10:18:00 EDT (-0400)`
- 用户指令：先暂停数学公式继续开发，把当前成果输出到 `prompts` 便于后续 LLM 交接。

本次已输出文档：
1. `prompts/module-14-formula-handoff-v5.md`
2. `prompts/formula-pdf-test-report-v5.md`

当前代码状态（暂停前）：
1. 公式行聚类、复合 span 拆分、续行合并、线性归一化、隐式乘法解析均已落地。
2. `formula.pdf` 最新结果：`m:oMathPara=7`, `m:f=1`, `m:sSup=9`, `m:sSub=4`, `m:sSubSup=1`。
3. 全部测试通过：
   - `cpp_pdf2docx`: `17/17`
   - `cpp_pdftools`: `7/7`
   - `cpp_pdftools_gui`: `12/12`

## `image-text.pdf` 标题排版修复

- 记录时间：`2026-04-04 12:47:00 EDT (-0400)`

本轮完成：
1. 在 `cpp_pdf2docx/src/docx/p0_writer.cpp` 增加标题识别与标题段落写出：
   - 首页顶部标题行识别；
   - 双行标题合并为单段并插入 `w:br`；
   - 标题 run 增加加粗与字号（`w:sz=36`）。
2. 同步迁移至 `pdftools` legacy writer：
   - `cpp_pdftools/src/legacy_pdf2docx/docx/p0_writer.cpp`。
3. 回归生成：
   - `cpp_pdf2docx/build/final_image_text.docx`（已重新生成）
   - `/tmp/image_text_title_fix.docx`
4. 验证结果：
   - `cpp_pdf2docx`: `17/17 passed`
   - `cpp_pdftools`: `7/7 passed`
   - `cpp_pdftools_gui`: `12/12 passed`

## pdftools 总进度汇总输出

- 记录时间：`2026-04-04 12:53:00 EDT (-0400)`
- 用户请求：输出当前 `pdftools` 总进度到 console 和 prompts，并回答测试是否完全正确。

已完成：
1. 新增总进度文档：`prompts/pdftools-progress-summary-v2.md`
2. 文档已包含：
   - 已完成模块范围（core/CLI/GUI/pdf2docx）
   - 最近测试结果（17/17, 7/7, 12/12）
   - 人工检查文件路径
   - 对“是否完全正确”的明确结论（否）与原因。

---

## PTools Native/WASM 异常统一记录（Module 16）

- 记录时间：`2026-04-04`
- 详细设计与实现说明：`prompts/module-16-native-wasm-exception-policy-v1.md`
- 关键结果：
  - 新增统一异常桥接模块（`GuardStatus/CurrentExceptionToStatus`）。
  - `cpp_pdftools` 核心模块、CLI、GUI 服务层异常处理已统一并加 context。
  - `web_pdftools/wasm` worker 协议错误模型统一为 `code/message/context/details`。
  - 新增 C++/JS 测试并全部通过。
