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
