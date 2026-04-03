# LLM 续作交接说明（2026-04-03）

- 更新时间：`2026-04-03 07:41 EDT (-0400)`
- 项目根目录：`/home/zhj/Projects/pdftools`
- 主工程目录：`/home/zhj/Projects/pdftools/cpp_pdf2docx`
- 当前状态：**M01~M13（M12 第一阶段）已执行并通过，Linux 本地回归 `16/16` 全绿**

---

## A. 一句话结论

当前分支已具备可用的 C++ PDF->DOCX 最小闭环（文本 + 图片），并补齐了调试工具、统计字段、integration 测试与 CI 草案；下一阶段重点应转向“高保真布局与更广格式覆盖”。

---

## B. 本轮已完成内容（按模块）

1. 模块 01（构建系统）
   - 依赖日志升级为 `source + target + version`。
   - 新增 `PDF2DOCX_STRICT_DEPS`（严格依赖 fail-fast）。
   - `CMakePresets.json` 补齐 Windows MSVC/MinGW test preset。

2. 模块 02（PoDoFo 后端）
   - 图片抽取新增 `ImageExtractionStats`，包含成功数/跳过数/跳过原因。
   - 新增 `FlateDecode -> PNG` 路径（RGBA decode + 最小 PNG 编码）。
   - 单图 decode 异常改为局部跳过，不中断整文档。

3. 模块 03（IR + Pipeline）
   - `Pipeline::Execute` 从占位改为真实排序（页内 `y` 降序、`x` 升序，稳定排序）。
   - 新增 `PipelineStats`，并接入转换主链。

4. 模块 04（字体系统）
   - `ProbeFreeType` 支持版本与模块能力输出。
   - 新增最小 `FontResolver`（alias/fallback/family->file）。

5. 模块 05（DOCX 写出）
   - 新增 `DocxWriteOptions`，支持 `use_anchored_images`。
   - 默认 inline，开关开启时输出 `wp:anchor` + position 节点。
   - 固定写出 `word/styles.xml`，并补齐 content types 与 styles relationship。
   - DOCX 测试改为 ZIP 二进制目录解析，不再依赖 `strings`。

6. 模块 06（API + CLI）
   - `ConvertOptions` 新增 `docx_use_anchored_images`。
   - `ConvertStats` 增加细分统计：`extracted/skipped/*reason*/backend_warning/font_probe`。
   - `pdf2docx` 新增参数：
     - `--dump-ir <path>`
     - `--no-images`
     - `--disable-font-fallback`
     - `--docx-anchored`

7. 模块 07（测试 + CI）
   - 新增 integration：`tests/integration/end_to_end_test.cpp`。
   - 新增 `.github/workflows/ci.yml`（Linux/macOS/Windows MSVC/Windows MinGW）。

8. 模块 08（图片专项）
   - IR 新增 `Point/Quad`，`ImageBlock` 新增 `has_quad + quad`。
   - HTML debug 可视化新增图片四角点 polygon overlay。
   - 后端 warning 已映射到 `ConvertStats.warning_count`。

9. 模块 09（工具与调试）
   - 新增通用 JSON 写出模块：`WriteIrToJson`。
   - `pdf2ir` 输出 summary（pages/spans/images）。
   - `ir2html` 新增 `--only-page N`。

10. 模块 10（`--dump-ir` 复用优化）
   - `Converter` 新增 `ConvertFile(..., ConvertStats*, ir::Document*)` 重载。
   - `pdf2docx --dump-ir` 改为复用单次提取 IR，去掉二次解析。

11. 模块 11（Pipeline 文本增强）
   - 在排序后新增行内近邻 span 合并。
   - `PipelineStats` 新增 `merged_span_count`，并补单测覆盖。

12. 模块 12（anchored 精度第一阶段）
   - `P0Writer` 锚定边界优先使用 `ImageBlock.quad`。
   - 写出 `a:xfrm rot`（基于 quad 估算旋转角）。
   - `docx_anchor_test` 增加偏移与旋转断言。

13. 模块 13（阶段耗时可观测）
   - `ConvertStats` 增加 `extract/pipeline/write` 分段耗时字段。
   - CLI 输出增加 `extract_ms/pipeline_ms/write_ms`。

14. Hotfix（image-text 专项）
   - 修复 title 乱排：writer 按同一行聚合 span 再写段落。
   - 修复 ICCBased 图片缺失：PoDoFo 颜色空间 fallback + 后端回退增强。
   - 修复异常图片坐标：对极端几何做 page 归一化与边界裁剪。

---

## C. 当前可用能力

1. `pdf2docx`：PDF -> DOCX（最小可用，文本 + 图片，支持 inline/anchored 开关）。
2. `pdf2ir`：PDF -> IR JSON（含 summary、每页 span/image 计数、图片 `data_size`、可选 quad）。
3. `ir2html`：PDF -> HTML 绝对定位预览（含图片 + quad overlay + only-page）。
4. 统计可观测：转换后可看到页数、图片数、跳过原因、warning 数。
5. `--dump-ir` 已是单次提取复用路径（不再重复解析 PDF）。
6. 统计含分段耗时：`extract_ms`、`pipeline_ms`、`write_ms`。

---

## D. 当前限制

1. 仍以 PoDoFo 后端为主，Poppler backend 未落地。
2. 图片格式虽新增 Flate 路径，但复杂滤镜/颜色空间仍会出现跳过或 warning。
3. DOCX anchored 仍是近似布局（虽支持 quad 边界与旋转属性），尚未达到高保真版式复现。
4. Pipeline 已有“排序 + 行内合并”，writer 也已做行聚合；但段落/标题/表格语义阶段尚未完成。

---

## E. 关键文件索引

- 构建：
  - `cpp_pdf2docx/CMakeLists.txt`
  - `cpp_pdf2docx/cmake/Dependencies.cmake`
  - `cpp_pdf2docx/CMakePresets.json`
- API：
  - `cpp_pdf2docx/include/pdf2docx/types.hpp`
  - `cpp_pdf2docx/include/pdf2docx/converter.hpp`
  - `cpp_pdf2docx/src/api/converter.cpp`
- 后端：
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.hpp`
  - `cpp_pdf2docx/src/backend/podofo/podofo_backend.cpp`
- 文档模型/工具：
  - `cpp_pdf2docx/include/pdf2docx/ir.hpp`
  - `cpp_pdf2docx/include/pdf2docx/ir_json.hpp`
  - `cpp_pdf2docx/src/core/ir_json.cpp`
  - `cpp_pdf2docx/include/pdf2docx/ir_html.hpp`
  - `cpp_pdf2docx/src/core/ir_html.cpp`
- DOCX：
  - `cpp_pdf2docx/src/docx/p0_writer.hpp`
  - `cpp_pdf2docx/src/docx/p0_writer.cpp`
- Pipeline：
  - `cpp_pdf2docx/src/pipeline/pipeline.hpp`
  - `cpp_pdf2docx/src/pipeline/pipeline.cpp`
- 字体：
  - `cpp_pdf2docx/include/pdf2docx/font.hpp`
  - `cpp_pdf2docx/include/pdf2docx/font_resolver.hpp`
  - `cpp_pdf2docx/src/font/freetype_probe.cpp`
  - `cpp_pdf2docx/src/font/font_resolver.cpp`

---

## F. 测试状态（最新）

`ctest --preset linux-debug`：`16/16` 通过。

测试集合：
1. `status_test`
2. `backend_test`
3. `writer_test`
4. `converter_test`
5. `freetype_test`
6. `font_resolver_test`
7. `pipeline_test`
8. `ir_html_test`
9. `ir_html_only_page_test`
10. `ir_html_image_test`
11. `ir_json_test`
12. `build_info_test`
13. `docx_image_test`
14. `docx_anchor_test`
15. `test_pdf_fixture_test`
16. `integration_e2e_test`

---

## G. 本地复现命令

```bash
cd /home/zhj/Projects/pdftools/cpp_pdf2docx
cmake --preset linux-debug
cmake --build --preset linux-debug -j4
ctest --preset linux-debug
```

`test-image-text.pdf` 抽检：

```bash
./build/linux-debug/pdf2ir build/test-image-text.pdf build/linux-debug/final_ir_image_text.json
./build/linux-debug/ir2html build/test-image-text.pdf build/linux-debug/final_ir_image_text.html --scale 1.3 --only-page 1
./build/linux-debug/pdf2docx build/test-image-text.pdf build/linux-debug/final_image_text.docx --dump-ir build/linux-debug/final_dump_ir.json
```

---

## H. 下一个阶段建议（真实剩余工作）

1. 高保真布局（P1）
   - anchored 从“近似定位”升级到更精确的版式映射。
   - 处理旋转、裁剪、层叠顺序。

2. 文本语义（P1）
   - 在 pipeline 中新增行合并、段落合并、标题/列表启发式。

3. 图片兼容性（P1）
   - 扩展更多滤镜/颜色空间处理。
   - 增加异常图片样本回归集。

4. 工程化（P2）
   - CI 上线后观察跨平台失败点并收敛。
   - 导出更稳定的 install/package 配置，方便下游项目集成。

---

## I. 交接约定

每完成一个模块，必须同步更新：
1. `prompts/worklog.md`
2. 对应 `prompts/module-*.md`
3. `prompts/llm_handoff.md`

本次已完成以上三项同步。
