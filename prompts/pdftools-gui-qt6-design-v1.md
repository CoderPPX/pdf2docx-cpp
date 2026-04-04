# `pdftools` GUI（C++/Qt6）设计说明（V1）

- 更新时间：`2026-04-03`
- 目标：在现有 `cpp_pdftools` 库基础上提供可商用的 Qt6 GUI 客户端
- 核心原则：同一套业务能力复用 `pdftools` 库，不重复实现 PDF 逻辑

---

## 1. 你需要的 GUI 功能（按优先级）

## 1.1 P0（首发必须）

1. PDF 合并（`merge`）
2. PDF 文本提取（`text extract`，支持 `strict/best-effort`）
3. PDF 附件提取（`attachments extract`，支持 `strict/best-effort`）
4. 图片转 PDF（`image2pdf`，展示 `skipped_images`）
5. 页面操作（`page delete / insert / replace`）
6. PDF -> DOCX（`convert pdf2docx`，支持 `dump-ir`、`no-images`、`anchored`）
7. 统一任务中心（进度、状态、错误、结果路径）
8. 参数模板保存（最近使用参数、最近文件）

## 1.2 P1（增强）

1. PDF 页面缩略图预览（插入/替换页前可视化）
2. 批量任务队列（多任务串行或并发）
3. 结果面板一键打开输出目录
4. 错误报告导出（便于用户反馈）

## 1.3 P2（后续）

1. 任务历史持久化
2. 多语言切换
3. 暗色主题/自定义主题
4. 插件式功能扩展页

---

## 2. GUI 信息架构（界面怎么设计）

推荐采用 `QMainWindow + 左侧导航 + 中央工作区 + 底部任务中心`：

1. 左侧导航（固定）
- 合并
- 文本提取
- 附件提取
- 图片转 PDF
- 页面编辑
- PDF 转 DOCX
- 设置

2. 中央工作区（按模块切换）
- 每个模块是一个独立 `QWidget` 页面
- 统一布局：输入区 -> 参数区 -> 输出区 -> 执行按钮 -> 结果摘要

3. 底部任务中心（`QDockWidget`）
- 显示任务列表、进度条、状态、耗时、错误简述
- 支持过滤：全部/成功/失败/运行中

4. 右侧可选信息栏（P1）
- 参数帮助说明
- 常见错误解释

---

## 3. 每个页面的交互设计（字段级）

## 3.1 合并页（MergePage）

字段：
1. 输入 PDF 列表（可拖拽排序）
2. 输出路径
3. 覆盖开关（`overwrite`）

按钮：
1. 添加文件
2. 删除文件
3. 上移/下移
4. 开始合并

结果显示：
1. 输出页数（`output_page_count`）
2. 输出文件路径

## 3.2 文本提取页（TextExtractPage）

字段：
1. 输入 PDF
2. 输出路径
3. 输出格式（txt/json）
4. 页范围（start/end，可选）
5. `best-effort` 开关（默认开）
6. `strict` 开关（和 `best-effort` 互斥）

结果显示：
1. `pages`
2. `entries`
3. `extractor`（`podofo`/`pdftotext`）
4. `fallback`（yes/no）

## 3.3 附件提取页（AttachmentsPage）

字段：
1. 输入 PDF
2. 输出目录
3. `best-effort` / `strict`

结果显示：
1. 提取附件数
2. `parser`
3. `parse_failed`
4. 附件列表（文件名、大小、路径）

## 3.4 图片转 PDF 页（ImageToPdfPage）

字段：
1. 图片列表（支持拖拽排序）
2. 输出 PDF
3. 页面策略：`use image size` / 固定页面尺寸
4. 缩放策略：`fit` / `original`
5. 边距

结果显示：
1. `page_count`
2. `skipped_image_count`
3. 如果 `skipped_image_count > 0`，显示黄色提示

## 3.5 页面编辑页（PageEditPage）

Tab 设计：
1. 删除页 Tab
2. 插入页 Tab
3. 替换页 Tab

共有字段：
1. 输入 PDF
2. 输出 PDF
3. 页号（1-based）

插入/替换额外字段：
1. 来源 PDF
2. 来源页号（1-based）

结果显示：
1. 输出页数

## 3.6 PDF 转 DOCX 页（Pdf2DocxPage）

字段：
1. 输入 PDF
2. 输出 DOCX
3. `dump-ir` 输出路径（可选）
4. `extract_images` 开关
5. `use_anchored_images` 开关
6. `enable_font_fallback` 开关

结果显示：
1. `page_count`
2. `image_count`
3. `warning_count`
4. 后端信息（`backend`）

---

## 4. 使用 `pdftools` 库的方式（重点）

## 4.1 推荐模式：进程内库调用（In-process）

原因：
1. 调用开销小
2. 结果结构化（`Request/Result/Status`）
3. 与 CLI 参数解析解耦

调用路径：
1. GUI 页面收集参数
2. 转换为 `pdftools` 的 `Request`
3. 在后台线程执行 `Status Xxx(request, &result)`
4. 主线程显示 `result` 或 `status`

直接调用的核心接口：
1. `pdftools::pdf::MergePdf`
2. `pdftools::pdf::ExtractText`
3. `pdftools::pdf::ExtractAttachments`
4. `pdftools::pdf::ImagesToPdf`
5. `pdftools::pdf::DeletePage/InsertPage/ReplacePage`
6. `pdftools::convert::ConvertPdfToDocx`

## 4.2 备用模式：子进程调用 CLI（Out-of-process）

适用场景：
1. 需要更强隔离（异常 PDF 不影响 GUI 主进程）
2. 快速集成原型

缺点：
1. 输出解析复杂
2. 性能稍差
3. 难以拿到完整结构化结果

建议：
1. 正式版优先 In-process
2. 增加一个“兼容执行器”保留 CLI 兜底能力

---

## 5. Qt6 代码架构（类怎么设计）

## 5.1 目录建议

```text
cpp_pdftools_gui/
  CMakeLists.txt
  src/
    app/
      main.cpp
      MainWindow.*
    pages/
      MergePage.*
      TextExtractPage.*
      AttachmentsPage.*
      ImageToPdfPage.*
      PageEditPage.*
      Pdf2DocxPage.*
    services/
      PdfToolsService.*
      TaskManager.*
      SettingsService.*
      ValidationService.*
    models/
      TaskItemModel.*
      RecentFilesModel.*
    widgets/
      FileListEditor.*
      OutputPathPicker.*
      TaskCenterWidget.*
```

## 5.2 核心类职责

1. `PdfToolsService`
- 纯业务调用层
- 对外提供 `runMerge/runTextExtract/...`
- 内部只做 `Request` 组装与 `pdftools` API 调用

2. `TaskManager`
- 任务调度与状态管理
- 将后台结果通过 signal 发回 UI
- 可基于 `QThreadPool + QtConcurrent` 或 `QFutureWatcher`

3. `ValidationService`
- 做参数合法性检查（文件存在、输出目录可写、页号范围）
- 在执行前给出友好错误

4. `SettingsService`
- 用 `QSettings` 保存最近配置/窗口状态/主题

5. `TaskItemModel`
- `QAbstractTableModel`，用于任务中心表格

6. `MainWindow`
- 负责页面切换，不负责业务执行

## 5.3 页面与服务边界

页面只做三件事：
1. 收集参数
2. 调用 `TaskManager.submit(...)`
3. 渲染状态

页面不要直接接触 PoDoFo 或 PDF 细节。

---

## 6. 线程与任务设计（GUI 不阻塞）

## 6.1 基本原则

1. 任何 PDF 操作都在后台线程执行
2. UI 主线程只收信号更新界面
3. 每个任务有唯一 `taskId`

## 6.2 推荐事件流

1. 页面发起任务 -> `TaskManager::submit(OperationType, RequestVariant)`
2. `TaskManager` 在后台执行 `PdfToolsService`
3. 发射信号：
- `taskStarted(taskId)`
- `taskProgress(taskId, percent, stage)`（当前可先做阶段进度）
- `taskFinished(taskId, success, summary)`

## 6.3 取消策略说明

现状：
1. `pdftools` 的具体 PDF 操作接口目前是同步函数，细粒度取消点有限。

建议：
1. GUI 的“取消”先实现为：
- 队列中任务可取消（未启动）
- 运行中任务标记“请求取消”，下一个阶段生效
2. 后续若要强取消：
- 在 `pdftools` 操作接口中加入 `RuntimeContext` 或 `CancelToken` 参数

---

## 7. 参数映射（GUI -> 库）

## 7.1 文本提取

GUI 字段映射：
1. 严格模式勾选 -> `ExtractTextRequest.best_effort = false`
2. 未勾选严格 -> `best_effort = true`
3. JSON 输出勾选 -> `output_format = kJson`

UI 结果渲染：
1. `extractor` 直接显示在结果卡片
2. `used_fallback=true` 时显示“已启用降级提取”

## 7.2 附件提取

GUI 字段映射：
1. 严格模式 -> `best_effort=false`

UI 结果渲染：
1. `parse_failed=true` 时显示“解析失败，已容错返回空结果”

## 7.3 图片转 PDF

GUI 字段映射：
1. “按原图尺寸建页” -> `use_image_size_as_page=true`
2. 缩放策略下拉 -> `scale_mode`

UI 结果渲染：
1. 显示 `skipped_image_count`
2. 当 >0 时给出可点击详情（哪些图片被跳过）

---

## 8. 可用性细节（非常关键）

1. 所有输入路径支持拖拽
2. 输出路径默认自动建议（基于输入名）
3. 执行按钮旁边显示最近一次执行耗时
4. 出错时弹窗 + 可复制错误详情
5. 任务完成后提供：
- “打开文件”
- “打开目录”
- “复制命令等价参数”

---

## 9. 测试策略（GUI 与库协同）

## 9.1 单元测试

1. `ValidationService` 参数校验测试
2. `PdfToolsService` 的请求映射测试
3. `TaskManager` 状态机测试

## 9.2 集成测试

1. 用固定样本跑 6 大页面流程
2. 校验结果文件存在性与关键字段
3. 增加异常样本：
- 非 PDF 输入（strict/best-effort 对比）
- 损坏图片混入批量

## 9.3 回归测试

1. 复用你已有公开测试集批量脚本
2. 每个版本发布前执行一次“最小全链路”

---

## 10. 分阶段落地建议（建议你这样做）

## Phase A（1~2 周）

1. 完成主窗口 + 6 个页面静态布局
2. 接通 `Merge`、`TextExtract`、`Pdf2Docx`
3. 接通任务中心最小版

## Phase B（1~2 周）

1. 接通 `Attachments`、`Image2Pdf`、`PageEdit`
2. 完成参数校验与错误提示
3. 完成设置持久化

## Phase C（1 周）

1. 加入缩略图与高级交互
2. 做批量任务体验优化
3. 完成发布包与文档

---

## 11. 你现在就可以直接用的集成建议

1. GUI 项目直接链接 `pdftools::core` 静态库
2. 页面执行统一走 `PdfToolsService`
3. `strict/best-effort` 在界面明确提供开关
4. 结果卡片必须展示：
- 任务状态
- 输出路径
- 关键统计（pages/images/warnings/skipped/fallback）

这样可以保证 GUI 与 CLI 行为一致，后续维护成本最低。

