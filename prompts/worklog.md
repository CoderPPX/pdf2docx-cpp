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
