# Qt PDFTools 方案二 Roadmap（内存会话 + Save 时落盘）

## 1. 目标
- 让“编辑当前PDF”操作先作用于当前 Tab 的会话状态，不立即调用 `native-lib` 落盘。
- 用户点击 `Save/Save As` 时，才把会话里的变更一次性提交给 backend（PoDoFo）。
- 保持现有能力：`dirty` 圆点、关闭确认、Undo/Redo、多 Tab。

## 2. 核心思路
- 当前实现是“每次编辑 -> backend 生成临时 PDF -> 重新加载 Tab”。
- 方案二改为“每次编辑 -> 只记录会话变更（Operation Log）+ 即时预览映射”。
- `Save` 时执行 `MaterializeSession()`，将操作日志按顺序应用到真实 PDF 文件。

## 3. 数据模型设计
新增 `DocumentSession`（建议挂在 `MainWindow`，key 为 `PdfTabView*`）：

```cpp
struct EditOp {
  enum class Type { kMergeAppend, kDeletePage, kSwapPages };
  Type type;
  QString source_pdf;   // merge 用
  int page_a = 0;       // delete/swap 用
  int page_b = 0;       // swap 用
};

struct DocumentSession {
  QString base_pdf_path;        // 打开时原始路径
  QString save_path;            // Save/SaveAs 目标
  bool dirty = false;

  QVector<EditOp> applied_ops;  // 当前有效操作日志
  QVector<EditOp> redo_ops;     // redo 栈

  // 用于预览页映射，不是最终文件
  // 例如 [1,2,5,3] 表示当前可见页序（可扩展成结构体）
  QVector<int> logical_pages;
};
```

要点：
- `Delete`/`Swap` 先改 `logical_pages`，并记录 `EditOp`。
- `Merge` 先登记“追加来源文档”的逻辑段，不立即合并实体 PDF。
- 所有改动统一 `MarkDirty(tab)=true`，并清空 `redo_ops`。

## 4. UI/交互改造
- `编辑当前PDF` 的按钮保持现在的单动作风格（无“预览/应用”双按钮）。
- 点击 `删除/交换/合并` 后：
  - 仅更新 session 与预览显示；
  - 写日志到“日志”面板；
  - 不触发 `RunTaskForCurrentTab(...)`。
- `Save`/`Save As` 时：
  - 若 `session.applied_ops` 为空，仅走普通 copy/save；
  - 否则执行 `MaterializeSession(tab, session, target_path)`。

## 5. 预览实现（不落盘）
### 5.1 删除页
- 在 `logical_pages` 删除对应项。
- `PdfTabView` 增加“逻辑页 -> 物理页”映射接口。
- 导航/页码显示基于逻辑页，渲染时映射到源页。

### 5.2 交换页
- 交换 `logical_pages[a-1]` 与 `logical_pages[b-1]`。
- 立即刷新当前页与总页数显示。

### 5.3 合并进来（追加）
- 逻辑层新增“外部段引用”（可扩展 `logical_pages` 为 `{pdf_path,page}` 结构）。
- 渲染时根据段来源选择 `QPdfDocument`（主文档或附加文档缓存）。
- MVP 可先限制为“仅追加完整文档”。

## 6. Save 时落盘（唯一 backend 写操作）
新增：
- `bool MainWindow::MaterializeSession(PdfTabView* tab, DocumentSession& s, const QString& target_pdf);`

执行流程：
1. 选输入基线：
   - `input = s.base_pdf_path`
2. 顺序应用 `s.applied_ops`：
   - `kDeletePage` -> 调 `native-lib DeletePage`
   - `kSwapPages` -> 调 `native-lib SwapPages`
   - `kMergeAppend` -> 调 `native-lib Merge`（`[current, source]`）
3. 每步输出到受控临时文件（`/tmp/qt_pdftools_session/*.pdf`）。
4. 成功后把最终产物写入 `target_pdf`，更新：
   - `s.base_pdf_path = target_pdf`
   - `s.save_path = target_pdf`
   - `s.applied_ops.clear(); s.redo_ops.clear(); s.dirty=false`
5. 失败时：
   - 保留 session 不清空（便于用户继续修复/重试）；
   - 日志输出统一错误码与上下文。

## 7. Undo/Redo（会话级）
- Undo：
  - 从 `applied_ops` 弹出最后一条；
  - 回滚 `logical_pages`（或重建逻辑视图）；
  - 压入 `redo_ops`。
- Redo：
  - 从 `redo_ops` 取回并重新 apply 到 session；
  - 重新刷新逻辑视图。
- 注意：Undo/Redo 不触发 backend。

## 8. 迁移步骤（建议执行顺序）
### M1：引入会话层
- 增加 `DocumentSession` 容器与生命周期（open/close tab）。
- `SetTabDirty` 改为由 session 驱动。

### M2：Delete/Swap 改成纯会话操作
- UI 按钮不再调用 `RunTaskForCurrentTab`。
- 打通逻辑页映射预览。

### M3：Merge 追加改成纯会话操作
- 先做“整文档追加”MVP。
- 引入附加文档缓存。

### M4：Save 物化管线
- 实现 `MaterializeSession()`。
- 接入现有异常规范（`Status + ErrorEnvelope`）。

### M5：Undo/Redo 会话化
- 不再依赖“临时 PDF 路径栈”。
- 改为基于 `EditOp` 的会话回放。

### M6：清理旧路径
- 下线 `RunTaskForCurrentTab(..., apply_current=true)` 的编辑分支。
- 保留“传统操作区”继续直接调用 backend（输出新文件）。

## 9. 测试用例（最小集）
### 单元测试
- `Session_DeletePage_UpdatesLogicalPages`
- `Session_SwapPages_UpdatesLogicalPages`
- `Session_UndoRedo_RestoresOrder`
- `Materialize_AppliesOpsInOrder`
- `Materialize_Failure_KeepSessionDirty`

### 集成测试
- 打开 PDF -> 删除第2页 -> 交换1/2 -> Save -> 重开后页序正确。
- 连续三次编辑不保存，关闭时弹“保存/丢弃/取消”。
- Save As 新路径成功，原文件不变。

### 手工测试
- 大 PDF（>300 页）连续删除/交换，UI 不阻塞。
- 编辑后直接关闭应用，恢复逻辑与 dirty 提示正常。

## 10. 风险与边界
- 真正“纯内存 PDF 改写”在现有 PoDoFo 接口下成本高；方案二采用“会话内存 + Save 时一次落盘”是折中且可落地。
- Merge 的跨文档逻辑预览需要多文档缓存，建议先交付追加 MVP，再扩展插入到任意位置。
- 若操作链很长，Save 物化可能耗时，建议加进度提示与可取消策略（后续迭代）。

