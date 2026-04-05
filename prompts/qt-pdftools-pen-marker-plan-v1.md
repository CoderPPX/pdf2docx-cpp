# Qt PDFTools 画笔与标记功能实现方案 (v1)

## 1. 目标与范围
目标：在 `qt_pdftools` 中新增可用的 PDF 批注能力，优先支持：
- 画笔（自由手写/涂鸦，Ink）
- 标记（荧光笔，高亮）

范围说明：
- 第一阶段支持“页面可视预览 + 保存到 PDF（注释对象）”
- 不做复杂协作和评论线程
- 不改动现有 merge/delete/insert/replace/pdf2docx 主流程

---

## 2. 功能定义

### 2.1 画笔（Ink）
- 鼠标/触控拖动生成连续路径
- 可调：颜色、线宽、不透明度
- 支持撤销/重做
- 支持删除当前页最后一笔 / 清空当前页

### 2.2 标记（Marker/Highlight）
- 模式 A（先实现）：自由轨迹荧光标记（半透明粗线）
- 模式 B（后续）：文本选择高亮（基于文本 quads）
- 可调：颜色、粗细、透明度（默认 35%~45%）

---

## 3. 总体架构

### 3.1 UI 层
新增 `AnnotationOverlay`（叠加在 `PdfTabView` 上）：
- 捕获鼠标事件（press/move/release）
- 实时绘制临时路径（橡皮筋反馈）
- 将路径点转换为 PDF 页坐标并提交到模型

新增 `AnnotationToolbar`（可放侧栏或状态栏）：
- 工具切换：`None / Pen / Marker / Eraser`
- 属性：颜色、线宽、透明度
- 操作：撤销、重做、清空当前页、保存注释

### 3.2 数据模型
新增 `AnnotationStore`（按文档 + 页维护）：
- `map<doc_id, map<page_index, vector<AnnotationStroke>>>`
- `AnnotationStroke` 字段：
  - `tool` (`ink` / `marker`)
  - `color_rgba`
  - `width_pt`
  - `opacity`
  - `vector<PointNorm> points`（归一化坐标，0~1）
  - `timestamp`

关键点：存储使用“页面归一化坐标”，避免缩放变化导致偏移。

### 3.3 持久化层
两级持久化：
1. 会话草稿：`<pdf_path>.annotations.json`（可恢复未保存编辑）
2. 写回 PDF：通过 `native-lib` 调 `cpp_pdftools` 写 PDF 注释对象

推荐顺序：先做 JSON 草稿 + 导出新 PDF；稳定后支持直接覆盖。

### 3.4 Backend（cpp_pdftools）
新增 API：
- `AddInkAnnotations(input_pdf, output_pdf, annotations[])`
- `AddHighlightAnnotations(input_pdf, output_pdf, annotations[])`

实现建议（PoDoFo）：
- 画笔：写 `Ink` annotation（`InkList`）
- 标记：
  - v1 可先写半透明路径外观（兼容性好）
  - v2 再补标准 `Highlight` + `QuadPoints`（文本高亮）

---

## 4. 坐标与渲染关键点

### 4.1 坐标系统
输入事件坐标：viewport 像素坐标
输出存储坐标：页内归一化坐标

转换流程：
1. 命中当前页（单页编辑模式优先，降低复杂度）
2. 计算页矩形 `pageRectInView`
3. `x_norm = (x - left) / width`
4. `y_norm = (y - top) / height`
5. clamp 到 `[0,1]`

### 4.2 编辑模式建议
- 浏览模式：保持现有多页 `MultiPage`
- 标注模式：切换为单页编辑（减少多页命中误差）
- 退出标注模式后恢复多页浏览

### 4.3 渲染
- Overlay `paintEvent` 重绘当前页已存 strokes
- Marker 使用 `CompositionMode_Multiply` + alpha
- 为性能：
  - 点采样阈值（最小移动距离）
  - 路径简化（Douglas-Peucker）
  - 每页缓存 `QPixmap`

---

## 5. 交互与状态机

工具状态：
- `Idle`
- `DrawingPen`
- `DrawingMarker`
- `Erasing`

事件：
- `MousePress`：开始 stroke
- `MouseMove`：追加点并实时绘制
- `MouseRelease`：结束 stroke，入 undo 栈

撤销/重做：
- 命令栈记录 `AddStroke/RemoveStroke/ClearPage`

---

## 6. Roadmap（分阶段）

### Milestone 1（MVP）
- [ ] `AnnotationOverlay` + Pen 工具
- [ ] 当前页绘制、撤销/重做
- [ ] 草稿 JSON 保存/加载
- [ ] UI 工具栏（颜色、宽度）

### Milestone 2
- [ ] Marker 工具（半透明）
- [ ] 清空当前页/删除最后一笔
- [ ] 切换标注模式（单页）

### Milestone 3
- [ ] backend `native-lib` API：写入 Ink/Marker 到新 PDF
- [ ] “保存注释为新文件”
- [ ] 错误处理统一到现有 exception convention

### Milestone 4
- [ ] 文本选择高亮（QuadPoints）
- [ ] 橡皮擦命中与局部删除
- [ ] 大文档性能优化

---

## 7. 测试用例（建议）

### 7.1 功能测试
1. Pen 基础绘制
- 步骤：打开 PDF -> Pen -> 连续绘制
- 预期：轨迹连续，缩放后位置不漂移

2. Marker 半透明效果
- 步骤：Marker 在文字上涂抹
- 预期：文字可见，叠加区域加深

3. Undo/Redo
- 步骤：连续画 3 笔，撤销 2 次，重做 1 次
- 预期：状态准确

4. 保存注释为新 PDF
- 步骤：画笔+标记后保存
- 预期：新 PDF 在外部阅读器可见注释

5. 草稿恢复
- 步骤：标注后不保存 PDF，仅保存草稿并重开
- 预期：草稿可恢复

### 7.2 稳定性测试
1. 大文档（>300 页）快速翻页 + 标注
2. 连续缩放/旋转（若支持）后位置一致性
3. 关闭窗口时有未保存标注
- 预期：弹出保存草稿或丢弃确认

### 7.3 回归测试
- 现有 merge/delete/insert/replace/pdf2docx 功能不受影响
- 预览 tab 关闭清理逻辑不被标注功能破坏

---

## 8. 风险与规避
- 风险：多页模式坐标命中复杂 -> 先单页标注模式
- 风险：不同 PDF 阅读器对注释外观兼容不一 -> 先用最兼容写法并加样例回归
- 风险：性能下降 -> 增量重绘 + 简化路径 + 缓存

---

## 9. 建议落地顺序
优先顺序：
1. Pen + Overlay + JSON 草稿
2. Marker + Undo/Redo 完整化
3. native-lib 写入 PDF 注释
4. 文本选择高亮（高级）

该顺序能最快交付“可见、可保存、可回退”的可用版本。
