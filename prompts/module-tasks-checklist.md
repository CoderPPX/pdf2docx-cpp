# 模块执行 TODO 清单（逐条命令 + 验收）

> 适用项目：`/home/zhj/Projects/pdftools/cpp_pdf2docx`  
> 目标：让任意 LLM/工程师按模块逐步执行并可验证结果。  
> 约定：每个模块先跑“基线命令”，再做改动，最后跑“验收命令”。

---

## 全局基线（每次开始前先执行）

## TODO G-01：确认工程可编译
- 命令：
```bash
cd /home/zhj/Projects/pdftools/cpp_pdf2docx
cmake --preset linux-debug
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```
- 验收：
  - `ctest` 全绿（当前基线 `16/16`）

## TODO G-02：实跑三工具
- 命令：
```bash
cd /home/zhj/Projects/pdftools/cpp_pdf2docx
./build/linux-debug/pdf2ir build/test.pdf build/linux-debug/test_ir.json
./build/linux-debug/ir2html build/test.pdf build/linux-debug/test_ir_images.html --scale 1.3
./build/linux-debug/pdf2docx build/test.pdf build/linux-debug/test_images.docx
```
- 验收：
  - 三个文件都存在
  - `test_ir.json` 含 `"images"`
  - `test_images.docx` 为 zip（`PK` 开头）

---

## 模块 01：构建系统与依赖

## TODO M01-01：输出依赖解析摘要（版本+来源）
- 改动文件：
  - `cmake/Dependencies.cmake`
- 建议内容：
  - 配置阶段打印每个依赖的版本和实际路径
- 验收命令：
```bash
cmake --preset linux-debug
```
- 验收：
  - configure 日志中出现 `zlib/tinyxml2/minizip/freetype/podofo` 的来源信息

## TODO M01-02：增加严格依赖开关
- 改动文件：
  - `CMakeLists.txt`
  - `cmake/Dependencies.cmake`
- 建议内容：
  - 新增 `PDF2DOCX_STRICT_DEPS`，开启后缺依赖直接失败
- 验收命令：
```bash
cmake --preset linux-debug
```
- 验收：
  - 开关为 ON 时依赖缺失可 fail-fast；ON/OFF 两模式都可配置

## TODO M01-03：补全 test presets（Windows）
- 改动文件：
  - `CMakePresets.json`
- 建议内容：
  - 增加 `windows-msvc-debug` 与 `windows-mingw-debug` 的 `testPresets`
- 验收：
  - `CMakePresets.json` 结构合法，Linux 现有 preset 不受影响

---

## 模块 02：PoDoFo 后端

## TODO M02-01：增加图片抽取统计（成功/跳过原因）
- 改动文件：
  - `src/backend/podofo/podofo_backend.cpp`
  - `include/pdf2docx/types.hpp`（如需统计字段）
- 建议内容：
  - 统计每页/全局：抽取成功、滤镜不支持、空流、异常
- 验收命令：
```bash
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```
- 验收：
  - 不破坏现有 `converter_test`
  - 统计字段可在 API 或日志观察到

## TODO M02-02：补 `FlateDecode` 图片导出路径（P1）
- 改动文件：
  - `src/backend/podofo/podofo_backend.cpp`
  - 可新增：`src/backend/podofo/image_decode_helpers.*`
- 建议内容：
  - 支持 `FlateDecode` -> 可写出格式（优先 PNG）
- 验收命令：
```bash
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```
- 验收：
  - 现有样本不回归
  - 新样本（含 Flate 图片）可进入 IR `images`

---

## 模块 03：IR 与 Pipeline

## TODO M03-01：实现 spans 排序阶段（不改 IR 结构）
- 改动文件：
  - `src/pipeline/pipeline.hpp`
  - `src/pipeline/pipeline.cpp`
  - 可新增：`src/pipeline/stages/sort_spans_stage.*`
- 建议内容：
  - 页内按阅读顺序稳定排序（先 y 后 x）
- 验收命令：
```bash
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```
- 验收：
  - `pipeline_test` 通过
  - `test_pdf_fixture_test` 通过

## TODO M03-02：补 pipeline 单测
- 改动文件：
  - `tests/unit/pipeline_test.cpp`
  - 可新增：`tests/unit/pipeline_sort_test.cpp`
  - `CMakeLists.txt`
- 验收：
  - 新测试可稳定验证排序结果

---

## 模块 04：字体系统

## TODO M04-01：扩展字体探测输出（版本/能力）
- 改动文件：
  - `include/pdf2docx/font.hpp`
  - `src/font/freetype_probe.cpp`
- 建议内容：
  - probe 返回更多上下文（例如 freetype 可用版本）
- 验收命令：
```bash
cmake --build --preset linux-debug -j4
ctest --preset linux-debug -R freetype_test
```
- 验收：
  - `freetype_test` 通过
  - API 不破坏已有调用

## TODO M04-02：引入最小 `FontResolver`（P2）
- 改动文件（建议）：
  - `include/pdf2docx/font_resolver.hpp`
  - `src/font/font_resolver.cpp`
  - 可选配置：`config/fonts.json`
- 验收：
  - 可做 alias/fallback 解析
  - 不影响现有转换主链

---

## 模块 05：DOCX 写出

## TODO M05-01：增加 docx 媒体文件计数验证（替代 strings）
- 改动文件：
  - `tests/unit/docx_image_test.cpp`
  - 可新增：`tests/unit/docx_zip_inspect_test.cpp`
- 建议内容：
  - 用 unzip API/zip reader 验证 `word/media/*` 数量
- 验收命令：
```bash
cmake --build --preset linux-debug -j4
ctest --preset linux-debug -R docx
```
- 验收：
  - 不依赖 `strings` 文本匹配
  - media 检查更稳定

## TODO M05-02：添加 styles.xml 最小模板
- 改动文件：
  - `src/docx/p0_writer.cpp`
- 验收：
  - DOCX 仍可打开
  - 现有 writer/converter/docx_image 测试通过

## TODO M05-03：anchored 布局预研（P2）
- 改动文件：
  - `src/docx/p0_writer.cpp`
  - 可新增：`src/docx/drawing_anchor_helpers.*`
- 验收：
  - 加开关不影响默认路径
  - 新增样本可看到位置改善

---

## 模块 06：API 与 CLI

## TODO M06-01：给 `pdf2docx` 增加 `--dump-ir`
- 改动文件：
  - `tools/cli/main.cpp`
  - `src/api/converter.cpp`（如需复用提取）
- 验收命令：
```bash
./build/linux-debug/pdf2docx build/test.pdf /tmp/out.docx --dump-ir /tmp/out_ir.json
```
- 验收：
  - `/tmp/out_ir.json` 存在且可读
  - 转换成功不受影响

## TODO M06-02：支持 `--no-images`
- 改动文件：
  - `tools/cli/main.cpp`
  - 可能涉及：`types.hpp`（复用现有 `extract_images`）
- 验收：
  - 启用参数后 `stats.image_count == 0`
  - 其余流程正常

---

## 模块 07：测试、CI、发布

## TODO M07-01：新增 integration 测试目录与首个用例
- 改动文件：
  - `tests/integration/*`（新增）
  - `CMakeLists.txt`
- 用例建议：
  - 端到端 `test.pdf -> docx` 并检查关键 part
- 验收：
  - `ctest` 可发现并执行 integration 用例

## TODO M07-02：CI workflow 草案
- 改动文件（建议）：
  - `.github/workflows/ci.yml`（如仓库使用 GitHub）
- 目标矩阵：
  - Linux / macOS / Windows MSVC / Windows MinGW
- 验收：
  - 工作流语法正确，可被 CI 平台识别

---

## 模块 08：图片专项

## TODO M08-01：补图片四角点表示（为 anchored 做准备）
- 改动文件：
  - `include/pdf2docx/ir.hpp`
  - `src/backend/podofo/podofo_backend.cpp`
  - `src/core/ir_html.cpp`（可视化调试）
- 验收：
  - IR 保留四角点或等价变换参数
  - HTML 可开启 debug 叠加显示

## TODO M08-02：将图片抽取 warning 映射到 stats
- 改动文件：
  - `src/backend/podofo/podofo_backend.cpp`
  - `src/api/converter.cpp`
  - `include/pdf2docx/types.hpp`
- 验收：
  - `warning_count` 有实际意义，不再固定 0

---

## 模块 09：工具与调试

## TODO M09-01：`pdf2ir` 增加 summary 区块
- 改动文件：
  - `tools/pdf2ir/main.cpp`
- 建议输出：
  - `total_pages`
  - `total_spans`
  - `total_images`
  - 每页 `span_count/image_count`
- 验收：
  - JSON 结构清晰，兼容旧字段

## TODO M09-02：`ir2html` 增加 `--only-page`
- 改动文件：
  - `tools/ir2html/main.cpp`
  - `src/core/ir_html.cpp`（必要时扩展参数）
- 验收：
  - 大文档仅渲染指定页，便于调试

---

## 最终交付检查（全部模块改动后）

## TODO FINAL-01：完整回归
- 命令：
```bash
cd /home/zhj/Projects/pdftools/cpp_pdf2docx
cmake --preset linux-debug
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```
- 验收：
  - 全绿

## TODO FINAL-02：文档更新
- 必更新文件：
  - `prompts/worklog.md`
  - `prompts/llm_handoff.md`
  - 对应 `prompts/module-*.md`
- 验收：
  - 记录包含时间、命令、结果、限制

## TODO FINAL-03：产物抽检
- 命令：
```bash
./build/linux-debug/pdf2ir build/test.pdf build/linux-debug/final_ir.json
./build/linux-debug/ir2html build/test.pdf build/linux-debug/final_ir.html --scale 1.3
./build/linux-debug/pdf2docx build/test.pdf build/linux-debug/final.docx
```
- 验收：
  - 三文件都存在
  - `final_ir.json` 含图片
  - `final.docx` 可打开

---

## 执行状态更新（2026-04-03）

已完成项：
1. `G-01`：完成，`ctest --preset linux-debug` 当前 `16/16` 通过。
2. `G-02`：完成，三工具均可实跑并产生产物。
3. `M01-01 ~ M01-03`：完成（依赖日志/严格依赖开关/Windows test preset）。
4. `M02-01 ~ M02-02`：完成（图片提取统计 + FlateDecode 路径）。
5. `M03-01 ~ M03-02`：完成（spans 排序 + pipeline 单测）。
6. `M04-01 ~ M04-02`：完成（FreeType 探测扩展 + FontResolver）。
7. `M05-01 ~ M05-03`：完成（ZIP 级媒体验证 + styles.xml + anchored 开关）。
8. `M06-01 ~ M06-02`：完成（`--dump-ir` + `--no-images`）。
9. `M07-01 ~ M07-02`：完成（integration 用例 + CI workflow 草案）。
10. `M08-01 ~ M08-02`：完成（图片 quad + warning 统计映射）。
11. `M09-01 ~ M09-02`：完成（`pdf2ir summary` + `ir2html --only-page`）。
12. `FINAL-01 ~ FINAL-03`：完成（完整回归 + 文档更新 + 产物抽检）。
