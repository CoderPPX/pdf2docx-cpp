# 模块 07：测试、CI、发布与交接流程

## 1) 模块定位

本模块目标是保证改动可验证、可复现、可交接：
1. 单测与集成测试分层。
2. CI 多平台矩阵。
3. 交付前回归与产物抽检规范。

---

## 2) 当前测试状态

当前 `ctest` 共 `16` 项：
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

当前覆盖：
- API/后端/writer 基础行为。
- 图文 PDF 的端到端链路。
- no-image、anchor、IR JSON 与 only-page 调试路径。

---

## 3) 集成测试（已落地）

新增目录：`cpp_pdf2docx/tests/integration/`

现有用例：`end_to_end_test.cpp`
- `test-image-text.pdf -> docx`。
- 校验 ZIP 关键 part：`word/media/*`、`word/styles.xml`。
- 校验 no-image 路径下媒体数量为 0。

---

## 4) CI（草案已落地）

文件：`.github/workflows/ci.yml`

矩阵：
1. `ubuntu-latest` + `linux-debug`
2. `macos-latest` + `macos-debug`
3. `windows-latest` + `windows-msvc-debug`
4. `windows-latest` + `windows-mingw-debug`（MSYS2）

统一步骤：
```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>
```

---

## 5) 回归与验收规范

## PR/提交前
1. 至少执行一次本地全量 `ctest`。
2. 对核心路径改动需补对应单测。

## 发布前
1. 四平台 CI 全绿。
2. 样本实跑：
   - `pdf2ir`
   - `ir2html`
   - `pdf2docx`
3. 产物抽检：
   - DOCX 包内 part 完整；
   - IR summary 与 media 数量一致。

---

## 6) 交接流程（固定）

1. 先读：
   - `prompts/llm_handoff.md`
   - `prompts/module-*.md`
2. 再跑：
   - `cmake --preset linux-debug`
   - `cmake --build --preset linux-debug -j4`
   - `ctest --preset linux-debug`
3. 开发后更新：
   - `prompts/worklog.md`
   - 对应模块文档
   - `prompts/llm_handoff.md`

---

## 7) 后续任务

## P1
1. 增加更多回归样本（旋转图、多栏、异常流）。
2. 为 CI 增加产物检查步骤（zip part 断言）。

## P2
1. 增加性能基线（阶段耗时统计与阈值告警）。
2. 建立回归报告（页数/图片数/warning 趋势）。

---

## 8) 最新验证（2026-04-03）

- `ctest --preset linux-debug`：`16/16` 通过。
