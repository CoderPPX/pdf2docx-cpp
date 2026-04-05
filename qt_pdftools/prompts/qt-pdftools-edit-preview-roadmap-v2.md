# Qt PDFTools 编辑 Preview Roadmap (v2)

## 本次新增（在 v1 基础上）
- 为 preview 结果 Tab 增加 `[Preview]` 标记（示例：`[Preview] merge #12`）
- 关闭单个 Tab / 关闭全部 Tab / 关闭窗口时，自动删除对应 preview 临时 PDF

## 实现细节
1. Tab 标记
- `OpenPdfTab(...)` 扩展参数：`custom_title` + `is_preview_temp`
- preview 成功时，以 `OpenPdfTab(result.output_path, true, "[Preview] ...", true)` 打开
- 非 preview 保持原行为

2. 临时文件自动清理
- 新增 `CleanupPreviewTempForWidget(QWidget*)`
- 通过 tab 属性记录预览临时文件路径：`preview_temp_path`
- 触发清理时机：
  - `tabCloseRequested`
  - `CloseCurrentPdfTab()`
  - `CloseAllPdfTabs()`
  - `closeEvent()`（窗口退出兜底）

3. 安全性
- 清理函数是幂等实现（不存在文件也可安全返回）
- 清理完成后重置 tab 的 preview 属性，避免重复误删

## 关键代码
- `qt_pdftools/include/qt_pdftools/app/main_window.hpp`
- `qt_pdftools/src/app/main_window.cpp`

## 已完成状态
- [x] Preview Tab 加标签
- [x] 临时预览文件自动清理
- [x] 编译通过
- [x] offscreen 启动/退出 smoke test 通过

## 下一步（v3 建议）
- [ ] 菜单新增 `Close All Preview Tabs`
- [ ] Preview Tab tooltip 增加源参数摘要（page/at/source-page）
- [ ] 增加清理日志级别区分（INFO vs WARN）
