# `pdf2docx` C++ 路线图（纯 CMake、无 vcpkg、MSVC + MinGW 易部署）

## 1. 目标与硬约束

- 目标：实现可嵌入的 `pdf2docx` 库，优先保证文本语义与阅读顺序，再逐步提升视觉还原。
- 平台：`Linux / macOS / Windows (MSVC + MinGW)`。
- 构建：仅使用 `CMake` 生态；**不依赖 vcpkg**。
- 依赖要求：第三方库必须可通过 CMake 集成（`add_subdirectory` / `FetchContent` / 已安装包 `find_package`）。
- 部署目标：Windows 端尽量“一键 configure + build”，避免手工跑 `autotools` 链路。

---

## 2. 依赖选型（最终建议）

## 2.1 核心依赖（默认）

1. **PDF 后端：`PoDoFo`**
   - 理由：C++ API、CMake 友好、在 MSVC/MinGW 下部署复杂度相对可控。
   - 角色：提取页面、文本对象、字体信息、图片对象。

2. **字体度量：`FreeType`**
   - 理由：成熟稳定，跨平台与 CMake 支持好。
   - 角色：字体 fallback、line metrics、字形 advance。

3. **XML 写出：`tinyxml2`（默认）**
   - 理由：轻量、零复杂依赖、CMake 直接集成非常简单。
   - 角色：生成 OOXML（`word/document.xml` 等）。

4. **DOCX 打包：`minizip-ng`**
   - 理由：CMake 支持好、跨平台稳定。
   - 角色：按 OPC 规范将 XML 与媒体资源打包为 `.docx`。

5. **压缩基础：`zlib`**
   - 理由：`minizip-ng` 通常依赖；CMake 生态成熟。

## 2.2 兼容依赖（可选）

- `libxml2`（可选 XML 后端）
  - 保留原因：你已允许使用，且功能全面。
  - 但默认不选：Windows 下依赖链（iconv/lzma/zlib）会增加部署复杂度。
- `Poppler`（可选高保真后端）
  - CMake 支持可用，但依赖较多，MSVC/MinGW 部署成本明显高于 PoDoFo。

## 2.3 Windows CMake 部署便利性评估

| 库 | CMake 支持 | MSVC 部署 | MinGW 部署 | 结论 |
|---|---|---|---|---|
| `PoDoFo` | 好 | 中-易 | 中 | **推荐默认 PDF 后端** |
| `FreeType` | 好 | 易 | 易 | **推荐** |
| `tinyxml2` | 很好 | 很易 | 很易 | **推荐默认 XML** |
| `minizip-ng` | 好 | 易 | 易 | **推荐** |
| `zlib` | 很好 | 易 | 易 | **推荐** |
| `libxml2` | 有 | 中 | 中 | 可选，不作为默认 |
| `Poppler` | 有 | 中-难 | 中-难 | 可选高级后端 |
| `PDFium` | 非纯 CMake 主链路 | 难 | 难 | 本项目暂不选 |
| `MuPDF` | 非纯 CMake 友好路径 | 中-难 | 中-难 | 本项目暂不选 |

> 结论：要实现“MSVC + MinGW 易部署”，默认依赖组合建议：
> `PoDoFo + FreeType + tinyxml2 + minizip-ng + zlib`。

---

## 3. 纯 CMake 集成策略（无 vcpkg）

## 3.1 三层依赖来源策略

- **第一优先：系统/预装包**（`find_package`）
  - Linux/macOS 用户可先走系统包管理器安装。
- **第二优先：源码 vendor**（`third_party/<lib>` + `add_subdirectory`）
  - Windows 推荐此路径，环境一致性最好。
- **第三优先：`FetchContent`**
  - 仅对轻量库（如 `tinyxml2`）默认开启，重库建议 vendor 固定版本。

## 3.2 CMake 顶层选项建议

- `PDF2DOCX_USE_BUNDLED_DEPS=ON`（Windows 默认 ON）
- `PDF2DOCX_XML_BACKEND=tinyxml2`（可选 `libxml2`）
- `PDF2DOCX_PDF_BACKEND=podofo`（可选 `poppler`）
- `PDF2DOCX_BUILD_SHARED=ON/OFF`
- `PDF2DOCX_ENABLE_TESTS=ON`
- `PDF2DOCX_ENABLE_CLI=ON`

## 3.3 目标结构

- `pdf2docx_core`
- `pdf2docx_backend_podofo`
- `pdf2docx_font_freetype`
- `pdf2docx_xml_tinyxml2`（或 `pdf2docx_xml_libxml2`）
- `pdf2docx_docx_zip`
- `pdf2docx_pipeline`
- `pdf2docx`（最终聚合库）
- `pdf2docx_cli`

## 3.4 Windows 预设（必须）

- `CMakePresets.json` 至少提供：
  - `windows-msvc-debug`
  - `windows-msvc-release`
  - `windows-mingw-debug`
  - `windows-mingw-release`
- 通过 preset 固化编译器、生成器、构建目录和关键选项，避免手工输入长命令。

---

## 4. 分层架构与职责

1. `backend/`：将 PDF 库对象统一转换为 `PdfPageSnapshot`。
2. `core/ir/`：统一中间模型（Document/Page/Paragraph/Run/Image/Table）。
3. `pipeline/`：阅读顺序、段落合并、样式推断、（可选）表格检测。
4. `font/`：字体匹配与度量。
5. `docx/`：OOXML part 构建 + OPC 打包。
6. `api/`：`Converter` 稳定对外接口。
7. `tools/cli/`：命令行与示例程序。

---

## 5. 详细模块路线图（里程碑）

## M0（1~2 周）：构建与最小闭环

- 建立跨平台 CMake + presets。
- 打通 `tinyxml2 + minizip-ng` 输出固定 docx。
- 实现 `Status/ErrorCode`、日志、基础 API 壳。

## M1（2~4 周）：PoDoFo 文本链路 MVP

- 实现 `IPdfBackend` 与 `PoDoFoBackend`。
- 单页/多页文本提取 -> IR -> DOCX。
- 保留分页与基本 run 样式（字号/粗斜体）。

## M2（2~4 周）：字体与图片

- 接入 FreeType，完善字体 fallback 与行高校正。
- 图片对象导出到 `word/media`。

## M3（3~6 周）：布局增强

- 多栏阅读顺序优化。
- 列表/标题启发式识别。
- 简单表格检测与映射（可选开关）。

## M4（持续）：可选后端与发布

- 增加 `PopplerBackend`（可选编译）。
- 完善 CI（MSVC + MinGW + Linux + macOS）。
- 发布稳定 API 与版本策略。

---

## 6. 关键接口总览（高层）

- `IBackend` / `IDocument` / `IPageExtractor`
- `IRBuilder` / `LayoutAnalyzer` / `StyleInferer`
- `IFontProvider`
- `IOoxmlWriter` / `IOpcPackager`
- `Converter`

> 详细类与函数签名拆分在 `prompts/module-*.md`。

---

## 7. 目录规划

```text
include/pdf2docx/
  converter.hpp
  types.hpp
  status.hpp
src/
  api/
  backend/podofo/
  backend/poppler/
  core/ir/
  pipeline/
  font/freetype/
  docx/xml_tinyxml2/
  docx/xml_libxml2/
  docx/zip/
cmake/
  presets/
  modules/
third_party/
tools/cli/
tests/
```

---

## 8. 质量与验收标准

- 功能：文本、分页、图片可转换。
- 可构建：MSVC 与 MinGW 使用 preset 一次配置成功。
- 可测试：核心单元测试 + 端到端样本回归可运行。
- 可扩展：新增 PDF 后端不需改 `docx` 模块。

---

## 9. 立即执行清单

1. 锁定默认依赖版本（PoDoFo/FreeType/tinyxml2/minizip-ng/zlib）。
2. 建 `CMakePresets.json` 与 `third_party` 拉取脚本。
3. 实现最小 API 与固定 docx 导出。
4. 实现 PoDoFo 文本提取到 IR。
5. 打通端到端并建立 20 份回归样本。
