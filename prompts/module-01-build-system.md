# 模块 01：构建系统与第三方依赖（CMake-first，MSVC/MinGW 友好）

## 1) 模块定位

本模块负责“可构建性”和“可移植性”：
- 统一 CMake 入口与配置参数；
- 封装 thirdparty 依赖导入（不依赖 vcpkg）；
- 保证 Linux/macOS/Windows（MSVC + MinGW）构建路径一致；
- 为其他模块提供稳定的编译宏和链接目标。

当前工程实际入口：
- `cpp_pdf2docx/CMakeLists.txt`
- `cpp_pdf2docx/cmake/Dependencies.cmake`
- `cpp_pdf2docx/CMakePresets.json`

---

## 2) 当前实现状态（已落地）

## 2.1 已实现能力
1. CMake 版本与标准：
   - `cmake_minimum_required(VERSION 3.24)`
   - `CMAKE_CXX_STANDARD 20`
2. 依赖导入路径：
   - 默认 `PDF2DOCX_THIRDPARTY_ROOT=${CMAKE_SOURCE_DIR}/../thirdparty`
3. 依赖导入策略：
   - `find_package + add_subdirectory` 混合
4. 目标结构：
   - 核心库：`pdf2docx_core`
   - CLI：`pdf2docx`、`ir2html`、`pdf2ir`
   - CTest 单元/集成测试：16 项
5. 安装导出：
   - `install(TARGETS ...)`
   - `install(EXPORT pdf2docxTargets ...)`

## 2.2 当前默认选项（关键）
```cmake
PDF2DOCX_USE_BUNDLED_DEPS=ON
PDF2DOCX_ADD_SUBDIR_DEPS=ON
PDF2DOCX_BUILD_SHARED=OFF
PDF2DOCX_ENABLE_TESTS=ON
PDF2DOCX_ENABLE_CLI=ON
PDF2DOCX_XML_BACKEND=tinyxml2
PDF2DOCX_PDF_BACKEND=podofo
```

说明：`PDF2DOCX_BUILD_SHARED` 默认为 `OFF`，用于减少静态 thirdparty 与 PIC 相关冲突。

---

## 3) 依赖矩阵与约束

## 3.1 目标依赖（当前）
- `zlib`
- `tinyxml2`
- `minizip-ng`
- `freetype2`
- `podofo`

## 3.2 约束
- 依赖必须支持 CMake。
- 避免 automake/toolchain 复杂链路。
- 避免 vcpkg，优先仓库内 thirdparty。

## 3.3 编译期开关（由 CMake 注入）
```cpp
PDF2DOCX_HAS_ZLIB
PDF2DOCX_HAS_TINYXML2
PDF2DOCX_HAS_MINIZIP
PDF2DOCX_HAS_FREETYPE
PDF2DOCX_HAS_PODOFO
```

这些宏用于实现文件中的功能降级和条件编译。

---

## 4) Presets 设计（跨平台入口）

当前 presets（`CMakePresets.json`）：
- Linux：`linux-debug` / `linux-release`
- macOS：`macos-debug` / `macos-release`
- Windows MSVC：`windows-msvc-debug` / `windows-msvc-release`
- Windows MinGW：`windows-mingw-debug` / `windows-mingw-release`

推荐交接后统一使用：
```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

---

## 5) 模块接口与对外契约

虽然本模块不暴露 C++ API，但暴露以下“构建契约”：
1. 任何新增源文件必须挂到 `pdf2docx_core` 或明确新 target。
2. 任何新增 CLI 必须受 `PDF2DOCX_ENABLE_CLI` 控制。
3. 任何新增测试必须受 `PDF2DOCX_ENABLE_TESTS` 控制并进入 CTest。
4. 新增 thirdparty 必须先在 `Dependencies.cmake` 统一导入，不允许在子模块私自 `find_package`。

---

## 6) 常见问题与排查

1. **PoDoFo add_subdirectory 后 include 异常**
   - 现有 `CMakeLists.txt` 已包含对 `_deps/podofo/.../auxiliary` 的兼容 include 逻辑。
2. **Windows 下 MinGW 找不到编译器**
   - 需确保 `gcc/g++` 在 PATH；preset 已显式设置 `CMAKE_C_COMPILER/CMAKE_CXX_COMPILER`。
3. **静态/共享链接冲突**
   - 优先保持 `PDF2DOCX_BUILD_SHARED=OFF`。

---

## 7) 下一步详细设计（构建模块）

## P1
1. 给 `Dependencies.cmake` 增加每个依赖版本检测日志（版本号 + 来源路径）。
2. 新增 `PDF2DOCX_STRICT_DEPS=ON`：
   - 依赖缺失时直接 fail-fast（当前可部分降级）。
3. 为 Windows 增加 `testPresets`（MSVC Debug + MinGW Debug）。

## P2
1. 拆分核心库为多个内部 target（backend/docx/core 等），优化增量编译。
2. 产出 `pdf2docxConfig.cmake` + `pdf2docxConfigVersion.cmake`，完善下游 `find_package` 体验。

---

## 8) 验收标准（交接口径）

1. 全平台 preset 可 configure/build（至少 Linux + 1 个 Windows 工具链）。
2. `ctest` 全绿且无模块因依赖缺失 silently disabled（除明确降级策略）。
3. 新 LLM 加入模块后，能在 30 分钟内复现 build/test。

---

## 9) 给下一位 LLM 的执行卡

1. 先跑基线：
   - `cmake --preset linux-debug`
   - `cmake --build --preset linux-debug -j4`
   - `ctest --preset linux-debug`
2. 再改构建：
   - 仅改 `CMakeLists.txt` / `cmake/Dependencies.cmake` / `CMakePresets.json`
3. 每改一轮都回归 `configure + build + ctest`。

---

## 10) 最新进展（2026-04-03）

已完成本模块 P1 的三项落地：
1. 依赖解析日志升级为“来源 + target + version”三元信息（`Dependencies.cmake`）。
2. 新增严格依赖开关 `PDF2DOCX_STRICT_DEPS`，可在依赖缺失时 fail-fast。
3. `CMakePresets.json` 补齐 Windows Debug 测试 preset：
   - `windows-msvc-debug`
   - `windows-mingw-debug`

最新 Linux 验证结果：`ctest --preset linux-debug` 通过（`16/16`）。

---

## 11) 增量进展（2026-04-03, 测试 fixture 回退）

为避免测试环境里 `build/test.pdf` / `build/test-image-text.pdf` 缺失导致回归误失败，已在
`CMakeLists.txt` 增加 fixture 自动回退：
1. `PDF2DOCX_TEST_PDF`：`test.pdf -> image.pdf -> text.pdf`
2. `PDF2DOCX_TEST_IMAGE_TEXT_PDF`：`test-image-text.pdf -> image-text.pdf -> PDF2DOCX_TEST_PDF`

并在 configure 日志输出最终 fixture 路径，便于排障。
