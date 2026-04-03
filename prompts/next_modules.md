# 下一阶段模块清单（2026-04-03）

## M10：`--dump-ir` 单次提取复用（已完成）

目标：避免 `pdf2docx --dump-ir` 先 `ConvertFile` 再二次 `ExtractIrFromFile` 的重复解析。

计划：
1. 扩展 `Converter` API，支持转换时返回已处理 IR。
2. `tools/cli/main.cpp` 改为复用该 IR 直接落盘 JSON。
3. 新增/更新 `converter_test` 验证返回 IR 与 stats 一致性。

验收：
- `ctest --preset linux-debug` 全绿。
- `pdf2docx ... --dump-ir ...` 功能不回归。

完成结果：
1. `Converter` 新增重载：`ConvertFile(..., ConvertStats*, ir::Document*)`。
2. `pdf2docx` CLI 的 `--dump-ir` 改为复用该 IR，删除二次 `ExtractIrFromFile`。
3. `converter_test` 增加“返回 IR 与 stats 一致性”断言。
4. 本地回归：`ctest --preset linux-debug`（`16/16`）。

---

## M11：Pipeline 行内合并（已完成）

目标：在当前排序基础上做最小行内 span 合并，提升 DOCX 文本可读性。

计划：
1. 在 `pipeline` 增加“同一行近邻 span 合并”阶段。
2. 控制在保守策略，避免跨列误合并。
3. 补 `pipeline_test` 用例覆盖。

验收：
- 现有测试不回归。
- 合并规则在构造用例上稳定可复现。

完成结果：
1. `pipeline` 新增同一行近邻 span 合并逻辑（保守阈值，避免跨列误合并）。
2. `PipelineStats` 增加 `merged_span_count`。
3. `pipeline_test` 新增合并用例（`Hello + world -> Hello world`）。
4. 本地回归：`ctest --preset linux-debug`（`16/16`）。

---

## M12：图片锚定精度提升（已完成第一阶段）

目标：让 anchored 图片位置更接近 PDF 页面布局。

计划：
1. 使用 `ImageBlock.quad` 改进坐标映射（不是仅靠 AABB）。
2. 增加调试输出与测试断言（至少验证关键 XML 节点参数变化）。

验收：
- `docx_anchor_test` 增强后通过。
- 端到端样本视觉偏差降低（人工抽检）。

阶段结果：
1. `P0Writer` 在收集图片时优先使用 `ImageBlock.quad` 计算锚定边界。
2. 写出 `a:xfrm rot`（从 quad 方向估算旋转角）。
3. `docx_anchor_test` 增加位置偏移与旋转属性断言并通过。
4. 本地回归：`ctest --preset linux-debug`（`16/16`）。

---

## M13：阶段耗时可观测性（已完成）

目标：让性能优化有量化依据，输出 extraction/pipeline/write 三段耗时。

完成结果：
1. `ConvertStats` 新增：
   - `extract_elapsed_ms`
   - `pipeline_elapsed_ms`
   - `write_elapsed_ms`
2. `converter.cpp` 增加阶段计时并写入统计。
3. `pdf2docx` CLI 输出新增三段耗时字段。
4. `converter_test` 新增阶段耗时有效性断言。
5. 本地回归：`ctest --preset linux-debug`（`16/16`）。

---

## M14：Pipeline 段落分组（下一步）

目标：把“行内合并”进一步提升为“按行聚合 + 段落分组”，减少 DOCX 中碎片化段落。

计划：
1. 在 pipeline 增加最小行聚合结构（内部模型，不改公开 IR）。
2. 同段落行合并为单个 `TextSpan`（保守启发式）。
3. 新增对应单测与回归样本断言。

验收：
- `ctest --preset linux-debug` 全绿。
- `test-image-text.pdf` 导出的段落碎片明显减少（抽样检查）。

## M15：图片抽取兼容性补强（下一步）

目标：进一步降低 `skipped_images`，并使 warning 分类更可操作。

计划：
1. 扩展更多滤镜/颜色空间处理分支。
2. 在 stats 中补更细分原因映射（如 color-space unsupported）。
3. 增加对应 fixture 与测试断言。

验收：
- 现有样本不回归。
- 新样本上 `skipped_images` 有可验证下降。

当前状态补充（2026-04-03）：
- 已完成“魔数识别 + 多像素格式解码 + ICCBased fallback”增强；
- `build/image-text.pdf` 已从 `skipped_images=5` 收敛到 `skipped_images=0`；
- 后续 M15 重点转向“几何精度优化（减少归一化警告）与视觉对齐质量”。
