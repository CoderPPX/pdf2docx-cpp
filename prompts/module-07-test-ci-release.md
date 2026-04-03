# 模块 07：测试、CI、发布与交接流程详细设计

## 1) 模块定位

本模块保证“改动可验证”和“交付可复现”：
- 单测/集成测试规则
- CI 矩阵设计
- 发布产物规范
- 交接给 LLM 的执行流程

当前测试文件目录：
- `cpp_pdf2docx/tests/unit/*`

---

## 2) 当前测试状态（已实现）

当前 CTest 共 11 项：
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

说明：
- 已覆盖“含图片 test.pdf”端到端路径。
- `converter_test` 已断言 `image_count > 0`。

---

## 3) 测试分层（建议与现状对齐）

## 3.1 Unit（当前主要层）
- 状态码、后端可用性、writer 基本行为、工具输出结构

## 3.2 Integration（建议新增）
- `pdf2docx test.pdf -> out.docx` 并校验 zip part
- `pdf2ir test.pdf -> json` 并校验图片统计

## 3.3 Regression（建议新增）
- 保存 IR golden（字段子集，不含大二进制）
- 对比页数、文本关键字、图片计数

---

## 4) CI 设计（目标态）

建议矩阵：
1. `ubuntu-latest` + gcc + ninja
2. `macos-latest` + clang + ninja
3. `windows-latest` + MSVC
4. `windows-latest` + MinGW + ninja

每 job 执行：
```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>
```

当前 `CMakePresets.json` 已含上述 configure/build preset，CI 只需对接。

---

## 5) 样本与 fixture 策略

当前 fixture：
- `cpp_pdf2docx/build/test.pdf`

建议扩展：
1. `samples/basic/`：纯文本
2. `samples/mixed/`：图文混排（含 DCT/JPX/Flate 图像）
3. `samples/edge/`：损坏/异常 PDF
4. `samples/layout/`：旋转图像/多栏

每样本维护：
- `expected_page_count`
- `expected_min_image_count`
- `expected_keywords[]`

---

## 6) 质量门禁（建议）

## 6.1 PR 必过
1. 全量单测通过
2. 至少一个端到端集成测试通过
3. 无新增编译 warning（或有记录）

## 6.2 发布前必过
1. 四平台 CI 全绿
2. `test.pdf` 三工具实跑通过：
   - `pdf2ir`
   - `ir2html`
   - `pdf2docx`
3. 产物内容检查：
   - docx 内含 `word/media/` 与 `document.xml.rels`

---

## 7) 发布产物建议

最小发布包：
1. `libpdf2docx_core`（静态/动态）
2. CLI：`pdf2docx`、`pdf2ir`、`ir2html`
3. headers：`include/pdf2docx/*.hpp`
4. CMake 导出：
   - `pdf2docxTargets.cmake`

文档：
- `README.md`
- `prompts/llm_handoff.md`
- 各模块设计（本目录）

---

## 8) 交接标准流程（给下一位 LLM）

1. 先读：
   - `prompts/llm_handoff.md`
   - `prompts/module-*.md`
2. 再跑基线：
   - `cmake --preset linux-debug`
   - `cmake --build --preset linux-debug -j4`
   - `ctest --preset linux-debug`
3. 开发后至少回归：
   - 改动相关单测
   - 一次全量 `ctest`
4. 更新：
   - `prompts/worklog.md`
   - 对应 module 文档

---

## 9) 后续任务卡（测试/CI）

## P1
1. 新增集成测试：zip 中实际枚举 `word/media/*`。
2. 增加 `pdf2ir` JSON 关键字段 golden 断言。
3. 为 Windows preset 增加 testPreset。

## P2
1. 加入性能基准（单页耗时统计）。
2. 建立回归指标报表（页数/图片数/警告数）。
