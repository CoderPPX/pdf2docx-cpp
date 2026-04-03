# pdf2docx-cpp P1 Framework

这是一个 **P1 框架版本**，目标是在跨平台骨架基础上接入 thirdparty 依赖并跑通最小端到端流程。

## 当前状态

- 已有：
  - 跨平台 CMake 工程与 Presets
  - thirdparty 依赖导入（`find_package + add_subdirectory`）
  - `Converter` / `Status` / `Options` 基础 API
  - 最小 `PoDoFo -> IR -> DOCX` 链路（含 JPEG/JP2/Flate->PNG 图片抽取与写出）
  - `backend/podofo`、`pipeline`、`docx`、`ir_html` 模块最小实现
  - CLI：`pdf2docx`、`pdf2ir`、`ir2html`
  - 16 个单元/集成用例（`ctest`）
- 未有（后续里程碑实现）：
  - 高保真 OOXML 样式/分页/图片绝对定位写出
  - 非 JPEG/JP2 图片编码的完整支持
  - 表格结构化转换

## 依赖约束

- 依赖源码放在上级目录：`../thirdparty`
- 默认检测以下库是否存在 CMakeLists：
  - `zlib`
  - `tinyxml2`
  - `minizip-ng`
  - `freetype2`
  - `podofo`
- 预设默认启用：`PDF2DOCX_ADD_SUBDIR_DEPS=ON`

## 快速开始

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

CLI 示例：

```bash
./build/linux-debug/pdf2docx input.pdf output.docx
./build/linux-debug/pdf2docx input.pdf output.docx --dump-ir output_ir.json
./build/linux-debug/pdf2docx input.pdf output.docx --no-images --disable-font-fallback
./build/linux-debug/pdf2docx input.pdf output.docx --docx-anchored
./build/linux-debug/pdf2ir input.pdf output_ir.json
./build/linux-debug/ir2html input.pdf output.html
./build/linux-debug/ir2html input.pdf output.html --scale 1.6 --hide-boxes --only-page 3
```

> 当前行为：若 `tinyxml2 + minizip-ng` 可用，输出最小合法 DOCX（文本 + 图片媒体）；否则自动降级为占位文本输出。
> `ir2html` 当前为“绝对定位预览”，可显示文本块和图片块。
