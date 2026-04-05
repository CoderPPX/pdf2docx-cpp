# Qt PDFTools 当前进度总结 (v3)

## 1. 当前已完成

### 1.1 基础工程与后端
- 已建立 `qt_pdftools` 工程，使用 Qt6 + native-lib backend
- 已接通核心操作：
  - merge
  - delete-page
  - insert-page
  - replace-page
  - pdf2docx

### 1.2 主界面与交互
- File 菜单已实现：Open/Close/CloseAll/Settings/Exit
- 后续已补：Save / Save As
- PDF 预览区支持多 Tab
- 预览控制（路径、翻页、缩放）已迁移到 status bar
- 预览渲染已调整为多页模式（不再单页限制）

### 1.3 日志与状态
- 侧栏“运行状态”已删除
- 日志统一输出到“日志”面板
- 日志支持彩色等级显示（INFO/TASK/OK/WARN/ERROR）

### 1.4 预览任务能力（编辑操作）
- merge/delete/insert/replace 已支持“预览”按钮
- preview 走统一任务链路（preview_only）
- 预览结果写入临时 PDF 并自动打开新 Tab
- 预览 Tab 已加 `[Preview]` 标记
- 关闭预览 Tab / 关闭窗口可自动清理预览临时文件

### 1.5 稳定性与会话体验
- 修复了窗口关闭阶段的 segfault 风险点（对象生命周期与信号处理）
- 文件对话框已记忆上次目录（跨会话）

### 1.6 Dirty 相关（刚完成）
- Tab 已具备 dirty 显示机制：
  - clean: `×`
  - dirty: `●`
- 关闭 dirty tab 时弹窗：
  - 保存
  - 丢弃修改
  - 取消关闭
- File > Save / Save As 已可对当前 Tab 生效

---

## 2. 当前尚未完成

### 2.1 侧栏仍是工具 Tab 模式
- 目前工具区仍由 `QTabWidget` 切换
- 尚未迁移到“折叠菜单（Accordion）”模式

### 2.2 原地编辑能力仍不完整
- 已有 preview-only + 保存基础
- 但“编辑当前PDF”完整工作流尚未单独成模块
- 尚未完成：
  - 交换页（native-lib 实做）
  - 提取图片（浏览+导出）
  - 原地编辑命令化 Undo/Redo

### 2.3 画笔/书签尚未落地
- 目前只有方案文档
- 尚未实现 Overlay 标注、书签树与写回

---

## 3. 与你最新需求的差距
你要求：
1. 工作区改为“每个功能一个折叠菜单”
2. 新增折叠菜单“编辑当前PDF”（原地合并/删页/换页/提图）
3. 加入画笔/书签

差距评估：
- UI 架构迁移：未开始（仅有 roadmap）
- 编辑当前PDF：部分基础就绪（dirty/save/preview），功能整合未完成
- 画笔/书签：未开始编码

---

## 4. 下一步优先级建议
1. 先做 UI 容器迁移（Tab -> Accordion），保持原功能可用
2. 落地“编辑当前PDF”MVP（合并进来/删除页/交换页）
3. 接入 Undo/Redo 栈（命令模式）
4. 再做提取图片
5. 最后接入画笔/书签

这样能在最短路径上先交付可用的“原地编辑”主流程。
