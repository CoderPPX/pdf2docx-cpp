# `cpp_pdf2docx` 迁移并入 `cpp_pdftools` 计划（V1）

- 更新时间：`2026-04-03`
- 目标：将现有 `cpp_pdf2docx` 成果并入 `cpp_pdftools`，不丢能力、不丢测试。

---

## 1. 迁移边界

保留并迁移：
1. PoDoFo 抽取链路（文本 + 图片 + quad + stats）
2. Pipeline（排序 + 行内合并）
3. DOCX writer（styles + inline/anchor）
4. CLI 参数能力（`--dump-ir`/`--no-images`/`--docx-anchored`）
5. 调试工具能力（IR JSON / HTML only-page）
6. 现有测试资产与 fixture 机制

---

## 2. 目录映射

| 旧目录 | 新目录 |
|---|---|
| `cpp_pdf2docx/include/pdf2docx/*` | `cpp_pdftools/include/pdftools/convert/pdf2docx/*` |
| `cpp_pdf2docx/src/backend/podofo/*` | `cpp_pdftools/src/backend/podofo/*` |
| `cpp_pdf2docx/src/pipeline/*` | `cpp_pdftools/src/convert/pdf2docx/pipeline/*` |
| `cpp_pdf2docx/src/docx/*` | `cpp_pdftools/src/convert/pdf2docx/docx/*` |
| `cpp_pdf2docx/tools/*` | `cpp_pdftools/tools/cli/commands/*` |
| `cpp_pdf2docx/tests/*` | `cpp_pdftools/tests/*` |

---

## 3. 迁移步骤

## S1：工程骨架

1. 创建 `cpp_pdftools` 顶层 CMake、presets、依赖导入。
2. 建立 `pdftools::core` 与 `pdftools::runtime`。

验收：
- 空框架可 configure/build。

## S2：无侵入平移

1. 先平移 `cpp_pdf2docx` 代码到 `convert/pdf2docx` 子模块。
2. 保留原命名空间与 wrapper，减少首轮改动风险。

验收：
- 在 `cpp_pdftools` 内单独跑通 `pdf2docx`。

## S3：接口统一

1. 对外统一成 `pdftools` 风格 `Request/Result`。
2. 保留兼容层，原行为不变。

验收：
- 新旧调用都可工作，输出一致。

## S4：测试平移与收敛

1. 把 16 个测试平移到新工程。
2. 清理路径依赖（fixture 自动回退机制保留）。

验收：
- `ctest` 全绿。

## S5：单 CLI 合并

1. 将 `pdf2docx/pdf2ir/ir2html` 合并到统一 `pdftools` 子命令体系。
2. 保留原子命令别名过渡期。

验收：
- `pdftools convert pdf2docx ...` 可替代旧入口。

---

## 4. 兼容策略

1. `pdf2docx` CLI 继续可用（过渡期）
2. 逐步引导至 `pdftools` 单入口
3. 文档中明确“弃用时间点”

---

## 5. 风险控制

1. 第三方 patch 同步风险
- 将 `thirdparty/podofo` patch 明确登记（尤其 ICCBased 修复）。

2. 迁移引发路径/fixture 失效
- 所有测试输入改为配置时可检测回退路径。

3. 命名空间重构造成大面积冲突
- 先 wrapper，后渐进式重命名。

---

## 6. 迁移完成标志

1. `cpp_pdftools` 已覆盖 `cpp_pdf2docx` 功能。
2. `cpp_pdf2docx` 仅作为历史兼容分支，不再新增功能。
3. 新功能统一只在 `cpp_pdftools` 开发。
