# 模块 03：IR 与 Pipeline（当前状态 + 详细演进设计）

## 1) 模块定位

本模块负责中间表示（IR）定义与结构化处理流程（Pipeline）。
当前工程中：
- IR 已用于后端输出、HTML 渲染、DOCX 写入
- Pipeline 仍是占位执行器（MVP 结构未完全落地）

相关文件：
- `cpp_pdf2docx/include/pdf2docx/ir.hpp`
- `cpp_pdf2docx/src/pipeline/pipeline.hpp`
- `cpp_pdf2docx/src/pipeline/pipeline.cpp`

---

## 2) 当前 IR（真实结构）

```cpp
namespace pdf2docx::ir {
struct Rect { double x,y,width,height; };
struct TextSpan {
  std::string text;
  double x,y,length;
  bool has_bbox;
  Rect bbox;
};
struct ImageBlock {
  std::string id;
  std::string mime_type;
  std::string extension;
  double x,y,width,height;
  std::vector<uint8_t> data;
};
struct Page {
  double width_pt, height_pt;
  std::vector<TextSpan> spans;
  std::vector<ImageBlock> images;
};
struct Document { std::vector<Page> pages; };
}
```

---

## 3) IR 语义约定（交接重点）

1. 坐标系：PDF 原生（左下原点）
2. 单位：`pt`
3. `TextSpan.length`：当前来自 PoDoFo `entry.Length`，并非严格排版宽度
4. `ImageBlock.data`：原始编码流（通常 JPEG/JP2）
5. IR 当前是“可渲染快照”，不是“语义文档模型”

---

## 4) 当前 Pipeline 状态

`Pipeline::Execute()` 已接入转换流程，但逻辑为占位（不改变 IR）。

现有编排（`Converter::ConvertFile`）：
1. 后端提取 IR
2. 执行 `Pipeline::Execute()`
3. DOCX 写出

这给后续“增量引入算法阶段”留了接口位。

---

## 5) 建议演进架构（详细）

建议引入显式阶段接口：
```cpp
class IStage {
 public:
  virtual ~IStage() = default;
  virtual Status Run(ir::Document* doc) const = 0;
  virtual const char* Name() const = 0;
};
```

Pipeline 维护阶段列表：
```cpp
class Pipeline {
 public:
  void AddStage(std::unique_ptr<IStage> s);
  Status Execute(ir::Document* doc, PipelineStats* stats) const;
};
```

阶段建议（按实现顺序）：
1. `NormalizeCoordinatesStage`
2. `SortSpansStage`
3. `LineMergeStage`
4. `ParagraphMergeStage`
5. `StyleInferStage`
6. `TableDetectStage`（后续）

---

## 6) 近期可落地最小算法（不破坏现有链路）

## 6.1 `SortSpansStage`
- 输入：每页 `spans`
- 规则：
  - 先按 `y`（从上到下）分桶
  - 再按 `x` 升序
- 目标：提升导出到 DOCX 的文本顺序稳定性

## 6.2 `LineMergeStage`
- 基于 `bbox.y/bbox.height` 聚类为行
- 无 bbox 时回退 `y + length` 近似策略

## 6.3 输出兼容
- 先保持当前 `ir.hpp` 不变
- 新结构（段落/行）作为 pipeline 内部临时对象，避免一次性改动过大

---

## 7) 与其他模块接口边界

1. 后端模块只产出“原始 spans/images”，不做语义推断
2. DOCX/HTML 模块读取 IR，不依赖具体 PDF 后端
3. API 模块只调用 `Pipeline::Execute()`，不感知具体 Stage

---

## 8) 测试设计（本模块）

建议新增：
1. `tests/unit/ir_sort_stage_test.cpp`
2. `tests/unit/line_merge_stage_test.cpp`
3. `tests/unit/pipeline_stage_order_test.cpp`

断言方向：
- 排序稳定性
- 空文档/空页处理
- 无 bbox 文本的降级行为

---

## 9) 后续任务卡（给下一位 LLM）

## P1
1. 保持现有 IR 结构不变，引入 `SortSpansStage` 并在 Pipeline 启用。
2. 给 `pdf2ir` 增加 `--normalized`（输出排序后 IR）。

## P2
1. 增量引入 `Line` 与 `Paragraph` 内部模型。
2. 评估是否扩展 `ir.hpp` 为双层结构（raw + semantic）。

---

## 10) 验收标准

1. 加入 Stage 后，现有测试不回归（当前 `16` 项）。
2. `build/test.pdf` 导出的文本顺序比基线更稳定（人工 spot check）。
3. `pdf2ir` 输出可用于 LLM 调试和比对。

---

## 11) 最新进展（2026-04-03）

已落地：
1. `Pipeline::Execute()` 从占位改为真实执行，接口升级为 `Execute(ir::Document*, PipelineStats*)`。
2. 启用了最小 `SortSpansStage` 逻辑（页内先 y 后 x，稳定排序）。
3. 新增 `PipelineStats` 并在 `converter` 调用链中接入。
4. `pipeline_test` 已扩展为排序行为断言，当前回归通过。
