# `pdftools` Qt6 GUI 实施记录（V1）

- 更新时间：`2026-04-04`
- 实施目录：`/home/zhj/Projects/pdftools/cpp_pdftools_gui`
- 目标状态：`P0 GUI 全部功能已实现并通过自动化测试`

---

## 1. 本次完成内容（模块级）

## M-GUI-00：工程初始化与构建集成

已完成：
1. 新建独立工程 `cpp_pdftools_gui`。
2. 新增 `CMakeLists.txt` 与 `CMakePresets.json`。
3. 使用 `Qt6::Widgets + Qt6::Concurrent + Qt6::Test`。
4. 通过 `add_subdirectory(../cpp_pdftools ...)` 进程内复用 `pdftools::core`。
5. 新增 GUI 测试开关：`PDFTOOLS_GUI_ENABLE_TESTS`。

## M-GUI-01：核心服务层与任务并发层

已完成：
1. `PdfToolsService`：封装全部业务 API 调用与结果摘要。
2. `TaskManager`：异步提交、完成回调、任务状态广播。
3. `TaskManager` 增强：支持空 service 自动兜底，且增加已完成任务保留上限（默认 200），避免长时间运行时任务记录无限增长。
4. `TaskManager` 并发增强：支持队列状态（`Queued`）与可配置最大并发数（基于 `QThreadPool`）。
5. `ValidationService`：统一参数检查（路径、页号、覆盖逻辑）。
6. `SettingsService`：`QSettings` 持久化（窗口状态/最近参数）。

## M-GUI-02：模型与通用控件层

已完成：
1. `TaskItemModel`（`QAbstractTableModel`）用于任务中心展示。
2. `FileListEditor`：列表增删/排序。
3. `PathPicker`：文件/目录统一选择控件。
4. `TaskCenterWidget`：任务表格 + 状态过滤 + 输出路径打开。

## M-GUI-03：6 个业务页面（P0）

已完成：
1. `MergePage`（PDF 合并）
2. `TextExtractPage`（文本提取，含 strict/best-effort）
3. `AttachmentsPage`（附件提取，含 strict/best-effort）
4. `ImageToPdfPage`（图片转 PDF，含尺寸/缩放策略）
5. `PageEditPage`（删除/插入/替换页）
6. `Pdf2DocxPage`（PDF->DOCX，支持 dump-ir/no-images/anchored/font-fallback）

## M-GUI-04：主窗口与信息架构

已完成：
1. `MainWindow`：左侧导航 + 中央页面栈 + 底部任务中心 Dock。
2. 页面全部走后台任务，不阻塞 UI。
3. 任务完成后状态栏提示 + 页面结果摘要。

## M-GUI-05：最近参数可复用（P0 补齐）

已完成：
1. `PathPicker` 增加 suggestions/completer，支持直接复用历史路径输入。
2. 6 个业务页面均支持：
   - 启动时加载最近输入/输出参数；
   - 提交时持久化当前参数；
   - 部分页恢复最近选项状态（strict、overwrite、scale、tab index 等）。
3. `SettingsService` 增加 `RecentValue` 快捷接口。
4. `TaskCenterWidget` 增加空模型保护分支，避免极端时序下空指针风险。

## M-GUI-06：设置页接入

已完成：
1. 新增 `SettingsPage`，并接入主导航：
   - 完成任务保留上限（可配置）；
   - 清空最近参数历史；
   - 清空任务历史记录；
   - 重置窗口布局状态（重启生效）。
2. `TaskManager` 增加运行时配置接口：
   - `SetMaxCompletedTasks`；
   - `MaxCompletedTasks`；
   - `RetainedTaskCount`（诊断/测试可用）。
3. `SettingsService` 增加设置清理能力：
   - `ClearByPrefix`；
   - `AllKeys`；
   - `RemoveKey`。

## M-GUI-07：任务中心可观测性增强

已完成：
1. 任务中心新增详情面板（Summary/Detail/Output Path）。
2. 选择任务时即时展示详情，任务状态更新时自动刷新详情面板。
3. 详情面板可用于快速定位失败原因，不必依赖外部日志。
4. “Open Output” 增强：当目标文件不存在时自动回退打开父目录，提升失败场景可用性。

## M-GUI-08：GUI 自动化测试扩展

已完成：
1. 新增 `task_center_widget_test`，覆盖任务选择后详情面板渲染。
2. 新增 `settings_page_test`，覆盖设置页按钮行为与设置项持久化。
3. 所有 GUI 测试统一加 `QT_QPA_PLATFORM=offscreen`，保证 headless 环境稳定执行。

## M-GUI-09：任务队列并发控制（P1）

已完成：
1. `TaskManager` 提交后先进入 `Queued` 状态，再在 watcher 启动时切换到 `Running`。
2. 任务执行切换为 `QtConcurrent::run(&thread_pool_, ...)`，由线程池控制全局并发。
3. 新增并发配置接口：
   - `SetMaxConcurrentTasks`；
   - `MaxConcurrentTasks`。
4. 设置页新增“最大并发任务数”配置项，并持久化到 `settings/max_concurrent_tasks`。

## M-GUI-10：失败任务报告导出（P1）

已完成：
1. 任务中心新增 `Export Report` 按钮，可将选中任务导出为文本报告。
2. 报告包含任务核心诊断字段：
   - `TaskId/Operation/Status/SubmittedAt/FinishedAt/OutputPath`；
   - `Summary/Detail`。
3. 支持配置报告目录（默认落在应用数据目录下的 `task_reports`）。
4. 记录最近一次导出路径，便于测试与后续“打开报告文件”扩展。

## M-GUI-11：任务历史持久化（P1）

已完成：
1. `SettingsService` 新增任务历史读写接口：
   - `WriteTaskHistory`；
   - `ReadTaskHistory`；
   - `ClearTaskHistory`。
2. `TaskItemModel` 增加快照/批量替换接口：
   - `SnapshotTasks`；
   - `ReplaceAllTasks`。
3. 主窗口生命周期接入：
   - 启动时自动恢复历史任务列表；
   - 退出时自动持久化最终态任务（成功/失败）。
4. 历史任务上限复用 `TaskManager` 保留上限策略，避免设置分裂。

## M-GUI-12：历史管理闭环（P1）

已完成：
1. 设置页新增 `clear_task_history_button`，可一键清空已持久化任务历史。
2. 设置项反馈文案与当前行为保持一致，避免“清空 recent”与“清空任务历史”混淆。
3. 与 `M-GUI-11` 联动后，任务历史支持“可恢复 + 可清理”的完整生命周期。

## M-GUI-13：任务中心过滤增强（P1）

已完成：
1. 任务中心过滤器新增 `Queued` 维度，便于定位队列拥塞问题。
2. 过滤器选项顺序更新为：
   - `All` / `Queued` / `Running` / `Succeeded` / `Failed`。
3. 保持与 `TaskManager` 状态机一致，避免“有队列状态但无法筛选”的观察盲区。

## M-GUI-14：排队任务取消（P1）

已完成：
1. `TaskManager` 新增 `CancelTask(task_id)`，支持取消 `Queued` 状态任务。
2. 任务状态机新增 `Canceled` 终态，UI 可见、可过滤。
3. 任务中心新增 `Cancel Task` 按钮（只对排队任务生效）。
4. 取消后立即发出 `TaskChanged/TaskCompleted`，并写入完成队列（保留上限策略仍生效）。

## M-GUI-15：任务历史即时清理（P1）

已完成：
1. `TaskManager` 新增 `ClearRetainedTasks()`，用于清理内存中的终态任务记录。
2. `TaskItemModel` 新增 `RemoveFinalTasks()`，支持 UI 层即时移除已完成/失败/取消任务。
3. `SettingsPage::ClearTaskHistory` 升级为：
   - 清理持久化历史；
   - 清理 `TaskManager` 内存历史；
   - 发出 `TaskHistoryCleared` 信号。
4. `MainWindow` 接收 `TaskHistoryCleared` 后即时刷新任务中心，不需重启生效。

## M-GUI-16：任务中心按钮状态联动（P1）

已完成：
1. 任务中心新增动作可用性联动逻辑：
   - 未选择任务时，`Cancel/Export/Open` 全禁用；
   - 选择任务后，`Export/Open` 启用；
   - `Cancel` 仅在 `Queued` 任务上启用。
2. 任务状态更新（`dataChanged`）和选择变化时，动作状态自动刷新。
3. 降低误操作概率（避免对已完成任务误触取消）。

## M-GUI-17：取消终态过滤回归（P1）

已完成：
1. 增加 `Canceled` 过滤回归测试，确保“取消任务 -> 终态筛选”链路稳定。
2. 过滤组合状态覆盖 `Queued/Canceled` 两条新增分支，防止后续索引调整引入回归。

## M-GUI-18：报告目录设置化（P1）

已完成：
1. 设置页新增“任务报告目录”配置项（目录选择器）。
2. `Apply` 时持久化 `settings/report_directory`，并发出 `ReportDirectoryChanged(path)`。
3. 主窗口启动时加载报告目录设置，实时同步到任务中心导出逻辑。
4. 补充修正：任务历史终态判定已纳入 `Canceled`，避免取消任务在持久化时漏存。

## M-GUI-19：页面编辑页数预览（P1）

已完成：
1. `cpp_pdftools` 新增 `GetPdfInfo` API（返回 `page_count`），用于轻量读取 PDF 页数。
2. `PageEditPage` 新增三处即时页数信息展示：
   - 输入 PDF 信息；
   - 插入来源 PDF 信息；
   - 替换来源 PDF 信息。
3. 当路径变化时自动刷新标签，成功显示页数，失败显示可读错误信息。
4. 补充对象命名，便于自动化测试稳定定位 UI 控件。

## M-GUI-20：PDF Info CLI 接入（P1）

已完成：
1. `pdftools` CLI 新增命令：
   - `pdftools pdf info --input <in.pdf>`
2. 输出格式：
   - `pdf info pages=<N>`
3. 该命令复用 `GetPdfInfo` 底层 API，保证 GUI 与 CLI 页数读取一致。

## M-GUI-21：页号输入范围联动（P1）

已完成：
1. 页面编辑页根据已读取页数自动更新输入范围：
   - 删除页号：`1..目标PDF页数`；
   - 替换页号：`1..目标PDF页数`；
   - 插入位置：`1..目标PDF页数+1`；
   - 来源页号：`1..来源PDF页数`。
2. 当 PDF 路径无效或未选择时，回退到默认上限，保证界面可继续编辑。
3. 通过范围联动减少“提交后才报页号越界”的低效交互。

## M-GUI-22：合并页总页数预估（P1）

已完成：
1. `MergePage` 新增输入摘要栏，实时显示：
   - 已选择文件数量；
   - 可读取文件的预计总页数；
   - 无法读取文件计数（如有）。
2. 摘要基于 `GetPdfInfo` 逐文件聚合，和页面编辑页信息来源一致。
3. 通过预估信息让用户在执行前快速判断合并结果规模。

## M-GUI-23：任务中心关键字搜索（P1）

已完成：
1. 任务中心新增搜索框（`summary/detail` 关键字匹配，大小写不敏感）。
2. 支持“状态过滤 + 关键字过滤”叠加，便于快速定位失败原因。
3. 修复边界行为：在 `All` 状态下关键字过滤同样生效。

## M-GUI-24：统一预览能力接入（P1）

已完成：
1. 新增 `PreviewService`（统一预览服务）：
   - PDF 页数查询；
   - PDF -> IR -> HTML 渲染；
   - 本地图片缩略图构建；
   - PDF 图片块缩略图提取。
2. `TextExtractPage` 接入“当前 PDF + 指定页预览”。
3. `PageEditPage` 接入“删除/插入/替换来源页预览”。
4. `ImageToPdfPage` 接入“输入图片缩略图预览 + 可读性计数”。
5. `Pdf2DocxPage` 接入“IR 渲染预览 + 提图缩略图预览”。
6. `MergePage` 接入“当前 PDF 预览”（文件选择 + 页码 + 刷新）。
7. UI 级错误处理统一为“可读提示 + 不崩溃”。

---

## 2. 兼容性修正（为 GUI 嵌入 `cpp_pdftools`）

为保证 `cpp_pdftools` 可作为子工程复用，已修复：
1. `cpp_pdftools/CMakeLists.txt`
   - `CMAKE_SOURCE_DIR` -> `CMAKE_CURRENT_SOURCE_DIR`
   - `CMAKE_BINARY_DIR` -> `CMAKE_CURRENT_BINARY_DIR`
2. `cpp_pdftools/cmake/Dependencies.cmake`
   - 依赖 `add_subdirectory` 输出目录从 `CMAKE_BINARY_DIR` 改为 `CMAKE_CURRENT_BINARY_DIR`

结果：
- `cpp_pdftools` 既可独立构建，也可被 `cpp_pdftools_gui` 正常嵌入。

---

## 3. 新增测试用例与验证结果

新增测试：
1. `tests/pdftools_service_test.cpp`
   - 用 fixture 验证 `ExtractText / Merge / DeletePage / InsertPage / ReplacePage / ExtractAttachments / ImagesToPdf / PdfToDocx`。
2. `tests/task_item_model_test.cpp`
   - 验证任务模型插入/更新与状态显示。
   - 验证任务快照与批量恢复顺序一致性。
   - 验证移除终态任务后仅保留活跃任务。
3. `tests/task_manager_test.cpp`
   - 验证异步任务提交与 `TaskCompleted` 信号链路。
   - 验证 `Queued` 初始状态与并发上限配置能力。
   - 验证排队任务取消后进入 `Canceled` 终态。
   - 验证 `ClearRetainedTasks` 对终态任务的清理行为。
4. `tests/settings_service_test.cpp`
   - 验证 recent 去重/上限逻辑与 last-directory 行为。
   - 验证按前缀清理设置项行为。
   - 验证任务历史序列化/反序列化与上限裁剪行为。
5. `tests/task_center_widget_test.cpp`
   - 验证任务中心详情面板内容更新。
   - 验证任务报告导出落盘及关键字段内容。
   - 验证 `Queued` 状态过滤行为。
   - 覆盖新增过滤项（含 `Canceled`）的可用性。
   - 验证按钮状态联动行为（按任务状态启停）。
   - 验证 `Canceled` 过滤行为。
6. `tests/settings_page_test.cpp`
   - 验证设置页应用任务保留上限、并发上限与清空 recent/任务历史功能。
   - 验证 `TaskHistoryCleared` 信号触发。
   - 验证报告目录配置持久化与 `ReportDirectoryChanged` 信号。
7. `tests/page_edit_page_test.cpp`
   - 验证页面编辑页在有效 PDF 上展示正确页数；
   - 验证无效路径展示错误提示；
   - 验证来源 PDF 页数信息同步刷新。
   - 验证页号/插入位/来源页号的最大值与检测页数联动。
8. `cpp_pdftools/tests/unit/m5_cli_test.cpp`
   - 验证 `pdf info` 子命令可用且输出 `pages=` 字段。
9. `tests/merge_page_test.cpp`
   - 验证合并页摘要可正确显示总页数；
   - 验证包含坏路径时可显示“无法读取”计数。
10. `tests/task_center_widget_test.cpp`
   - 验证关键字可匹配 `summary` 和 `detail`；
   - 验证清空关键字后恢复全量列表。
11. `tests/preview_service_test.cpp`
   - 验证 PDF 页数查询、IR 预览 HTML 产出、PDF 提图缩略图。
12. `tests/text_extract_page_test.cpp`
   - 验证输入 PDF 后页数标签、页码范围、指定页预览链路。
13. `tests/pdf2docx_page_test.cpp`
   - 验证 IR 预览与提图预览控件联动。
14. `tests/image_to_pdf_page_test.cpp`
   - 验证可读/不可读图片统计与缩略图刷新。
15. `tests/merge_page_test.cpp`
   - 新增“当前 PDF 预览”用例（刷新后 HTML 成功渲染）。

本地验证命令：

```bash
cd /home/zhj/Projects/pdftools/cpp_pdftools_gui
cmake --preset linux-debug
cmake --build --preset linux-debug -j8
ctest --preset linux-debug
```

结果：
- `12/12` 测试通过（Debug）。
- `12/12` 测试通过（Release）。

同时回归：

```bash
cd /home/zhj/Projects/pdftools/cpp_pdftools
cmake --preset linux-debug
cmake --build --preset linux-debug -j8
ctest --preset linux-debug
```

结果：
- `7/7` 测试通过。

---

## 4. 已实现但尚可继续增强的点（P1）

1. 任务取消：已支持排队任务取消；运行中任务仍未支持强制中断。
2. 任务进度：当前为阶段状态（开始/完成），后续可接入细粒度 progress。
3. 页面预览：已覆盖 IR 预览与缩略图预览；后续可增加像素级 PDF 渲染（非 IR）与缩放/拖拽。
4. 历史任务当前为“基础持久化”，尚未支持按条件检索、分页和导出。

---

## 5. 关键文件索引

1. 构建：
   - `cpp_pdftools_gui/CMakeLists.txt`
   - `cpp_pdftools_gui/CMakePresets.json`
2. 应用：
   - `cpp_pdftools_gui/src/app/main.cpp`
   - `cpp_pdftools_gui/src/app/main_window.cpp`
3. 服务层：
   - `cpp_pdftools_gui/src/services/pdftools_service.cpp`
   - `cpp_pdftools_gui/src/services/task_manager.cpp`
   - `cpp_pdftools_gui/src/services/validation_service.cpp`
   - `cpp_pdftools_gui/src/services/settings_service.cpp`
4. 页面层：
   - `cpp_pdftools_gui/src/pages/*.cpp`
5. 测试：
   - `cpp_pdftools_gui/tests/pdftools_service_test.cpp`
   - `cpp_pdftools_gui/tests/task_item_model_test.cpp`

---

## 6. 当前结论

`pdftools` 的 Qt6 GUI P0 范围已完成，具备可用的端到端能力：
- 6 个核心 PDF 功能均可在 GUI 发起；
- 统一任务中心可追踪结果；
- 已具备跨页面的 PDF/IR/图片预览能力；
- 已有自动测试与回归验证；
- 构建体系支持与 `cpp_pdftools` 一体化维护。
