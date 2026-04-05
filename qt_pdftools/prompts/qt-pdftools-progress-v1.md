# qt_pdftools 进展（v1）

更新时间：2026-04-05

## 已完成

1. 新建 `qt_pdftools` Qt6 工程并可编译运行。
2. 按要求仅保留 `native-lib` 后端（不启用 `native-cli`）。
3. 实现 Web 风格主界面结构：
   - File 菜单
   - 左侧工具栏（5 个操作页）
   - 主区域 PDF 多 Tab 预览
   - 底部预览控制（上一页/下一页/缩放/页码）
   - 任务日志
4. 接入 `Qt6Pdf`：
   - `QPdfDocument + QPdfView`
   - Tab 内独立页码/缩放状态
5. 实现 5 个任务链路（in-process）：
   - merge
   - delete-page
   - insert-page
   - replace-page
   - pdf2docx
6. 实现基础 Settings：
   - 主题（system/dark/light）
   - `QSettings` 持久化
7. 增加轻量任务中心表格：
   - 时间/任务/状态/输出
   - Running/Succeeded/Failed 状态可见
7. 补充项目文档：
   - `qt_pdftools/README.md`
   - `qt_pdftools/.gitignore`
   - `qt_pdftools/CMakePresets.json`

## 构建与验证

1. Configure（preset）：
```bash
cmake --preset linux-debug
```
2. Build：
```bash
cmake --build --preset linux-debug -j8
```
3. Offscreen 启动烟测：
```bash
timeout 3s env QT_QPA_PLATFORM=offscreen ./qt_pdftools/build/linux-debug/qt_pdftools
```
说明：超时码 `124` 为预期（GUI 进入事件循环），无崩溃。

## 当前限制

1. 当前仅实现轻量任务中心表格，尚未接入完整队列模型（queued/canceled/历史筛选）。
2. 当前未实现自动 fallback（因已按要求移除 `native-cli`）。
3. 页面的“参数模板/最近使用项”尚未接入。

## 下一步（建议）

1. 增加任务中心模型（状态、耗时、输出路径、错误详情）。
2. 为 5 个工具页补充更严格参数校验与错误展示。
3. 补充 QtTest：
   - backend 调用单测
   - 主窗口基本交互冒烟
4. 增加“从 Tab 反填输出建议”的细节优化（与 web 行为更一致）。
