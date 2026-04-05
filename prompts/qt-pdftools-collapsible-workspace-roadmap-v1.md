# Qt PDFTools 折叠菜单工作区 Roadmap (v1)

## 1. 目标
将当前“工具 Tab 切换”模式改为“每个功能一个折叠菜单（Accordion）”，并新增“编辑当前PDF”工作流，同时保留原有 PDF 操作页。

目标包含：
- 新增折叠菜单项：`编辑当前PDF`（原地编辑当前打开 Tab）
- 新增折叠菜单项：`批注与书签`（画笔/荧光笔/书签）
- 保留原有 `传统操作`（合并/删除页/插入页/替换页/PDF2DOCX）
- 与现有 `dirty tab + Save/Save As + 关闭确认` 一致联动

---

## 2. 目标 UI 信息架构

左侧：`可滚动折叠菜单`
- 编辑当前PDF（新增）
- 传统操作（保留）
- 批注与书签（新增）
- 任务日志（保留）

中间：`PDF 预览多 Tab`
- 继续保留多 Tab 预览
- dirty Tab 关闭按钮：`● / ×`

底部：`StatusBar`
- 文件位置、页码、缩放、状态消息

---

## 3. 折叠菜单拆分

### 3.1 编辑当前PDF（新增）
用于“原地修改当前 Tab 对应文档”，所有操作结果直接写回当前会话（先临时文件再替换）。

包含子功能：
1. 合并进当前PDF
- 选择外部 PDF
- 选择插入位置（前/后/指定页）
- 操作：`预览` / `应用`

2. 删除页
- 支持输入页码或从缩略图多选
- 操作：`预览` / `应用`

3. 交换页
- 输入 A 页 / B 页
- 操作：`预览` / `应用`

4. 提取图片
- 页范围筛选
- 图片列表预览（尺寸、格式）
- 导出选中图片/导出全部

行为规则：
- `应用`后：当前 Tab 标记 dirty
- 每次应用进入 Undo/Redo 栈
- Close Tab / Close Window 触发保存确认

### 3.2 传统操作（保留）
保留现有流程：
- Merge / Delete / Insert / Replace / PDF2DOCX
- 输出到新文件
- 可继续使用“预览结果（临时PDF）”

### 3.3 批注与书签（新增）
1. 画笔/荧光笔
- 工具切换：Pen / Marker / Eraser
- 参数：颜色、线宽、透明度
- 操作：撤销、重做、清空当前页

2. 书签
- 添加书签（当前页）
- 书签树重命名、删除、拖拽排序
- 点击书签跳转页

行为规则：
- 批注/书签变更都计入当前 Tab Undo/Redo
- 改动后统一 mark dirty

---

## 4. 技术实现方案

## 4.1 UI 容器重构
当前：`tool_tabs_ = QTabWidget`
目标：`QScrollArea + CollapsibleSection`

建议新增组件：
- `ui/collapsible_section.hpp/.cpp`
  - Header 区（标题 + 展开箭头）
  - Body 区（现有表单内容）
  - 支持单开/多开策略（默认多开）

迁移策略：
- 复用现有 `BuildMergePage/BuildDeletePage/...` 返回的 QWidget
- 包装进对应 Section，避免一次性大改业务逻辑

## 4.2 文档会话模型（原地编辑）
每个预览 Tab 引入 `DocumentSession` 元信息：
- `current_path`
- `save_path`
- `is_dirty`
- `undo_stack` / `redo_stack`
- `preview_temp_files`

现阶段已有 tab property（is_dirty/save_path/preview_temp_path），后续建议升级为显式结构体并挂到 `PdfTabView`。

## 4.3 Undo/Redo 设计
采用命令模式：
- `EditCommand` 抽象：`apply()` / `revert()`
- 具体命令：
  - MergeIntoCurrentCommand
  - DeletePagesCommand
  - SwapPagesCommand
  - AnnotationCommand
  - BookmarkCommand

每个命令保存：
- before_pdf_path（快照）
- after_pdf_path（结果）

执行路径：
- apply: backend 产出 after，Tab 加载 after
- undo: 加载 before
- redo: 再加载 after

## 4.4 后端接口扩展
在 `native-lib` 侧补能力：
- swap-pages
- extract-images（列表 + 导出）
- 注释写入（ink/highlight）
- 书签写入（add/update/remove/reorder）

原则：
- 原地编辑流程实际写“新临时文件”，然后替换当前会话指针
- Save 时再固化到用户目标路径

---

## 5. 里程碑

### M1（UI 架构迁移）
- [ ] 侧栏改为折叠菜单容器
- [ ] 迁移现有“传统操作”到折叠菜单
- [ ] 保留当前日志区与预览区

### M2（编辑当前PDF MVP）
- [ ] 新增“编辑当前PDF”折叠菜单
- [ ] 支持原地：合并进来、删除页、交换页
- [ ] 接入 mark dirty + Save/Save As
- [ ] 接入 Undo/Redo（至少 3 类命令）

### M3（提取图片 + 书签）
- [ ] 提取图片预览与导出
- [ ] 书签树 + 基础编辑
- [ ] 书签改动写回 PDF

### M4（画笔/标记）
- [ ] Overlay 标注（Pen/Marker）
- [ ] 橡皮擦、撤销重做
- [ ] 批注写回 PDF

---

## 6. 验收标准

1. 交互
- 用户不再需要切换工具 Tab；所有功能在折叠区展开操作

2. 原地编辑
- 编辑当前PDF可完成：合并进来、删除页、交换页、提取图片
- 每次应用后当前 Tab 显示 dirty 圆点

3. 保存与关闭
- File 菜单 Save/Save As 可保存当前会话
- 关闭 dirty tab 弹：保存 / 丢弃 / 取消

4. 可回退
- Undo/Redo 覆盖原地编辑与批注/书签

5. 兼容性
- 传统操作不回归（原有输出新文件流程仍可用）

---

## 7. 风险与对策
- 风险：UI 改造大，回归面广
  - 对策：先迁移容器，不改业务逻辑；分步替换
- 风险：Undo/Redo 占用磁盘
  - 对策：快照数量上限 + LRU 清理
- 风险：批注/书签写回兼容性
  - 对策：先做外部阅读器回归集（Acrobat/Chrome/Okular）
