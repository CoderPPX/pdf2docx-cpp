# `cpp_pdftools` 模块级详细设计说明（V1）

- 更新时间：`2026-04-03`
- 适用目录：`/home/zhj/Projects/pdftools/cpp_pdftools`
- 目标：把“PDF 工具能力 + 现有 `pdf2docx`”统一到一套可扩展、可测试、同时支持 CLI/GUI 的架构中。

---

## 1. 总体分层与协作关系

建议采用四层结构，所有功能都遵循同一执行路径：

1. `core`（基础层）
- 放置状态码、错误模型、日志、配置、路径工具、时间统计、取消控制。

2. `runtime`（运行时编排层）
- 放置命令注册、任务调度、进度事件、同步/异步桥接。
- 负责把“一个业务请求”路由到具体模块，不做 PDF 细节处理。

3. `domain`（能力层）
- 放置具体功能模块：文档编辑、文本提取、附件提取、图片转 PDF、PDF->DOCX。
- 只依赖抽象后端接口，不直接依赖 CLI/GUI。

4. `backend`（底层实现层）
- 默认实现 `PoDoFo` 后端，提供页面遍历、内容流读取、对象写入、保存等基础能力。
- 后续新增 Poppler/PDFium 时，只替换 backend 实现，不改 domain 接口。

统一调用链：
`CLI/GUI -> CommandRegistry/TaskRunner -> Domain Service -> Backend Adapter -> Output`

---

## 2. 全局接口规范（所有模块必须遵守）

## 2.1 请求/结果模型

每个模块都采用 `Request/Result`：
- `Request`：输入路径、输出路径、参数选项、执行策略（覆盖/并发/日志级别）。
- `Result`：是否成功、统计信息（页数/对象数/耗时）、warning 列表、产物路径。

这样做的目的：
- CLI 和 GUI 共用同一业务接口。
- 测试可以直接构造 `Request`，不依赖命令行解析。

## 2.2 统一返回

统一返回 `Status`：
- 成功：`ok + 可选 warning`
- 失败：`error code + message + 可选上下文（页号、对象ID、文件名）`

实现要求：
- 不抛“跨模块异常”作为主控制流。
- 仅在模块内部捕获第三方异常并转换为 `Status`。

## 2.3 同步与异步双通道

每个业务接口必须至少提供同步实现，异步通过 `TaskRunner` 包装：
- 同步：CLI 直接调用。
- 异步：GUI 通过 `Submit/Cancel/Query` 调用，接收进度回调。

---

## 3. M0 基础框架模块（Core + Runtime）

## 3.1 本模块需要完成的工作

1. 建立 `cpp_pdftools` 工程和依赖装配（CMake + presets）。
2. 定义全局基础类型：`Status`、错误码、任务状态、进度事件。
3. 建立命令注册中心和任务调度器骨架。
4. 约束日志、配置、路径处理的统一入口。

## 3.2 需要设计的核心类

1. `Status`
- 表达成功/失败/warning。
- 持有错误码、消息、上下文。

2. `ErrorCatalog`
- 集中定义错误码分类（参数错误、IO 错误、PDF 解析错误、后端错误、内部错误）。

3. `TaskHandle`
- 异步任务唯一标识。

4. `TaskState`
- 任务状态（queued/running/succeeded/failed/cancelled）与进度信息。

5. `TaskRunner`
- 管理任务队列、线程池、取消信号、进度通知。

6. `CommandRegistry`
- 管理 `operation_id -> handler` 映射。

7. `RuntimeContext`
- 在任务执行时传递共享能力：日志器、临时目录、取消令牌、配置快照。

## 3.3 需要设计的接口与功能定义

1. `ILogger`
- 功能：统一日志记录（debug/info/warn/error）。
- 实现方式：先给出控制台实现，后续可接文件与 GUI log sink。

2. `IProgressSink`
- 功能：接收进度事件（百分比、当前阶段、附加指标）。
- 实现方式：CLI 版本写 stderr/quiet；GUI 版本转发到主线程消息队列。

3. `ICommandHandler`
- 功能：处理一种具体 operation 的同步调用。
- 实现方式：每个业务模块提供 handler，注册到 `CommandRegistry`。

4. `ITaskScheduler`
- 功能：异步执行、取消和查询。
- 实现方式：基于线程池 + 可取消 token；任务内阶段性检查取消标记。

## 3.4 实现要点（自然语言）

1. `CommandRegistry` 不关心参数来源（CLI/GUI），只关心结构化 `Request`。
2. `TaskRunner` 调用 `CommandRegistry`，统一同步/异步入口，避免双实现。
3. 所有任务在关键阶段上报进度：读取输入、处理、写出、校验。
4. 把第三方库异常统一转换到 `Status`，保证上层稳定。

---

## 4. M1 `pdf2docx` 迁移模块（最高优先级）

## 4.1 本模块需要完成的工作

1. 将 `cpp_pdf2docx` 的能力平移到 `cpp_pdftools/convert/pdf2docx`。
2. 保留既有能力：文本/图片抽取、IR、pipeline 排序合并、DOCX writer。
3. 对外暴露统一接口 `ConvertPdfToDocx(Request, Result)`。
4. 保留调试能力（dump IR / only-page / anchored 模式）作为选项字段。

## 4.2 需要设计的核心类

1. `PdfToDocxRequest`
- 输入 PDF、输出 DOCX、页范围、是否导出 IR、图片策略、锚点策略、调试选项。

2. `PdfToDocxResult`
- 输出路径、页统计、文本块/图片块数量、warning 列表。

3. `PdfToDocxService`
- 对外总入口，编排“抽取 -> 分析 -> 写入”。

4. `ExtractionStage`
- 调用 backend 抽取文本与图片，构建中间表示。

5. `LayoutStage`
- 行分组、段落聚合、阅读顺序排序、标题识别与样式推断。

6. `DocxWriteStage`
- 把规范化 IR 写入 DOCX 包结构（document.xml, styles.xml, media）。

7. `AssetStore`
- 管理中间图片和临时资源，保证命名唯一与生命周期清晰。

## 4.3 需要设计的接口与功能定义

1. `IPdfContentExtractor`
- 功能：按页提取文本 run、glyph quad、图片对象、坐标与元信息。
- 实现方式：默认 PoDoFo 实现；负责把 PDF 坐标归一化到统一坐标系。

2. `ILayoutAnalyzer`
- 功能：把原子文本/图片块组织成“行/段落/标题/图文关系”。
- 实现方式：基于几何阈值 + 字号权重 + 同行重叠规则，支持可调参数。

3. `IDocxComposer`
- 功能：把布局结果写成 DOCX（含样式、段落、run、图片关系）。
- 实现方式：复用已有 minizip+tinyxml2 writer，封装成独立组件。

4. `IImageResourceResolver`
- 功能：将 PDF 图像对象解码为可写入 DOCX 的图片资源。
- 实现方式：统一处理 colorspace/尺寸异常/空图像回退，输出稳定 media 文件。

## 4.4 实现要点（自然语言）

1. 迁移策略采用“先平移后收敛”：先保持行为一致，再逐步改命名空间与目录。
2. 对 `image-text.pdf` 这类复杂样例，重点保证两点：
- 图片对象不因坐标异常或 colorspace 处理缺失而丢失。
- 标题行不被过度拆分或错误合并，保持阅读顺序稳定。
3. 对 IR 建立版本字段，便于后续升级不破坏调试工具。
4. Writer 层增加一致性检查：缺失图片资源时给 warning，不直接 silent fail。

---

## 5. M2 文档编辑模块（合并/增删改页）

## 5.1 本模块需要完成的工作

1. PDF 合并（多个输入 -> 一个输出）。
2. 页面插入、删除、替换。
3. 页范围与页索引解析（支持单页、区间、列表）。
4. 编辑后文档结构校验并保存。

## 5.2 需要设计的核心类

1. `MergePdfRequest/Result`
2. `InsertPageRequest/Result`
3. `DeletePageRequest/Result`
4. `ReplacePageRequest/Result`
5. `PageRange`
- 负责把用户表达（如 `1,3-5`）规范化为索引集合。

6. `DocumentEditorService`
- 对外统一入口，内部路由到具体操作器。

7. `PageMutationPlan`
- 表达本次页面变更序列，保证多步操作顺序明确、可回放。

## 5.3 需要设计的接口与功能定义

1. `IPdfDocumentEditor`
- 功能：打开文档、执行页面级变更、保存输出。
- 实现方式：PoDoFo 适配层封装 page collection 操作，隔离第三方 API 细节。

2. `IPageSelector`
- 功能：解析并验证页范围，处理越界与重复。
- 实现方式：先词法解析，再基于文档页数做语义校验。

3. `IDocumentValidator`
- 功能：在写出前检查关键结构（页树、对象引用、基本元信息）。
- 实现方式：轻量规则校验 + backend save 失败兜底。

## 5.4 实现要点（自然语言）

1. 所有页索引对外使用 1-based（用户友好），内部统一转 0-based。
2. 合并时保留输入顺序，并输出来源统计，便于审计。
3. 替换页实现为“删除 + 插入”的原子计划，失败可回滚到原文档快照。
4. 大文件处理采用流式写出策略，避免一次性复制全部对象造成内存峰值。

---

## 6. M3 提取模块（文本/附件/可扩展到图片）

## 6.1 本模块需要完成的工作

1. 纯文本提取（按页或整文）。
2. 可选结构化文本提取（块坐标、行号、字体信息）。
3. 嵌入附件提取（导出原始附件文件 + 元信息）。
4. 为后续“提取图片/元数据”预留统一扩展点。

## 6.2 需要设计的核心类

1. `ExtractTextRequest/Result`
- 选项含：页范围、输出格式（txt/json）、是否包含位置信息。

2. `ExtractAttachmentsRequest/Result`
- 选项含：输出目录、命名冲突策略、是否覆盖。

3. `TextSpan` / `TextLine` / `TextPage`
- 表示结构化文本结果。

4. `AttachmentItem`
- 文件名、大小、MIME、校验信息、导出路径。

5. `ExtractionService`
- 对外统一入口，内部子能力解耦。

## 6.3 需要设计的接口与功能定义

1. `ITextExtractor`
- 功能：从 PDF 内容流提取文本并按规则排序。
- 实现方式：backend 返回原始 text run，domain 层做顺序重建与规范化换行。

2. `IAttachmentExtractor`
- 功能：枚举并导出 embedded files。
- 实现方式：通过后端 name tree/embedded files 访问对象，统一导出并校验写入。

3. `IOutputFormatter`
- 功能：把提取结果写成 `txt/json` 等格式。
- 实现方式：格式器独立，避免提取逻辑和输出格式耦合。

## 6.4 实现要点（自然语言）

1. 文本提取默认 UTF-8 输出，遇到不可映射字符记录 warning。
2. 结构化输出中的坐标使用统一单位和页面坐标系，和 `pdf2docx` IR 保持一致。
3. 附件导出采用“安全文件名 + 冲突重命名”策略，避免覆盖与路径穿越风险。
4. 导出后生成清单（json）记录 hash 与来源对象 ID，方便回归验证。

---

## 7. M4 创建模块（图片转 PDF）

## 7.1 本模块需要完成的工作

1. 多图片输入生成 PDF（每图一页或多图拼版）。
2. 支持页面尺寸、方向、边距、缩放策略（fit/fill/keep）。
3. 支持图片旋转和基础质量参数。
4. 提供确定性输出（相同输入选项产出稳定）。

## 7.2 需要设计的核心类

1. `ImagesToPdfRequest/Result`
- 选项含：输入列表、排序、页面模板、边距、背景色、压缩策略。

2. `ImageItem`
- 图片路径、像素尺寸、色彩信息、旋转信息。

3. `ImageLayoutPlan`
- 描述每张图在目标页的摆放矩形与缩放模式。

4. `ImageToPdfService`
- 对外入口，编排“读图 -> 布局 -> 写 PDF”。

## 7.3 需要设计的接口与功能定义

1. `IImageDecoder`
- 功能：读取图片头信息和像素/编码数据。
- 实现方式：优先复用第三方库已有能力，抽象出统一解码结果。

2. `IPageLayoutEngine`
- 功能：根据页面模板和缩放策略计算摆放区域。
- 实现方式：纯算法层，不依赖 PDF 后端，便于单测覆盖。

3. `IPdfPageComposer`
- 功能：把图片资源放入 PDF 页面并写出。
- 实现方式：后端适配创建页面、嵌图、保存；处理不同色彩空间兼容。

## 7.4 实现要点（自然语言）

1. 布局计算和 PDF 写入分离，先算 `layout plan` 再落盘，便于调试。
2. 对超大图做尺寸预检查，防止内存峰值过高；必要时分块或降采样。
3. 多图输入支持自然排序与显式排序，结果可预期。
4. 对损坏图片给出可定位错误（第几张、哪种失败），不中断已完成页可选保留策略。

---

## 8. M5 统一 CLI 模块

## 8.1 本模块需要完成的工作

1. 形成单入口可执行文件：`pdftools`。
2. 构建统一子命令树：`merge`、`text extract`、`image2pdf`、`page ...`、`convert pdf2docx`。
3. 参数校验、错误提示、帮助信息标准化。
4. CLI 与 domain 解耦，仅做参数解析和结果展示。

## 8.2 需要设计的核心类

1. `CliApp`
- 管理命令生命周期：解析、分发、执行、返回码映射。

2. `CommandSpec`
- 描述命令参数、选项、必填规则、示例文本。

3. `OptionBinder`
- 把命令行参数绑定到对应 `Request` 字段。

4. `ExitCodeMapper`
- 将 `Status` 映射到稳定的进程退出码。

## 8.3 需要设计的接口与功能定义

1. `ICliCommand`
- 功能：定义一个子命令的参数模型和执行入口。
- 实现方式：每个命令构造对应 Request，调用 `CommandRegistry`。

2. `IHelpRenderer`
- 功能：生成统一风格的帮助输出和示例。
- 实现方式：根据 `CommandSpec` 自动生成，避免手写重复文案。

3. `IResultPrinter`
- 功能：把 `Result` 转为终端可读文本或 JSON。
- 实现方式：支持 `--json`，便于脚本化调用。

## 8.4 实现要点（自然语言）

1. CLI 不持有业务逻辑，业务全部回到 domain service。
2. 参数解析失败和执行失败用不同退出码，便于 CI 脚本判定。
3. 对路径类参数统一做预检查（存在性、可写性、覆盖策略）。
4. 先保证可用性和一致性，再优化命令别名与兼容参数。

---

## 9. M6 GUI 接入模块（任务 API）

## 9.1 本模块需要完成的工作

1. 提供 GUI 可调用的稳定任务接口（提交/取消/查询/订阅进度）。
2. 进度、日志、产物路径、错误详情可实时回传。
3. 保证 GUI 主线程不阻塞，支持多个任务并发。

## 9.2 需要设计的核心类

1. `GuiTaskFacade`
- 面向 GUI 的薄门面，屏蔽内部命令与线程细节。

2. `TaskEvent`
- 标准化事件结构（progress/log/state-change/result）。

3. `TaskEventBus`
- 负责线程安全分发事件到订阅者。

4. `CancellationTokenSource`
- 控制任务取消，保证长任务可中断。

## 9.3 需要设计的接口与功能定义

1. `ITaskFacade`
- 功能：GUI 提交任务与获取状态的统一入口。
- 实现方式：内部调用 `TaskRunner`，返回可持久化 `TaskHandle`。

2. `ITaskObserver`
- 功能：监听任务事件更新 GUI。
- 实现方式：回调或消息队列两种适配，保持 UI 框架无关。

3. `IResultStore`
- 功能：保存任务结果摘要，供 GUI 重进页面时恢复历史。
- 实现方式：轻量本地持久化（json/sqlite 任一），先满足查询与展示。

## 9.4 实现要点（自然语言）

1. 任务事件采用“阶段 + 百分比 + 文本消息”三元组，统一展示逻辑。
2. 取消不是强杀线程，而是阶段性检查 token 后安全退出。
3. 大任务结果（如输出文件列表）只存摘要，详情按需加载。
4. GUI 层只消费 `TaskEvent`，不直接调用具体 PDF 模块，降低耦合。

---

## 10. 横切模块（所有能力共享）

## 10.1 Backend 抽象层

需要类：
- `PdfBackendFactory`
- `PoDoFoBackend`
- `PdfDocumentHandle`

需要接口：
- `IPdfBackend`
  - 功能：打开/创建/保存文档，页面枚举，资源读写，文本与对象访问。
  - 实现方式：以 PoDoFo 为第一实现，domain 只依赖该抽象。

实现重点：
- 把第三方对象生命周期封装在 handle 内，避免裸指针跨层传播。

## 10.2 配置与策略层

需要类：
- `RuntimeConfig`
- `OperationPolicy`

需要接口：
- `IConfigProvider`
  - 功能：读取默认配置 + 命令覆盖配置。
  - 实现方式：优先命令参数，其次配置文件，再次编译默认值。

## 10.3 日志与可观测性

需要类：
- `LogEvent`
- `MetricsSnapshot`

需要接口：
- `IMetricsCollector`
  - 功能：记录耗时、处理页数、错误分布。
  - 实现方式：内存累积 + 任务结束输出摘要，后续可接监控系统。

## 10.4 测试与验证

需要类：
- `FixtureLocator`
- `GoldenComparator`
- `ArtifactInspector`

需要接口：
- `ITestScenario`
  - 功能：定义输入、执行命令、断言产物。
  - 实现方式：单测覆盖算法与解析，集成测试覆盖端到端产物。

实现重点：
- 回归测试必须覆盖 `build/test-image-text.pdf`、页面编辑样例、附件样例、图片转 PDF 样例。

---

## 11. 推荐落地顺序（执行视角）

1. `M0`：先打通工程与 runtime 抽象。
2. `M1`：优先迁入 `pdf2docx`，确保已有成果不丢。
3. `M2 + M3`：补齐编辑和提取核心能力。
4. `M4`：完成图片转 PDF 创建能力。
5. `M5`：统一 CLI 单入口。
6. `M6`：提供 GUI 任务接入。
7. 横切层持续收敛：backend、日志、配置、测试标准化。

---

## 12. 完成标准（模块级 DoD）

每个模块完成时都要满足：

1. 有清晰的 `Request/Result/Status` 对外接口。
2. 有对应的 CLI 命令或 GUI 任务映射。
3. 有最少一条端到端测试与一条失败场景测试。
4. 有 `prompts` 交接记录（本模块做了什么、已知风险、下一步）。
5. 不破坏已通过的回归用例（尤其 `pdf2docx` 现有样本）。
