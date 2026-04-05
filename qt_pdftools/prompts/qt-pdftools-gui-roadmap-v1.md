# Qt PDFTools GUI Roadmap（对齐 `web_pdftools`，V1）

- 更新时间：2026-04-05
- 目标目录：`qt_pdftools`
- 目标：参考 `web_pdftools` 的交互与能力，构建纯 C++/Qt6 桌面 GUI（优先走 native backend，放弃 wasm 作为首发依赖）

---

## 1. 目标与边界

1. 功能对齐 `web_pdftools`：
- 多 Tab PDF 预览
- 侧边栏工具（merge / delete-page / insert-page / replace-page / pdf2docx）
- File 菜单统一文件操作
- Settings（主题、backend、fallback）
- 任务日志与状态

2. 首发不做：
- 浏览器/wasm 运行时
- 云端同步与账号体系

3. 首发必须：
- backend 抽象层
- Native In-process backend（主路径）
- Native CLI backend（兜底路径）
- 统一错误模型（遵循 `prompts/exception_convention.md`）

---

## 2. 对齐 web 版的功能映射

1. 界面结构映射：
- web `menubar + sidebar + preview + footer` -> Qt `QMainWindow + QMenuBar + QDockWidget + central splitter + status bar`

2. 预览映射：
- web `pdf.js + canvas` -> Qt `QPdfDocument/QPdfView`（Qt6Pdf）
- 多文件预览：`QTabWidget`，每个 tab 维护独立文档状态（页码/缩放）

3. 工具面板映射：
- web 左侧表单 -> Qt `QStackedWidget` 页面（每个操作一个 page widget）
- “从已打开 tab 选择文件” -> 下拉框绑定当前打开 tab 列表

4. backend 映射：
- web `task_backends.mjs` -> Qt `ITaskBackend` 接口 + `NativeLibBackend` + `NativeCliBackend`
- wasm backend 在 Qt 版仅保留接口占位，不进入首发路径

5. 统计与日志映射：
- web bridge stats -> Qt 任务中心统计卡（成功率/耗时/fallback）
- web task log -> Qt `QPlainTextEdit` 日志面板

---

## 3. 技术架构（Qt 版）

## 3.1 分层

1. UI 层：
- `MainWindow`
- `PreviewTabWidget`
- `ToolPages`（merge/delete/insert/replace/pdf2docx）
- `SettingsDialog`
- `TaskCenterWidget`

2. 应用层：
- `TaskController`（参数校验、任务调度、日志聚合）
- `OpenTabsModel`（tab 与文件映射）
- `SettingsService`（QSettings）

3. backend 层：
- `ITaskBackend`
- `NativeLibBackend`（调用 `cpp_pdftools`）
- `NativeCliBackend`（调用 `pdftools` 二进制）

4. 公共层：
- `TaskRequest/TaskResult`
- `ErrorEnvelope`（`code/message/context/details`）
- `ErrorCodeMapper`（对齐 exception convention）

## 3.2 推荐目录

```text
qt_pdftools/
  CMakeLists.txt
  src/
    app/
      main.cpp
      MainWindow.h/.cpp
    ui/
      preview/
      tools/
      settings/
      task_center/
    core/
      TaskController.h/.cpp
      OpenTabsModel.h/.cpp
      SettingsService.h/.cpp
    backend/
      ITaskBackend.h
      NativeLibBackend.h/.cpp
      NativeCliBackend.h/.cpp
      BackendFactory.h/.cpp
    models/
      TaskTypes.h
      ErrorEnvelope.h
```

---

## 4. 分阶段实施计划

## 阶段 S0：工程脚手架与依赖接入（1-2 天）

1. 建立 Qt6 工程（Widgets + Concurrent + Test）
2. 接入 `cpp_pdftools`（优先 in-process）
3. 搭建最小主窗口与菜单骨架

交付物：
1. 可运行空窗口
2. `File` / `Settings` 菜单可响应

## 阶段 S1：预览与多 Tab（2-3 天）

1. 集成 `QPdfDocument/QPdfView`
2. 实现 `Open PDF in New Tab` / `Close Current` / `Close All`
3. 实现页码切换、缩放、当前路径显示

交付物：
1. 与 web 版同等多 Tab 预览能力

## 阶段 S2：工具侧边栏（3-4 天）

1. merge 表单页
2. delete-page 表单页
3. insert-page 表单页
4. replace-page 表单页
5. pdf2docx 表单页
6. 所有页面支持从打开 tab 选择输入

交付物：
1. UI 参数采集完整
2. 前置校验（必填、页号、输出路径）

## 阶段 S3：backend 抽象与 native 执行（3-4 天）

1. 实现 `ITaskBackend` 统一接口
2. `NativeLibBackend` 打通所有任务
3. `NativeCliBackend` 实现同参数执行
4. backend 状态检测与切换

交付物：
1. 任务可执行并返回结构化结果
2. 错误码/上下文统一输出

## 阶段 S4：任务中心、日志、fallback（2-3 天）

1. 任务运行状态（queued/running/succeeded/failed/canceled）
2. 日志面板追加 stdout/stderr/摘要
3. 自动 fallback 机制（仅在可回退错误码触发）
4. 统计卡（最近耗时、成功率、是否发生 fallback）

交付物：
1. 与 web 版近似的可观测性与回退体验

## 阶段 S5：Settings 与主题（1-2 天）

1. 主题模式（dark/light/system）
2. backend 默认选择
3. fallback 开关
4. 配置持久化（QSettings）

交付物：
1. 重启后设置可恢复

## 阶段 S6：测试与发布准备（2-3 天）

1. 单元测试：参数映射、错误映射、backend 工厂
2. 集成测试：merge/delete/insert/replace/pdf2docx 主路径
3. GUI 冒烟：打开文件、tab 流程、任务执行与回退
4. 打包脚本（Linux 优先，后续扩展 Win/macOS）

交付物：
1. 可交付安装包
2. 最小回归测试集

---

## 5. 错误处理与异常约束（必须遵守）

1. 统一错误模型：`code/message/context/details`
2. `TaskController` 只消费结构化错误，不直接拼接异常字符串
3. 所有 backend 错误最终映射为 exception convention 里的标准 code
4. UI 不直接崩溃，不把 C++ 异常暴露到事件循环

---

## 6. 测试用例清单（最小首发集）

1. 预览与 Tab：
- 打开 3 个 PDF，切换 tab，页码/缩放独立保存
- 关闭当前 tab / 关闭全部 tab

2. 文件菜单：
- File->Open 打开文件进入新 tab
- File->Settings 打开设置窗口

3. 任务执行：
- merge 成功（>=2 输入）
- merge 非法输入（1 输入）被前置拦截
- delete-page / insert-page / replace-page 成功
- pdf2docx 成功并可选 dump-ir

4. 错误路径：
- 输入文件不存在 -> `NOT_FOUND` / `IO_ERROR`
- 页号越界 -> `INVALID_ARGUMENT`
- backend 不可用 -> 状态红色 + 可读提示

5. fallback：
- 模拟 lib backend 返回可回退错误码 -> 自动切 CLI 成功
- fallback 关闭时不自动切换

6. 配置持久化：
- 主题、backend、fallback 开关重启后保持

---

## 7. 风险与规避

1. Qt PDF 预览兼容性差异：
- 规避：抽象 `IPdfPreviewAdapter`，必要时可切 Poppler

2. 长任务阻塞 UI：
- 规避：全部任务走后台线程池 + 信号回主线程

3. native/lib 与 cli 结果语义不一致：
- 规避：统一 `TaskResult` 适配层，做字段标准化

4. 大文件内存压力：
- 规避：预览延迟加载，任务 IO 流式化，结果日志限长

---

## 8. 里程碑（建议）

1. M1（S0-S1 完成）：可用预览壳子
2. M2（S2-S3 完成）：核心功能全部可跑
3. M3（S4-S5 完成）：可观测性与设置闭环
4. M4（S6 完成）：可发布版本

---

## 9. 下一步执行建议（给下一个 LLM）

1. 先落 `S0/S1`：生成 Qt 工程与多 Tab 预览最小可运行版本。
2. 同步创建 `ITaskBackend` 与 `TaskRequest/TaskResult`，避免后续重构。
3. 优先打通 `merge` 一条完整链路（UI -> backend -> 日志 -> 预览回显）后再批量扩展其余操作。
